// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoxMap.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UDataTable;

// 用于将高度值映射到用户指定的静态网格
USTRUCT(BlueprintType)
struct FHeightMeshPair
{
    GENERATED_BODY()

    // 该映射适用于高度 <= MaxHeight 的点（单位与生成函数一致，例如世界坐标中的 Z）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeightMapping")
    float MaxHeight = 0.0f;

    // 对应要放置的静态网格
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeightMapping")
    UStaticMesh* Mesh = nullptr;
};

// ================= 新增：波函数坍缩（WFC）相关结构 ====================
USTRUCT(BlueprintType)
struct FWFCTile
{
    GENERATED_BODY()

    // Optional name for ease of editing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    FName TileName = NAME_None;

    // Static mesh to instantiate for this tile (DataTable-driven)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    UStaticMesh* Mesh = nullptr;

    // Edge types (north, east, south, west). Tiles are compatible when adjoining edges have equal type values.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeNorth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeEast = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeSouth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeWest = 0;

    // Optional weight for probabilistic selection
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC", meta=(ClampMin="0.0"))
    float Weight = 1.0f;
};

// Optional connect rule: allow edge type A to connect to B (undirected)
USTRUCT(BlueprintType)
struct FConnectPair
{
    GENERATED_BODY()

    // Identifier for the edge type
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 ID = 0;

    // Edge type that ID can connect to
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 CanConnectID = 0;
};

// DataTable row for tiles (used when driving WFC from a DataTable)
USTRUCT(BlueprintType)
struct FWFCTileRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    FName TileName = NAME_None;

    // Mesh stays in the DataTable row - WFC must use DataTable-defined meshes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    UStaticMesh* Mesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeNorth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeEast = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeSouth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 EdgeWest = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    float Weight = 1.0f;
};

// DataTable row for connect rules
USTRUCT(BlueprintType)
struct FConnectRow : public FTableRowBase
{
    GENERATED_BODY()

    // Identifier for the edge type
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 ID = 0;

    // Edge type that ID can connect to
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    int32 CanConnectID = 0;
};

UCLASS()
class BLASTERLEARING_API ABoxMap : public AActor
{
    GENERATED_BODY()

public:
      ABoxMap();
      virtual void Tick(float DeltaTime) override;
  
      UFUNCTION(BlueprintCallable, CallInEditor)
      void GenerateBoxMap();

      // (LLM/WFC rule application is handled by AIMapGenerator; BoxMap remains standalone.)
 
      // Expose clearing function so external code (GameMode) can request regeneration safely
      void ClearGeneratedObjects();

     UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MapSettings")
    float Xlength = 4000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MapSettings")
    float Ylength = 4000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MapSettings")
    float WallHeight = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MapSettings")
    float WallThickness = 100.0f;

protected:
    virtual void BeginPlay() override;
    
  UPROPERTY()
    UStaticMesh* CubeMesh;

private:
    UPROPERTY()
    UStaticMeshComponent* LastGeneratedFloor = nullptr;

    UStaticMeshComponent* CreateCubeComponent(
       const FName& Name,
       const FVector& Location,
       const FVector& Scale,
       const FName& MaterialSlotName = NAME_None
   );

    // 通用的创建静态网格组件函数（在 cpp 中实现）
    // Rotation is applied as relative rotation (degrees)
    UStaticMeshComponent* CreateMeshComponent(const FName& Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);

    // ================= 新增：基于波函数坍缩（WFC）的地图生成 =================
public:
    // 如果启用，则使用 WFC 在房间内部按格子生成瓦片化的地图（每个瓦片使用一个静态网格）
    // NOTE: WFC is required for this BoxMap. The optional toggle was removed so GenerateBoxMap always runs WFC.

    // DataTable that defines tile rows (TileName, Mesh, EdgeNorth/East/South/West, Weight)
    // WFC now requires using a DataTable for tiles; editor-side WFCTiles array has been removed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    UDataTable* TileDataTable = nullptr;

    // DataTable that defines allowed connect pairs (A,B)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
    UDataTable* ConnectDataTable = nullptr;


private:
    // 在房间内运行 WFC 并实例化瓦片静态网格
    void GenerateWFCMap();
 
     // Ensure dynamically-created components are replicated to clients
     virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, struct FReplicationFlags* RepFlags) override;

    UPROPERTY()
    bool bHasGenerated = false;

};
