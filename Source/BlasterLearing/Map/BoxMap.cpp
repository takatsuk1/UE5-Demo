// Fill out your copyright notice in the Description page of Project Settings.


#include "BoxMap.h"

#include "Engine/ActorChannel.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"

// Sets default values
ABoxMap::ABoxMap()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(
                TEXT("/Engine/BasicShapes/Cube.Cube")
            );
    if (CubeMeshAsset.Succeeded())
    {
        CubeMesh = CubeMeshAsset.Object;
    }
    else
    {
        CubeMesh = nullptr;
    }

}

// Called when the game starts or when spawned
void ABoxMap::BeginPlay()
{
    Super::BeginPlay();
}

UStaticMeshComponent* ABoxMap::CreateCubeComponent(const FName& Name, const FVector& Location, const FVector& Scale,
	const FName& MaterialSlotName)
{
	return CreateMeshComponent(Name, CubeMesh, Location, Scale);
}

UStaticMeshComponent* ABoxMap::CreateMeshComponent(const FName& Name, UStaticMesh* Mesh, const FVector& Location,
    const FVector& Scale, const FRotator& Rotation)
{
    if (!Mesh)
    {
        return nullptr;
    }

    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, Name);
    Comp->SetupAttachment(GetRootComponent());
    Comp->SetStaticMesh(Mesh);
    Comp->SetCollisionProfileName(TEXT("BlockAll"));
    Comp->SetGenerateOverlapEvents(false);
    Comp->SetRelativeLocation(Location);
    Comp->SetRelativeRotation(Rotation);
    Comp->SetRelativeScale3D(Scale);
    // Ensure runtime-created components are network-replicated so they can be safely replicated by the channel
    Comp->SetIsReplicated(true);

    // [Navigation Fix] Ensure the wall component affects navigation so AI can see it
    Comp->SetCanEverAffectNavigation(true);

    // Tag components so we can more easily identify them for replication/diagnostics
    FString SName = Name.ToString();
    if (SName.StartsWith(TEXT("WFCTile_")))
    {
        Comp->ComponentTags.Add(FName(TEXT("WFCTile")));
        Comp->ComponentTags.Add(FName(TEXT("WFCTile_Wall"))); 
    }
    else if (SName.StartsWith(TEXT("Floor")) || SName.StartsWith(TEXT("NorthWall")) || SName.StartsWith(TEXT("SouthWall")) || SName.StartsWith(TEXT("EastWall")) || SName.StartsWith(TEXT("WestWall")))
    {
        Comp->ComponentTags.Add(FName(TEXT("WFCTile_Wall")));
    }
    Comp->RegisterComponent();
    return Comp;
}

// Cell representation for WFC grid
struct FWFCCell
{
	TArray<int32> Possibilities; // indices into variant array
	bool bCollapsed = false;
};

struct FTileVariant
{
	int32 VariantIndex; // index in variants array
	int32 BaseTileIndex; // index in WFCTiles
	int32 EdgeN, EdgeE, EdgeS, EdgeW;
	float Weight;
	FRotator Rotation;
	UStaticMesh* Mesh;
};

void ABoxMap::GenerateWFCMap()
{
    // ==============================
    // WFC(波函数坍缩) 地图生成（墙体）
    // 目标：
    // 1) 从 DataTable 读取 tile 定义（网格块的边缘连接ID、权重、Mesh 等）
    // 2) 生成 tile 的旋转变体（0/90/180/270）并去重
    // 3) 根据连接规则构建“边缘是否可连接”的判定与“变体邻接兼容表”
    // 4) 在网格上运行 WFC：选择最小熵格子 -> 按权重挑选 -> 约束传播
    // 5) 将结果实例化为运行时 UStaticMeshComponent
    // 注意：本函数只生成 WFCTile_* 组件；Floor/四面墙在 GenerateBoxMap() 里生成。
    // ==============================

    // Compact WFC implementation: read tiles/connect rules, build rotated variants, compute compat, run WFC, basic repair, instantiate
    // （原注释）紧凑版 WFC：读取 tile/连接规则 -> 构建旋转变体 -> 计算兼容 -> 跑 WFC -> 基础修复 -> 实例化

    TArray<FWFCTile> TileDefs;                 // 读取到的“基础 tile 定义”（未旋转）
    if (!TileDataTable)                        // 没有配置 TileDataTable 直接返回
    {
        return;
    }
    if (TileDataTable->GetRowStruct() != FWFCTileRow::StaticStruct()) // DataTable 行结构不匹配就不能安全读取
    {
        return;
    }

         // 从 DataTable 把所有 tile 行读出来
        static const FString TileCtx(TEXT("WFC Tile Rows"));
        TArray<FWFCTileRow*> TileRows; TileDataTable->GetAllRows(TileCtx, TileRows);
        for (FWFCTileRow* R : TileRows)
        {
            if (R) {
                FWFCTile T;
                T.TileName  = R->TileName;
                T.Mesh      = R->Mesh;
                T.EdgeNorth = R->EdgeNorth;
                T.EdgeEast  = R->EdgeEast;
                T.EdgeSouth = R->EdgeSouth;
                T.EdgeWest  = R->EdgeWest;
                T.Weight    = R->Weight;

                if (T.Weight > 0.0f)
                {
                    TileDefs.Add(T);
                }
            }
        }
    if (TileDefs.Num() == 0) return;

    // ------------------------------
    // 读取“连接规则”：某些边缘ID之间允许互相连接
    // 例如：ID=1 可以与 ID=2 连接（并且这里会做成双向）
    // ------------------------------
    TArray<FConnectPair> ConnectsLocal;        // 本地缓存连接规则（避免直接依赖 DataTable 行）
    if (ConnectDataTable && ConnectDataTable->GetRowStruct() == FConnectRow::StaticStruct())
    {
        static const FString Ctx(TEXT("WFC Connect Rows"));
        TArray<FConnectRow*> Rows; ConnectDataTable->GetAllRows(Ctx, Rows);
        ConnectsLocal.Empty();
        for (FConnectRow* R : Rows)
        {
            if (R) {
                FConnectPair P;
                P.ID = R->ID;
                P.CanConnectID = R->CanConnectID;
                ConnectsLocal.Add(P);
            }
        }
    }

    // ------------------------------
    // 构建“旋转变体 Variants”
    // 每个基础 tile 最多产生 4 个变体（旋转 0/90/180/270）
    // 变体用 EdgeN/E/S/W 描述旋转后边缘ID，用 Rotation 记录旋转角
    // 并用 SigSeen 做去重：若旋转后边缘组合相同，则认为是等价变体（例如四向对称的 tile）
    // ------------------------------

    // Build rotated variants (avoid duplicates)
    TArray<FTileVariant> Variants;
    Variants.Reserve(TileDefs.Num()*4);    // 预留容量：tile*4 + 1(后面加 EmptyVar)
    TSet<uint64> SigSeen;                     // 记录已出现的“签名”用于去重（baseIndex + edges）

    // todo 边ID超过255会导致重复
    // 这里把 baseIdx 与四个边缘ID 压到一个 64bit 里：
    // 高 32 位存 baseIdx，低 32 位按字节存 N/E/S/W（因此边ID若 >255 会发生截断/冲突）
    auto PackSig = [](int baseIdx, int N, int E, int S, int W)->uint64
    {
        uint64 s = ((uint64)(uint32)baseIdx<<32) | ((uint32)N<<24) | ((uint32)E<<16) | ((uint32)S<<8) | (uint32)W;
        return s;
    };

    // 遍历每个基础 tile，生成其旋转变体
    for (int i=0;i<TileDefs.Num();++i)
    {
        const FWFCTile& T = TileDefs[i];       // 取出基础 tile
        int N=T.EdgeNorth,E=T.EdgeEast,S=T.EdgeSouth,W=T.EdgeWest; // 当前旋转状态下的四边缘
        for (int r=0;r<4;++r)
        {
            uint64 sig = PackSig(i,N,E,S,W);   // 当前旋转状态的签名
            if (!SigSeen.Contains(sig))
            {
                  FTileVariant V;
                V.BaseTileIndex = i;           // 指向基础 tile 的索引
                V.EdgeN = N;
                V.EdgeE = E;
                V.EdgeS = S;
                V.EdgeW = W;
                V.Weight = T.Weight;           // 变体权重沿用基础 tile
                V.Rotation = FRotator(0.0f, r*90.0f, 0.0f); // 绕 Z(Yaw) 旋转
                V.Mesh = T.Mesh;
                V.VariantIndex = Variants.Num(); // 记录在 Variants 中的索引（Add 前的 Num）
                Variants.Add(V);
                SigSeen.Add(sig);
            }

            // rotate edges 90deg clockwise
            // 旋转边缘ID：顺时针 90° 时
            // oldW -> newN, oldN -> newE, oldE -> newS, oldS -> newW
            int oldN=N, oldE=E, oldS=S, oldW=W;
            N = oldW;
            E = oldN;
            S = oldE;
            W = oldS;
        }
    }
    const int32 NumVariants = Variants.Num();
    if (NumVariants == 0) return;

    // ------------------------------
    // 把连接规则做成快速查表：AllowedPairs
    // 以 64bit key = (a<<32)|b 表示 a 可以与 b 相连
    // 并加入双向 (a,b) 与 (b,a)
    // ------------------------------

    // Build allowed pair set
    TSet<uint64> AllowedPairs;
    for (const FConnectPair& P : ConnectsLocal)
    {
        uint64 k1 = (uint64)((uint32)P.ID)<<32 | (uint32)P.CanConnectID;
        uint64 k2 = (uint64)((uint32)P.CanConnectID)<<32 | (uint32)P.ID;
        AllowedPairs.Add(k1);
        AllowedPairs.Add(k2);
    }

    // 判断两个边缘ID是否兼容：
    // - 相同ID：直接兼容
    // - 不同ID：查 AllowedPairs（由 ConnectDataTable 提供）
    auto EdgeCompatible = [&](int a,int b)->bool
    {
        if (a==b) return true;
        uint64 key = (uint64)((uint32)a)<<32 | (uint32)b;
        return AllowedPairs.Contains(key);
    };
    

    // ------------------------------
    // 构建“邻接兼容表” Compat
    // Compat[dir, a] = 所有与变体 a 在 dir 方向上兼容的邻居变体索引集合
    // dir 约定：0=N,1=E,2=S,3=W（与下面使用保持一致）
    // 兼容性规则：
    // - a 的北边 与 b 的南边 兼容 -> b 可放在 a 的北侧
    // - a 的东边 与 b 的西边 兼容 -> b 可放在 a 的东侧
    // - a 的南边 与 b 的北边 兼容 -> b 可放在 a 的南侧
    // - a 的西边 与 b 的东边 兼容 -> b 可放在 a 的西侧
    // ------------------------------

    // Compatibility lists: for each variant and dir (0=N,1=E,2=S,3=W) list neighbor variants allowed
    TArray<TArray<int32>> Compat;
    Compat.SetNum(4 * NumVariants);            // 4 个方向 * 变体数
    auto CompatIndex = [&](int dir,int var)
    {
        return dir*NumVariants + var;
    };

    // 双层遍历所有变体对 (a,b)，预计算它们在四个方向是否兼容
    for (int a=0;a<NumVariants;++a)
    {
        const FTileVariant& A = Variants[a];
        for (int b=0;b<NumVariants;++b)
        {
            const FTileVariant& B = Variants[b];

            // 检查四个方向的兼容性并记录（log 强度很高）
            if (EdgeCompatible(A.EdgeN, B.EdgeS))
            {
                Compat[CompatIndex(0,a)].Add(b);
            }
            if (EdgeCompatible(A.EdgeE, B.EdgeW))
            {
                Compat[CompatIndex(1,a)].Add(b);
            }
            if (EdgeCompatible(A.EdgeS, B.EdgeN))
            {
                Compat[CompatIndex(2,a)].Add(b);
            }
            if (EdgeCompatible(A.EdgeW, B.EdgeE))
            {
                Compat[CompatIndex(3,a)].Add(b);
            }
        }
    }
   // ------------------------------
    // 按权重从 Possibilities 中随机挑选一个变体
    // - Poss: 当前格子可选的变体索引列表
    // - Variants[i].Weight: 该变体权重
    // 返回挑中的变体索引；若 Poss 为空返回 INDEX_NONE
    // ------------------------------

    // weighted pick
    auto WeightedPick = [&](FRandomStream& Rnd, const TArray<int32>& Poss)->int32 {
        if (Poss.Num()==0)
            return INDEX_NONE;
        float sum=0;
        for (int i:Poss)
            sum += Variants[i].Weight;         // 计算总权重
        float v = Rnd.FRandRange(0.0f,sum);    // [0,sum] 之间取随机数
        float acc=0;
        for (int i:Poss)
        {
            acc+=Variants[i].Weight;
            if (v<=acc) return i;              // 落入哪个区间就选哪个
        }
        return Poss.Last();                    // 理论上不会走到这，但防浮点误差兜底
    };

    // ------------------------------
    // 计算网格规模：用 Xlength/Ylength 除以 WFCTileSize 得到格子数
    // WFCTileSize 是“WFC tile 的理想尺寸”，但最终实例化时会用 TileSizeX/Y 做自适应缩放
    // ------------------------------

    // grid size
    const float WFCTileSize = 500.0f;
    int32 EffectiveGridX = FMath::Max(1, FMath::RoundToInt(Xlength / WFCTileSize));
    int32 EffectiveGridY = FMath::Max(1, FMath::RoundToInt(Ylength / WFCTileSize));
    
     if (EffectiveGridX<=0||EffectiveGridY<=0) return;
     const int Tot = EffectiveGridX * EffectiveGridY; // 总格子数

    // 初始化随机流：用全局 Rand() 再喂给 FRandomStream，保证每次生成有随机性
    FRandomStream Rand; Rand.Initialize(FMath::Rand());

    // ------------------------------
    // 尝试生成一次：
    // - 初始化每格 Possibilities=全部变体
    // - 不断选择“可能性最少”的格子坍缩，并传播约束
    // - 若矛盾（某格 Possibilities 变空）则失败
    // - 若成功则做连通性修复与实例化
    // ------------------------------

    auto TryOnce = [&](FRandomStream& LocalRand)->bool
    {
        // Grid：每个格子存 Possibilities（可选变体索引）与是否已坍缩
        TArray<FWFCCell> Grid;
        Grid.SetNum(Tot);
        for (int i=0;i<Tot;++i)
        {
            Grid[i].Possibilities.Empty();
            Grid[i].Possibilities.Reserve(NumVariants);
            for (int v=0;v<NumVariants;++v)
            {
                Grid[i].Possibilities.Add(v);  // 初始：每格都可选任何变体
            }
            Grid[i].bCollapsed=false;          // 初始：都未坍缩
        }

        // 二维坐标转一维索引：Index2(x,y)=y*X+x
        auto Index2 = [&](int X,int Y)
        {
            return Y*EffectiveGridX + X;
        };

        // propagate using queue
        // 约束传播：从一个起点格子 (sx,sy) 开始，把它的可能性变化推给邻居
        // 若传播导致任何格子 possibilities==0 则返回失败
        auto Propagate = [&](int sx,int sy)->bool {
            TArray<FIntPoint> Stack;           // 用栈/队列都行，这里用 TArray 当栈
            Stack.Add(FIntPoint(sx,sy));
            while (Stack.Num()) {
                FIntPoint P = Stack.Pop();    // Pop：取最后一个
                int CX=P.X, CY=P.Y;
                if (CX<0||CY<0||CX>=EffectiveGridX||CY>=EffectiveGridY)
                {
                    continue;                  // 越界则忽略
                }
                int CIdx=Index2(CX,CY);
                FWFCCell& Cell=Grid[CIdx];
                if (Cell.Possibilities.Num()==0)
                {
                    return false;              // 当前格子无解 -> 矛盾
                }

                // 遍历四个方向，把当前格子的可能性约束传给邻居
                for (int Dir=0;Dir<4;++Dir) {
                    // 方向到坐标偏移：
                    // Dir==1 表示向东 x+1; Dir==3 表示向西 x-1
                    // Dir==0 表示向北 y+1; Dir==2 表示向南 y-1（注意这里 y 正方向被当作“北”）
                    int NX = CX + (Dir==1?1:(Dir==3?-1:0));
                    int NY = CY + (Dir==0?1:(Dir==2?-1:0));
                    if (NX<0||NY<0||NX>=EffectiveGridX||NY>=EffectiveGridY)
                    {
                        continue;              // 邻居越界则跳过
                    }
                    int NIdx = Index2(NX,NY);
                    FWFCCell& NCell = Grid[NIdx];

                    // allowed：在该方向上，邻居允许出现的所有变体集合
                    // 计算方式：
                    // 对当前格子每个可能 cposs，查 Compat[Dir, cposs] 把所有允许的邻居变体并起来
                    TSet<int32> allowed;
                    for (int cposs : Cell.Possibilities)
                    {
                        for (int v : Compat[CompatIndex(Dir,cposs)])
                        {
                            allowed.Add(v);
                        }
                    }

                    // 过滤邻居的 possibilities，只保留 allowed 里的
                    TArray<int32> New;
                    New.Reserve(NCell.Possibilities.Num());
                    for (int p : NCell.Possibilities)
                    {
                        if (allowed.Contains(p))
                        {
                            New.Add(p);
                        }
                    }

                    // 如果邻居 possibilities 发生变化，则把邻居加入 Stack 继续传播
                    if (New.Num() != NCell.Possibilities.Num())
                    {
                        NCell.Possibilities = MoveTemp(New);
                        Stack.Add(FIntPoint(NX,NY));
                        if (NCell.Possibilities.Num()==0)
                        {
                            return false;      // 邻居被过滤为空 -> 矛盾
                        }
                    }
                }
            }
            return true;                       // 无矛盾传播完成
        };

        // IterLimit：防止死循环的迭代上限（经验值：格子数 * 8）
        int IterLimit = Tot * 8; int Iter=0;
        while (true)
        {
            // 判断是否全部坍缩完成
            bool allCollapsed=true;
            for (int i=0;i<Tot;++i)
            {
                if (!Grid[i].bCollapsed)
                {
                    allCollapsed=false; break;
                }
            }
            if (allCollapsed)
            {
                break;                         // 全部坍缩 -> 求解结束
            }

            // 选择“熵最小”的格子：Possibilities 数量最少但 >1
            int bestIdx=-1, bestCount=INT_MAX;
            for (int y=0;y<EffectiveGridY;++y)
            {
                for (int x=0;x<EffectiveGridX;++x) {
                    int idx=Index2(x,y);
                    FWFCCell& C=Grid[idx];
                    if (C.bCollapsed) continue;
                    int cnt=C.Possibilities.Num();
                    if (cnt==0) return false;  // 有格子无解 -> 失败
                    if (cnt==1)
                    {
                        C.bCollapsed=true;     // 已经只剩 1 种可能，直接视为坍缩
                        continue;
                    }
                    if (cnt < bestCount)
                    {
                        bestCount = cnt; bestIdx = idx;
                    }
                }
            }
            if (bestIdx==-1)
            {
                break;                         // 找不到需要坍缩的格子，退出循环
            }

            // 对 bestIdx 格子进行坍缩：按权重随机选一个变体
            FWFCCell& Chosen = Grid[bestIdx];
            int pick = WeightedPick(LocalRand, Chosen.Possibilities);
            if (pick==INDEX_NONE)
            {
                return false;                  // 没有可选项
            }
            Chosen.Possibilities.SetNum(1);    // 只保留挑中的那个
            Chosen.Possibilities[0]=pick;
            Chosen.bCollapsed=true;

            // 把该选择传播给邻居
            int CX = bestIdx % EffectiveGridX, CY = bestIdx / EffectiveGridX;
            if (!Propagate(CX,CY))
            {
                return false;                  // 传播出现矛盾 -> 本次尝试失败
            }

            if (++Iter > IterLimit)
            {
                break;                         // 超过迭代上限就提前终止（可能得到半随机解）
            }
        }

        // 最后再检查一次：任何格子 possibilities 为空都视为失败
        for (int i=0;i<Tot;++i)
        {
            if (Grid[i].Possibilities.Num()==0){
                return false;
        }
          }

        // ------------------------------
        // 实例化阶段：把 Grid 的最终结果生成 UStaticMeshComponent
        // ------------------------------

        int InstGridX = EffectiveGridX, InstGridY = EffectiveGridY;

        float TileSizeX = (InstGridX>0) ? (Xlength / (float)InstGridX) : WFCTileSize;
        float TileSizeY = (InstGridY>0) ? (Ylength / (float)InstGridY) : WFCTileSize;

        float HalfWidthLocal = Xlength*0.5f, HalfHeightLocal = Ylength*0.5f;
        float startX = -HalfWidthLocal + TileSizeX*0.5f;
        float startY = -HalfHeightLocal + TileSizeY*0.5f;

        auto ComputeTileZLocal = [&]()->float
        {
            float TileZLocal = 50.0f;
            if (LastGeneratedFloor && LastGeneratedFloor->IsRegistered())
            {
                const FBoxSphereBounds FB = LastGeneratedFloor->Bounds;
                float FloorTopWorldZ = FB.Origin.Z + FB.BoxExtent.Z;
                TileZLocal = FloorTopWorldZ - GetActorLocation().Z;
            }
            return TileZLocal;
        };

        const float TileZLocal = ComputeTileZLocal();

        auto IsRot90 = [&](const FRotator& Rot)->bool
        {
            int32 yaw = FMath::RoundToInt(Rot.Yaw) % 360;
            if (yaw < 0) yaw += 360;
            return (yaw == 90 || yaw == 270);
        };

        auto ComputeTileTransform = [&](
            UStaticMesh* Mesh,
            const FTileVariant& V,
            int32 GridX,
            int32 GridY,
            FVector& OutLoc,
            FVector& OutScale
        )->bool
        {
            if (!Mesh)
            {
                return false;
            }

            const float CenterX = startX + GridX * TileSizeX;
            const float CenterY = startY + GridY * TileSizeY;

            const FBoxSphereBounds MB = Mesh->GetBounds();

            const bool bRot90 = IsRot90(V.Rotation);
            (void)bRot90;

            const float ScaleX = 1.0f;
            const float ScaleY = 1.0f;
            const float ScaleZ = 1.0f;

            const FVector OriginScaled(MB.Origin.X * ScaleX, MB.Origin.Y * ScaleY, MB.Origin.Z * ScaleZ);
            const FVector RotOrigin = V.Rotation.RotateVector(OriginScaled);

            const float BottomLocalScaled = (MB.Origin.Z - MB.BoxExtent.Z) * ScaleZ;
            const float LocZ = TileZLocal - BottomLocalScaled;

            const float LocX = CenterX - RotOrigin.X;
            const float LocY = CenterY - RotOrigin.Y;

            OutLoc = FVector(LocX, LocY, LocZ);
            OutScale = FVector(ScaleX, ScaleY, ScaleZ);
            return true;
        };

        // 主循环：逐格读取最终变体，过滤后实例化
        for (int y = 0; y < InstGridY; ++y)
        {
            for (int x = 0; x < InstGridX; ++x)
            {
                int idx = Index2(x, y);
                FWFCCell& C = Grid[idx];

                // 取最终选定的变体索引（若意外为空则用 0 兜底）
                int varIdx = (C.Possibilities.Num() > 0) ? C.Possibilities[0] : 0;
                if (!Variants.IsValidIndex(varIdx))
                {
                    continue;
                }

                const FTileVariant& V = Variants[varIdx];

                // 无 Mesh 的变体不实例化
                UStaticMesh* Mesh = V.Mesh;
                if (!Mesh)
                {
                    continue;
                }

                // 计算 transform
                FVector Loc, Scale;
                if (!ComputeTileTransform(Mesh, V, x, y, Loc, Scale))
                {
                    continue;
                }


                // 创建组件：名称 WFCTile_x_y
                CreateMeshComponent(
                    FName(*FString::Printf(TEXT("WFCTile_%d_%d"), x, y)),
                    Mesh,
                    Loc,
                    Scale,
                    V.Rotation
                );
            }
        }

        return true; // 本次尝试成功
    };

    // ------------------------------
    // 多次尝试：WFC 是随机+约束的，可能出现矛盾导致失败
    // 这里最多尝试 8 次，只要一次成功就 return
    // ------------------------------

    const int32 WFCMaxAttempts = 8;

    for (int attempt=0; attempt < WFCMaxAttempts; ++attempt)
    {
        if (TryOnce(Rand)) { return; }         // 成功则退出 GenerateWFCMap()
        Rand.Initialize(FMath::Rand());        // 失败则换一个随机种子重试
    }
}

void ABoxMap::ClearGeneratedObjects()
{
    // 清除之前生成的静态网格组件和旧的 Floor/Wall/WFCTile
    TArray<UStaticMeshComponent*> Components;
    GetComponents<UStaticMeshComponent>(Components);

    for (UStaticMeshComponent* SComp : Components)
    {
        if (!SComp) continue;

        const FString CompName = SComp->GetName();
        const bool bShouldRemove =
            CompName.StartsWith(TEXT("Floor")) ||
            CompName.StartsWith(TEXT("NorthWall")) ||
            CompName.StartsWith(TEXT("SouthWall")) ||
            CompName.StartsWith(TEXT("EastWall")) ||
            CompName.StartsWith(TEXT("WestWall")) ||
            CompName.StartsWith(TEXT("WFCTile_"));

        if (!bShouldRemove) continue;

        // 避免重复销毁
        if (SComp->IsBeingDestroyed() || SComp->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        {
            continue;
        }

        // 如果我们删除了之前保存的 Floor，则清空指针以避免悬空引用
        if (SComp == LastGeneratedFloor)
        {
            LastGeneratedFloor = nullptr;
        }

        // 先尽量解除各系统引用（TypedElement/Nav/Physics/Collision 等）
        SComp->SetGenerateOverlapEvents(false);
        SComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SComp->SetSimulatePhysics(false);
        SComp->SetCanEverAffectNavigation(false);

        // 先从世界里反注册，再销毁（降低“外部仍引用”的概率）
        if (SComp->IsRegistered())
        {
            SComp->UnregisterComponent();
        }

        SComp->DestroyComponent();
    }
}

// Called every frame
void ABoxMap::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABoxMap::GenerateBoxMap()
{
   // 清理历史生成的对象（随机物体）以避免重复
   	ClearGeneratedObjects();
   
   	const float HalfWidth = Xlength/ 2.0f;
   	const float HalfHeight = Ylength / 2.0f;
   	const float FloorZ = 0.0f;
   	const float WallZ = WallHeight / 2.0f;
   
   	// 1. 地板（厚度 50 单位）
   	LastGeneratedFloor = CreateCubeComponent(
   		TEXT("Floor"),
   		FVector(0, 0, FloorZ), 
   		FVector(Xlength / 100.0f, Ylength / 100.0f, 0.5f) 
   	);
   
   	// 2. 北墙（Y+）
   	CreateCubeComponent(
   		TEXT("NorthWall"),
   		FVector(0, HalfHeight, WallZ),
   		FVector(Xlength / 100.0f, WallThickness / 100.0f, WallHeight / 100.0f)
   	);
   
   	// 3. 南墙（Y-）
   	CreateCubeComponent(
   		TEXT("SouthWall"),
   		FVector(0, -HalfHeight, WallZ),
   		FVector(Xlength / 100.0f, WallThickness / 100.0f, WallHeight / 100.0f)
   	);
   
   	// 4. 东墙（X+）
   	CreateCubeComponent(
   		TEXT("EastWall"),
   		FVector(HalfWidth, 0, WallZ),
   		FVector(WallThickness / 100.0f, Ylength / 100.0f, WallHeight / 100.0f)
   	);
   
   	// 5. 西墙（X-）
   	CreateCubeComponent(
   		TEXT("WestWall"),
   		FVector(-HalfWidth, 0, WallZ),
   		FVector(WallThickness / 100.0f, Ylength / 100.0f, WallHeight / 100.0f)
   	);
   
		GenerateWFCMap();

    if (HasAuthority())
    {
        ForceNetUpdate();
    }

}

bool ABoxMap::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
    bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

    // Replicate the floor and walls and any WFCTile_* components that were created at runtime
    TArray<UStaticMeshComponent*> Comps;
    GetComponents<UStaticMeshComponent>(Comps);
    for (UStaticMeshComponent* Comp : Comps)
    {
        if (!Comp) continue;
        // Only consider components that are owned by this actor
        if (Comp->GetOwner() != this) continue;
        // Must be registered in the world to be networked
        if (!Comp->IsRegistered()) continue;
        // Only replicate components explicitly set to replicate
        if (!Comp->GetIsReplicated()) continue;

        FString Name = Comp->GetName();
        bool bIsTileByName = Name.StartsWith(TEXT("WFCTile_"));
        bool bIsTileByTag = Comp->ComponentHasTag(FName(TEXT("WFCTile"))) || Comp->ComponentHasTag(FName(TEXT("WFCTile_Walkable"))) || Comp->ComponentHasTag(FName(TEXT("WFCTile_Wall")));
        bool bIsWallOrFloor = Name.StartsWith(TEXT("Floor")) || Name.StartsWith(TEXT("NorthWall")) || Name.StartsWith(TEXT("SouthWall")) || Name.StartsWith(TEXT("EastWall")) || Name.StartsWith(TEXT("WestWall"));
        if (bIsWallOrFloor || bIsTileByName || bIsTileByTag)
        {
            // Only replicate components that are supported for networking to avoid engine assertion
            if (Comp->IsSupportedForNetworking())
            {
                WroteSomething |= Channel->ReplicateSubobject(Comp, *Bunch, *RepFlags);
            }
            else
            {
                // If component isn't supported for networking, skip logging to reduce spam
                static TSet<FString> ReportedNames;
                if (!ReportedNames.Contains(Name))
                {
                    ReportedNames.Add(Name);
                }
            }
        }
    }

    return WroteSomething;
}
