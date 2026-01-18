// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"

// 武器前向声明：用于武器生成配置。
class AWeapon;
// PlayerController 前向声明：用于登录/退出与 ready 握手。
class APlayerController;
// BoxMap 前向声明：用于生成并在盒子地图内重生/生成武器。
class ABoxMap;

/**
 * 游戏模式：负责比赛流程（Warmup/Match）、玩家淘汰/重生，以及地图/武器/AI 的服务器端生成。
 */
UCLASS()
class BLASTERLEARING_API ABlasterGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	/*
	* 基础
	*/
	// 构造：开启延迟开始（Warmup 阶段），等待玩家加入后再正式开始比赛。
	ABlasterGameMode();
	// Tick：根据当前 MatchState 计算倒计时并在阶段结束时切换状态。
	virtual void Tick(float DeltaTime) override;

	/*
	* 玩家事件：淘汰 / 重生 / 离开
	*/
	// 玩家被淘汰：结算分数/死亡数、广播击杀信息，并在条件满足时触发快速重置。
	virtual void PlayerEliminated(class ABlasterCharacter* ElimmedCharacter, class ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController);
	// 请求重生：销毁旧 Pawn，并在随机 PlayerStart 重生。
	virtual void RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController);
	// 玩家离开：从领先列表移除并触发离开淘汰逻辑。
	void PlayerLeftGame(class ABlasterPlayerState* PlayerLeaving);

	/*
	* 客户端 ready 握手（避免 BoxMap 未同步就生成/重生的竞态）
	*/
	// 客户端通知已准备好：由 PlayerController 的 RPC 间接调用，满足条件后允许 Warmup 继续倒计时。
	UFUNCTION()
	void NotifyClientReady(APlayerController* PC);
	// 客户端上报 ready 的等待超时时间（秒）。
	UPROPERTY(EditDefaultsOnly, Category = "Map|Spawn")
	float ClientReadyTimeout = 6.0f;

	/*
	* AI 生成
	*/
	// AI 敌人角色类：用于在地图内随机生成 AI。
	UPROPERTY(EditAnywhere, Category = "AI")
	TSubclassOf<class ACharacter> AICharacterClass;
	// 在 BoxMap 范围内生成一个 AI 敌人。
	void SpawnAIEnemy();

	/*
	* 比赛时长配置
	*/
	// 热身阶段时长（秒）。
	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;
	// 比赛进行阶段时长（秒）。
	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;
	// 关卡开始时间点（用于计算各阶段倒计时）。
	float LevelStartingTime = 0.f;

	/*
	* 客户端广播
	*/
	// 获取当前倒计时（HUD 显示用）。
	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }

	/*
	* 登录/退出
	*/
	// 玩家登录：服务器维护期望的控制器集合（用于 ready 握手）。
	virtual void PostLogin(APlayerController* NewPlayer) override;
	// 玩家退出：从期望集合与 ready 集合中移除。
	virtual void Logout(AController* Exiting) override;
	
	virtual void RestartPlayer(AController* NewPlayer) override;

protected:
	/*
	* 生命周期
	*/
	// BeginPlay：记录 LevelStartingTime，并尝试在场景中查找已有的 BoxMap Actor。
	virtual void BeginPlay() override;
	// MatchState 变化回调：通知所有 PlayerController 更新 HUD/状态。
	virtual void OnMatchStateSet() override;

	/*
	* 比赛开始初始化（延迟开始模式下）
	*/
	// 是否已执行过比赛开始初始化（仅执行一次）。
	bool bMatchStartInitialized = false;
	// 比赛正式开始时初始化：生成 BoxMap/武器，并把玩家重生到地图内。
	void StartMatchInitialization();

private:
	/*
	* 倒计时
	*/
	// 当前阶段倒计时（HUD 显示/逻辑判断用）。
	float CountdownTime = 0.f;

	/*
	* BoxMap 引用
	*/
	// BoxMap Actor 缓存：用于在其范围内生成武器与随机重生点。
	UPROPERTY(Transient)
	class ABoxMap* BoxMapActor = nullptr;

	/*
	* 地图实体生成
	*/
	// 在 BoxMap 内为所有玩家寻找可用点并重生。
	void SpawnPlayersOnMap();
	// 在 BoxMap 内随机生成可拾取武器。
	void SpawnWeaponsOnMap();

	// MapEntity 重试次数：用于 BoxMap 尚未就绪时的延迟重试（预留）。
	int32 MapEntityRetryAttempts = 0;
	// MapEntity 最大重试次数：用于限制延迟重试次数（预留）。
	int32 MapEntityMaxRetries = 10;
	// MapEntity 重试间隔（秒）：用于控制延迟重试节奏（预留）。
	float MapEntityRetryDelay = 0.5f;

	/*
	* 武器生成配置
	*/
	// 可生成的武器类列表（生成时随机抽取）。
	UPROPERTY(EditAnywhere, Category = "Map|Weapons")
	TArray<TSubclassOf<AWeapon>> WeaponClassesToSpawn;
	// 生成武器数量。
	UPROPERTY(EditAnywhere, Category = "Map|Weapons", meta = (ClampMin = "0"))
	int32 NumWeaponsToSpawn = 8;

	/*
	* 快速重置（只剩 1 名玩家时）
	*/
	// 快速重置：清理地图/武器/玩家并重新生成（用于对局快速循环）。
	void RegenerateMapAndRespawnQuick();
	// 重置锁：避免并发触发多次重置。
	bool bIsMapEnding = false;

	/*
	* ready 握手数据
	*/
	// 期望参与握手的控制器集合（服务器端维护）。
	TSet<TWeakObjectPtr<APlayerController>> ExpectedPlayerControllers;
	// 已上报 ready 的控制器集合。
	TSet<TWeakObjectPtr<APlayerController>> ReadyPlayerControllers;
	// 关卡/客户端是否已准备就绪：未就绪时 Warmup 倒计时会被暂停。
	bool bLevelReady = false;
};
