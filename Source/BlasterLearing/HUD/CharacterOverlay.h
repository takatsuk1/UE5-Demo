// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterOverlay.generated.h"

/**
 * CharacterOverlay Widget
 * 说明：
 * - 显示角色的实时状态：血量、护盾、分数、击倒次数、弹药、手雷数量与比赛倒计时等
 * - 这些控件通过 Blueprint 或 PlayerController/Character 在运行时更新文本/进度条数值
 */
UCLASS()
class BLASTERLEARING_API UCharacterOverlay : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 血量进度条（左上角）
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthBar;

	// 血量文本（例如 "75 / 100"）
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* HealthText;

	// 玩家分数文本
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScoreAmount;

	// 被击倒次数文本
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DefeatsAmount;

	// 当前武器弹匣子弹数
	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeaponAmmoAmount;

	// 背包中携带的弹药数量
	UPROPERTY(meta = (BindWidget))
	UTextBlock* CarriedAmmoAmount;

	// 比赛倒计时文本（热身/比赛/冷却）
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MatchCountdownText;

	// 手雷数量文本
	UPROPERTY(meta = (BindWidget))
	UTextBlock* GrenadesText;

	// 高延迟警告图标（当 Ping 高于阈值时显示）
	UPROPERTY(meta = (BindWidget))
	class UImage* HighPingImage;

	// 高延迟动画（当触发高 Ping 时播放）
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* HighPingAnimation;
};
