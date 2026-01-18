// Fill out your copyright notice in the Description page of Project Settings.


#include "OverheadWidget.h"
#include "Components/TextBlock.h"

void UOverheadWidget::SetDisplayText(FString TextToDisplay)
{
	// SetDisplayText：将传入字符串设置到 DisplayText 文本控件
	// 检查 DisplayText 是否已经绑定，防止在尚未绑定时访问空指针
	if (DisplayText)
	{
		// 将 FString 转换为 FText 并设置到控件上
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	// ShowPlayerNetRole：用于显示当前 Pawn 的远端网络角色，用于调试网络行为
	ENetRole RemoteRole = InPawn->GetRemoteRole();
	FString Role;
	switch (RemoteRole)
	{
	case ENetRole::ROLE_Authority:
		Role = FString("Authority");
		break;
	case ENetRole::ROLE_AutonomousProxy:
		Role = FString("Autonomous Proxy");
		break;
	case ENetRole::ROLE_SimulatedProxy:
		Role = FString("Simulated Proxy");
		break;
	case ENetRole::ROLE_None:
		Role = FString("None");
		break;
	}
	// 格式化输出并调用 SetDisplayText 将结果写入 UI
	FString RemoteRoleString = FString::Printf(TEXT("Remote Role: %s"), *Role);
	SetDisplayText(RemoteRoleString);
}

void UOverheadWidget::NativeDestruct()
{
	// NativeDestruct：Widget 被销毁时的回调
	// 这里先从父节点移除自身，然后调用父类的销毁逻辑以保证资源正确释放
	RemoveFromParent();
	Super::NativeDestruct();
}