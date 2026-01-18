// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterGameMode.h"

#include "BlasterLearing/Character/BlasterCharacter.h"
#include "BlasterLearing/PlayerController/BlasterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "BlasterLearing/PlayerState/BlasterPlayerState.h"
#include "BlasterLearing/GameState/BlasterGameState.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "BlasterLearing/Map/BoxMap.h"
#include "BlasterLearing/Weapon/Weapon.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"


ABlasterGameMode::ABlasterGameMode()
{
	// 延迟开始：使用热身阶段（Warmup），在玩家加入后再正式开始比赛
	bDelayedStart = true;
}

void ABlasterGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 记录关卡开始的世界时间，后续基于此值计算各阶段的剩余时间/倒计时
    LevelStartingTime = GetWorld()->GetTimeSeconds();

    // 尝试在场景中找到 ABoxMap Actor（优先使用项目现有的生成器）
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(this, ABoxMap::StaticClass(), Found);
    if (Found.Num() > 0)
    {
        BoxMapActor = Cast<ABoxMap>(Found[0]);
    }
}

// New: called once when the match actually starts (after delay/warmup)
void ABlasterGameMode::StartMatchInitialization()
{
     if (!HasAuthority()) return; // server-only
    if (bMatchStartInitialized) return;
    bMatchStartInitialized = true;

    // If a BoxMapActor is present, generate or ensure it's generated; otherwise SpawnMapEntitiesDelayed/EnsureAndSpawnMapEntities will handle it
    if (BoxMapActor)
    {
        BoxMapActor->GenerateBoxMap();
        BoxMapActor->ForceNetUpdate();
    }

    // Generate map entities immediately when match starts
    SpawnWeaponsOnMap();
    SpawnPlayersOnMap();
}

void ABlasterGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (MatchState == MatchState::WaitingToStart)
    {
        // If levels/clients aren't ready, keep resetting start time so countdown doesn't advance
        if (!bLevelReady)
        {
            LevelStartingTime = GetWorld()->GetTimeSeconds();
        }

        CountdownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
             if (CountdownTime <= 0.f)
        {
            StartMatch();
        }
    }
    else if (MatchState == MatchState::InProgress)
    {
        // If using delayed start, ensure initialization runs once when match becomes InProgress
        if (bDelayedStart && !bMatchStartInitialized)
        {
            StartMatchInitialization();
        }

        CountdownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
        if (CountdownTime <= 0.f)
        {
            RestartGame();
        }
    }
}

void ABlasterGameMode::PlayerEliminated(ABlasterCharacter* ElimmedCharacter, ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController)
{
    ABlasterPlayerState* AttackerPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
    ABlasterPlayerState* VictimPlayerState = VictimController ? Cast<ABlasterPlayerState>(VictimController->PlayerState) : nullptr;

    ABlasterGameState* BlasterGameState = GetGameState<ABlasterGameState>();

    if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && BlasterGameState)
    {
        TArray<ABlasterPlayerState*> PlayersCurrentlyInTheLead;
        for (auto LeadPlayer : BlasterGameState->TopScoringPlayers)
        {
            PlayersCurrentlyInTheLead.Add(LeadPlayer);
        }
        AttackerPlayerState->AddToScore(1.f);
        BlasterGameState->UpdateTopScore(AttackerPlayerState);
        if (BlasterGameState->TopScoringPlayers.Contains(AttackerPlayerState))
        {
            ABlasterCharacter* Leader = Cast<ABlasterCharacter>(AttackerPlayerState->GetPawn());
            if (Leader)
            {
                Leader->MulticastGainedTheLead();
            }
        }

        for (INT32 i = 0; i < PlayersCurrentlyInTheLead.Num(); i++)
        {
            if (!BlasterGameState->TopScoringPlayers.Contains(PlayersCurrentlyInTheLead[i]))
            {
                ABlasterCharacter* Loser = Cast<ABlasterCharacter>(PlayersCurrentlyInTheLead[i]->GetPawn());
                if (Loser)
                {
                    Loser->MulticastLostTheLead();
                }
            }
        }
    }
    if (VictimPlayerState)
    {
        VictimPlayerState->AddToDefeats(1);
    }

    if (ElimmedCharacter)
    {
        ElimmedCharacter->Elim(false);
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ABlasterPlayerController* BlasterPlayer = Cast<ABlasterPlayerController>(*It);
        if (BlasterPlayer && AttackerPlayerState && VictimPlayerState)
        {
            BlasterPlayer->BroadcastElim(AttackerPlayerState, VictimPlayerState);
        }
    }

    // After handling elimination, if we're server-side check remaining alive players
    if (HasAuthority())
    {
        int32 AliveCount = 0;
        TArray<AActor*> Pawns; UGameplayStatics::GetAllActorsOfClass(this, APawn::StaticClass(), Pawns);
        for (AActor* P : Pawns)
        {
            APawn* Pawn = Cast<APawn>(P);
            if (!Pawn) continue;
            if (!Pawn->IsPlayerControlled()) continue; // only consider player pawns
            ABlasterCharacter* Char = Cast<ABlasterCharacter>(Pawn);
            if (Char)
            {
                if (!Char->IsElimed()) ++AliveCount;
            }
            else
            {
                ++AliveCount;
            }
        }

        if (AliveCount <= 1)
        {
            RegenerateMapAndRespawnQuick();
        }
    }
}

void ABlasterGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
    if (ElimmedCharacter)
    {
        ElimmedCharacter->Reset();
        ElimmedCharacter->Destroy();
    }
    if (ElimmedController)
    {
        TArray<AActor*> PlayerStarts;
        UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
        if (PlayerStarts.Num() > 0)
        {
            int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
            RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
        }
        
    }
}

void ABlasterGameMode::OnMatchStateSet()
{
    Super::OnMatchStateSet();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ABlasterPlayerController* BlasterPlayer = Cast<ABlasterPlayerController>(*It);
        if (BlasterPlayer)
        {
            BlasterPlayer->OnMatchStateSet(MatchState);
        }
    }
}

void ABlasterGameMode::PlayerLeftGame(ABlasterPlayerState* PlayerLeaving)
{
    //调用Elim函数，传递参数bLeftGame为真
    if (PlayerLeaving == nullptr) return;
    ABlasterGameState* BlasterGameState = GetGameState<ABlasterGameState>();
    if (BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(PlayerLeaving))
    {
        BlasterGameState->TopScoringPlayers.Remove(PlayerLeaving);
    }
    ABlasterCharacter* CharacterLeaving = Cast<ABlasterCharacter>(PlayerLeaving->GetPawn());
    if (CharacterLeaving)
    {
        CharacterLeaving->Elim(true);
    }
}

void ABlasterGameMode::NotifyClientReady(APlayerController* PC)
{
    if (!PC)
    {
        UE_LOG(LogTemp, Log, TEXT("PC is null"));
        return;
    }

    // 统计并打印 ExpectedPlayerControllers
    int32 ExpectedLive = 0;
    {
        int32 Index = 0;
        for (auto& E : ExpectedPlayerControllers)
        {
            if (E.IsValid())
            {
                APlayerController* PCtr = E.Get();
                ++ExpectedLive;
                const FString ControllerName = PCtr->GetName();
                const FString PlayerStateName = PCtr->PlayerState ? PCtr->PlayerState->GetPlayerName() : FString(TEXT("NoPlayerState"));
                UE_LOG(LogTemp, Log, TEXT("Expected[%d] Controller=%s PlayerState=%s"), Index, *ControllerName, *PlayerStateName);
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("Expected[%d] : Invalid"), Index);
            }
            ++Index;
        }
    }
    
    if (ExpectedLive > 0)
    {
        bool bExpected = false;
        for (auto& E : ExpectedPlayerControllers)
        {
            UE_LOG(LogTemp, Log, TEXT("NotifyClientReady %s"), *E->GetName());
            if (E.IsValid() && E.Get() == PC)
            {
                bExpected = true;
                break;
            }
        }
        if (!bExpected)
        {
            UE_LOG(LogTemp, Log, TEXT("PC not expected"));
            return;
        }

        ReadyPlayerControllers.Add(TWeakObjectPtr<APlayerController>(PC));
        
        int32 ReadyLive = 0;
        for (auto& R : ReadyPlayerControllers) if (R.IsValid()) ++ReadyLive;

        UE_LOG(LogTemp, Log, TEXT("NotifyClientReady: %d / %d"), ReadyLive, ExpectedLive);
        if (ReadyLive >= ExpectedLive && ExpectedLive > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("NotifyClientReady: Player Controller Expected"));
            bLevelReady = true;
        }
    } else
    {
        bLevelReady = true;
    }
}


// Helper: check if point is free by doing a sphere overlap against blocking collision channel
static bool IsPointFree(UWorld* World, const FVector& P, float Radius)
{
    // 约定：
    // - World：必须来自当前运行时关卡世界（Server 的 World）
    // - P：候选点的世界坐标
    // - Radius：用于近似“占位体积”的半径（玩家/武器/AI 的大致碰撞半径）
    // 返回：true 表示该点附近没有与 WorldStatic 发生阻挡重叠，可以认为“可放置”。

    if (!World) return false;

    // 用球形碰撞体做一次“重叠测试（Overlap）”，比 Sweep / PathFinding 更轻量。
    // 注意：这里只测 WorldStatic（环境静态物体），不测 Pawn / PhysicsBody 等。
    FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

    // QueryParams：这里禁用复杂追踪等，仅用于简单 overlap。
    FCollisionQueryParams Params(SCENE_QUERY_STAT(IsPointFree), false);

    // OverlapAnyTestByChannel：只要有任意一个阻挡重叠就返回 true。
    // 这里用 ECC_WorldStatic 表示“墙/地板/静态网格体”等阻挡。
    const bool bAnyBlockingOverlap = World->OverlapAnyTestByChannel(
        P,
        FQuat::Identity,
        ECC_WorldStatic,
        Shape,
        Params
    );

    // “没有任何阻挡重叠” 才算 Free。
    return !bAnyBlockingOverlap;
}

// Find a random free point inside the BoxMap Actor bounds (avoid walls by margin). Returns true if found.
static bool FindRandomFreePointInBox(
    ABoxMap* BoxMapActor,
    UWorld* World,
    FVector& OutLocation,
    float SearchRadius,
    int32 MaxAttempts = 512,
    float Margin = 100.f)
{

    if (!BoxMapActor || !World) return false;

    // 1) 计算 Box 的可用范围（世界空间）
    const FVector Center = BoxMapActor->GetActorLocation();

    // BoxMap 生成的地板 Cube 位于 Actor 中心 (Relative=0)，Scale.Z=0.5 -> 高度 50，顶面在 +25。
    // 设定生成高度 Z：地板顶面 + 对象半径 + 安全间隙。这样球体正好切于地板上方。
    const float FloorTopZ = Center.Z + 25.f;
    const float SpawnZ = FloorTopZ + SearchRadius + 1.0f; 

    // HalfX/HalfY：可用半长度 = 地图半长 - Margin。
    const float HalfX = FMath::Max(0.f, BoxMapActor->Xlength * 0.5f - Margin);
    const float HalfY = FMath::Max(0.f, BoxMapActor->Ylength * 0.5f - Margin);

    // 如果可用尺寸为 0，说明 Box 太小或 Margin 过大，直接失败。
    if (HalfX <= 0.f || HalfY <= 0.f) return false;

    // 2) 随机采样 + 碰撞检测
    FRandomStream Rand;
    Rand.Initialize(FPlatformTime::Cycles());

    for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
    {
        // 在 [-HalfX, HalfX], [-HalfY, HalfY] 内均匀采样。
        const float RX = Rand.FRandRange(-HalfX, HalfX);
        const float RY = Rand.FRandRange(-HalfY, HalfY);

        // Candidate 初始点，Z 固定为计算出的安全高度
        FVector Candidate(Center.X + RX, Center.Y + RY, SpawnZ);

        // 检测候选点附近是否会与环境静态物体重叠（墙壁、WFC障碍物等）
        if (IsPointFree(World, Candidate, SearchRadius))
        {
            OutLocation = Candidate;
            return true;
        }
    }

    // 经过 MaxAttempts 次尝试仍没找到合适点，返回失败。
    return false;
}

void ABlasterGameMode::SpawnPlayersOnMap()
{
    if (!HasAuthority()) return; // GameMode runs on server; guard just in case
    if (!BoxMapActor)
    {
        return;
    }

    // For each player controller, find a random free point inside the box and respawn there
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<FVector> ChosenLocations;
    const float PlayerRadius = 34.f;
    const int32 MaxAttemptsPerPlayer = 512;

    FRandomStream Rand; Rand.Initialize(FPlatformTime::Cycles());

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = Cast<APlayerController>(*It);
        if (!PC) continue;

        FVector Loc;
        bool bFound = FindRandomFreePointInBox(BoxMapActor, World, Loc, PlayerRadius, MaxAttemptsPerPlayer, BoxMapActor->WallThickness + 32.f);
        if (!bFound)
        {
            continue;
        }

        // ensure uniqueness: if we already chose a location very close, jitter a bit or retry
        bool bUnique = true;
        for (const FVector& Ex : ChosenLocations)
        {
            if (FVector::DistSquared(Ex, Loc) < FMath::Square(150.f)) { bUnique = false; break; }
        }
        if (!bUnique)
        {
            // attempt a few more tries for a unique point
            FVector NewLoc; bool bNewFound = false;
            for (int r=0;r<8 && !bNewFound;++r)
            {
                if (FindRandomFreePointInBox(BoxMapActor, World, NewLoc, PlayerRadius, 64, BoxMapActor->WallThickness + 32.f))
                {
                    bool bNowUnique=true; for (const FVector& Ex:ChosenLocations) if (FVector::DistSquared(Ex,NewLoc)<FMath::Square(150.f)) { bNowUnique=false; break; }
                    if (bNowUnique) { Loc = NewLoc; bNewFound=true; }
                }
            }

        }

        ChosenLocations.Add(Loc);

        // Respawn player at Loc
          APawn* Pawn = PC->GetPawn(); if (Pawn) Pawn->Destroy(); FTransform T(FRotator::ZeroRotator, Loc);

        RestartPlayerAtTransform(PC, T);


    }
    // 玩家复活时，尝试生成一个AI敌人
    SpawnAIEnemy();
}

void ABlasterGameMode::SpawnAIEnemy()
{
      if (!HasAuthority() || !AICharacterClass || !BoxMapActor) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector Loc;
    const float AIRadius = 34.f;
    bool bFound = FindRandomFreePointInBox(BoxMapActor, World, Loc, AIRadius, 512, BoxMapActor->WallThickness + 32.f);

    if (bFound)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        ACharacter* SpawnedCharacter = World->SpawnActor<ACharacter>(AICharacterClass, Loc, FRotator::ZeroRotator, SpawnParams);
        if (SpawnedCharacter)
        {
            if (SpawnedCharacter->GetController() == nullptr)
            {
                SpawnedCharacter->SpawnDefaultController();
            }
        }
    }
}

// Update SpawnWeaponsOnMap: use random sampling inside box bounds and overlap test for placement
void ABlasterGameMode::SpawnWeaponsOnMap()
{
    if (!HasAuthority()) { if (GetNetMode() != NM_Standalone && GetNetMode() != NM_ListenServer && GetNetMode() != NM_DedicatedServer) return; }
    if (!BoxMapActor) { return; }

    UWorld* World = GetWorld(); if (!World) return;

    const float WeaponRadius = 20.f; const int32 MaxAttemptsPerWeapon = 256;
    FRandomStream Rand; Rand.Initialize(FPlatformTime::Cycles());

    int32 ToSpawn = FMath::Clamp(NumWeaponsToSpawn, 0, NumWeaponsToSpawn);
    if (WeaponClassesToSpawn.Num() == 0) { return; }

    for (int i=0;i<ToSpawn;++i)
    {
        FVector Loc;
        bool bFound = FindRandomFreePointInBox(BoxMapActor, World, Loc, WeaponRadius, MaxAttemptsPerWeapon, BoxMapActor->WallThickness + 16.f);
        if (!bFound)
        {
            continue;
        }

        int32 wIdx = Rand.RandRange(0, WeaponClassesToSpawn.Num()-1);
        TSubclassOf<AWeapon> WCls = WeaponClassesToSpawn[wIdx];
        if (!WCls)
        {
            continue;
        }

        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        AWeapon* W = World->SpawnActor<AWeapon>(WCls, Loc, FRotator::ZeroRotator, P);
        if (W)
        {
            W->Dropped();
        }
    }
}

void ABlasterGameMode::RegenerateMapAndRespawnQuick()
{
    if (!HasAuthority()) return;
    if (bIsMapEnding) return;
    bIsMapEnding = true;

    UWorld* World = GetWorld(); if (!World) { bIsMapEnding = false; return; }

    // Destroy existing weapons
    TArray<AActor*> FoundWeapons; UGameplayStatics::GetAllActorsOfClass(this, AWeapon::StaticClass(), FoundWeapons);
    for (AActor* AW : FoundWeapons) if (AW) AW->Destroy();

    // Destroy AI enemies
    if (AICharacterClass)
    {
        TArray<AActor*> FoundAI;
        UGameplayStatics::GetAllActorsOfClass(this, AICharacterClass, FoundAI);
        for (AActor* AI : FoundAI)
        {
            APawn* AIPawn = Cast<APawn>(AI);
            // Only destroy if it is not possessed by a player (to avoid destroying player characters if classes overlap)
            if (AIPawn && !AIPawn->IsPlayerControlled())
            {
                AIPawn->Destroy();
            }
        }
    }

    // Destroy remaining player pawns (we'll respawn them cleanly)
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = Cast<APlayerController>(*It);
        if (!PC) continue;
        APawn* Pawn = PC->GetPawn(); if (Pawn) Pawn->Destroy();
    }

    // If BoxMapActor exists, clear and regenerate its contents in-place; otherwise spawn a new one
    if (BoxMapActor && BoxMapActor->IsValidLowLevel())
    {
        BoxMapActor->ClearGeneratedObjects();
        BoxMapActor->GenerateBoxMap();
        BoxMapActor->ForceNetUpdate();
    }
    else
    {
        // spawn a new one at origin
        FTransform SpawnTransform = FTransform::Identity;
        FActorSpawnParameters SpawnParams; SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ABoxMap* NewBoxMap = World->SpawnActor<ABoxMap>(ABoxMap::StaticClass(), SpawnTransform, SpawnParams);
        if (NewBoxMap)
        {
            NewBoxMap->SetReplicates(true);
            BoxMapActor = NewBoxMap;
            BoxMapActor->GenerateBoxMap();
            BoxMapActor->ForceNetUpdate();
        }
    }

    // Spawn weapons and respawn players
    SpawnWeaponsOnMap();
    SpawnPlayersOnMap();

    bIsMapEnding = false;
}

void ABlasterGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!HasAuthority() || !NewPlayer) return;

    // 服务器端建立/维护 ExpectedPlayerControllers（用于客户端 ready 握手）
    ExpectedPlayerControllers.Add(TWeakObjectPtr<APlayerController>(NewPlayer));
}

void ABlasterGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    if (!HasAuthority()) return;

    APlayerController* PC = Cast<APlayerController>(Exiting);
    if (!PC) return;

    ExpectedPlayerControllers.Remove(TWeakObjectPtr<APlayerController>(PC));
    ReadyPlayerControllers.Remove(TWeakObjectPtr<APlayerController>(PC));
}

// Override玩家重生
void ABlasterGameMode::RestartPlayer(AController* NewPlayer)
{
}


