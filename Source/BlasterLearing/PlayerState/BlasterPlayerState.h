// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BlasterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API ABlasterPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	// 注册需要复制的属性（Defeats/Team）
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

	/*
	* 复制回调：当 Score/Defeats 在客户端更新时触发，用于刷新 HUD
	*/
	virtual void OnRep_Score() override;

	UFUNCTION()
	virtual void OnRep_Defeats();

	// 在服务器上增加分数并更新本地 HUD（若控制器存在）
	void AddToScore(float ScoreAmount);
	// 在服务器上增加被击倒次数并更新 HUD
	void AddToDefeats(int32 DefeatsAmount);

private:
	// 缓存对 Character/Controller 的弱引用，便于在复制回调中更新 HUD
	UPROPERTY()
	class ABlasterCharacter* Character;
	UPROPERTY()
	class ABlasterPlayerController* Controller;

	// 被击倒次数：复制到客户端时触发 OnRep_Defeats
	UPROPERTY(ReplicatedUsing = OnRep_Defeats)
	int32 Defeats;
	
};
