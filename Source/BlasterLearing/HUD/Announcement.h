// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Announcement.generated.h"

/**
 * Announcement Widget
 * 说明：
 * - 用于在比赛不同阶段显示文字提示（例如热身倒计时、比赛信息、额外提示）
 * - WarmupTime: 显示热身阶段剩余时间的文本控件（通常由 GameMode 传入）
 * - AnnouncementText: 主要的公告文本，用于显示中央大字提示（例如队伍赢得比赛）
 * - InfoText: 辅助信息行，例如地图、规则或按键提示
 */
UCLASS()
class BLASTERLEARING_API UAnnouncement : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 绑定到 UMG 中的文本控件，用于显示热身倒计时
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* WarmupTime;

	// 绑定到 UMG 中的主公告文本（较大的文本，用于突出信息）
	UPROPERTY(meta = (BindWidget))
	UTextBlock* AnnouncementText;

	// 绑定到 UMG 中的辅助信息文本（例如额外说明、按键提示）
	UPROPERTY(meta = (BindWidget))
	UTextBlock* InfoText;
};
