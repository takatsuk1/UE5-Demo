// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

/**
 * 大厅 GameMode：在多人会话中用于统计已连接玩家数量并在达到期望人数时触发地图切换
 * 说明：
 * - 在玩家登录（PostLogin）时检查当前已连接玩家数
 * - 当人数达到会话期望值时，通过 ServerTravel 切换到相应的游戏地图并启用无缝传送
 */
UCLASS()
class BLASTERLEARING_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	// 当有新玩家连接时调用，重载用于判断是否满足开始游戏的条件并触发地图切换
	virtual void PostLogin(APlayerController* NewPlayer) override;
protected:
	virtual void BeginPlay() override;
};
