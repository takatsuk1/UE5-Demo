// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterGameState.h"
#include "Net/UnrealNetwork.h"
#include "BlasterLearing/PlayerState/BlasterPlayerState.h"
#include "BlasterLearing/PlayerController/BlasterPlayerController.h"

// 注册需要网络复制的属性列表：TopScoringPlayers、RedTeamScore、BlueTeamScore
void ABlasterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 将领先玩家数组复制到客户端，使客户端可以显示谁处于领先
	DOREPLIFETIME(ABlasterGameState, TopScoringPlayers);
}

// 当有玩家得分时调用此函数以维护领先者列表（TopScoringPlayers）和 TopScore（服务端逻辑）
void ABlasterGameState::UpdateTopScore(ABlasterPlayerState* ScoringPlayer)
{
	// 如果当前没有任何领先玩家，则直接将该玩家加入领先列表并设置 TopScore
	if (TopScoringPlayers.Num() == 0)
	{
		TopScoringPlayers.Add(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}
	// 如果该玩家的分数与当前最高分相等，则将其加入并列领先列表（AddUnique 防止重复）
	else if (ScoringPlayer->GetScore() == TopScore)
	{
		TopScoringPlayers.AddUnique(ScoringPlayer);
	}
	// 如果该玩家的分数超过当前最高分，则清空并将其作为唯一领先者，同时更新 TopScore
	else if (ScoringPlayer->GetScore() > TopScore)
	{
		TopScoringPlayers.Empty();
		TopScoringPlayers.AddUnique(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}
}
