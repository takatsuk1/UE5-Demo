// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OverheadWidget.generated.h"

/**
 * OverheadWidget
 * 说明：
 * - 显示悬浮在角色上方的信息，例如玩家名、网络角色（用于调试）等
 * - SetDisplayText：设置显示文本（供 C++ 或蓝图调用）
 * - ShowPlayerNetRole：将 Pawn 的远端网络角色转为字符串并显示（用于开发调试）
 * - NativeDestruct：Widget 销毁时移除父级并执行父类清理
 */
UCLASS()
class BLASTERLEARING_API UOverheadWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 绑定到 UMG 中用于显示文字的控件
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DisplayText;

	// 将传入字符串设置到 DisplayText 上
	void SetDisplayText(FString TextToDisplay);

	// BlueprintCallable：在蓝图中也可调用以显示 Pawn 的网络角色
	UFUNCTION(BlueprintCallable)
	void ShowPlayerNetRole(APawn* InPawn);

protected:
	// 覆盖 NativeDestruct 来在销毁时做额外清理（例如移除父级）
	virtual void NativeDestruct() override;
};
