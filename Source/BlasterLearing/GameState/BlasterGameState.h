// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "BlasterGameState.generated.h"

/**
 * 自定义 GameState：用于记录比分、领先玩家与分队信息
 * 说明：
 * - TopScoringPlayers: 保存当前领先分数的玩家列表（可能有多个并列）
 * - RedTeamScore / BlueTeamScore: 两队的总分，通过 ReplicatedUsing 通知客户端更新 HUD
 */
UCLASS()
class BLASTERLEARING_API ABlasterGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	// 重写：注册需要网络复制的属性
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 更新领先者列表：当某玩家得分后调用以维护 TopScoringPlayers 与 TopScore
	void UpdateTopScore(class ABlasterPlayerState* ScoringPlayer);

	// 当前领先的玩家集合（通过网络复制给客户端）
	UPROPERTY(Replicated)
	TArray<class ABlasterPlayerState*> TopScoringPlayers;


private:
	// 内部维护的最高分值（仅服务器维护）
	float TopScore = 0.f;
};
