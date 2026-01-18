#include "ReturnToMainMenu.h"
#include "GameFramework/PlayerController.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "BlasterLearing/Character/BlasterCharacter.h"

// MenuSetup
// 说明：将 Widget 添加到视口并设置输入模式为 GameAndUI，显示鼠标，绑定返回按钮和会话销毁回调。
// 步骤：
// 1. AddToViewport 并设置可见与焦点
// 2. 保存 PlayerController 的引用并将输入切换到 UI 模式
// 3. 绑定 ReturnButton 的点击事件（防止重复绑定）
// 4. 获取 GameInstance 的 MultiplayerSessionsSubsystem 并绑定会话销毁完成事件
void UReturnToMainMenu::MenuSetup()
{
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	// bIsFocusable 已弃用，输入焦点通过 SetWidgetToFocus/FInputModeGameAndUI 管理，因此移除对其写入
	
	UWorld* World = GetWorld();
	if (World)
	{
		// 获取或缓存 PlayerController
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			// 设置输入模式为 GameAndUI 并聚焦当前 Widget，显示鼠标
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	// 绑定按钮点击事件（如果尚未绑定）
	if (ReturnButton && !ReturnButton->OnClicked.IsBound())
	{
		ReturnButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}

	// 获取会话子系统并绑定销毁完成回调
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if (MultiplayerSessionsSubsystem)
		{
			// 绑定会话销毁完成事件，用于在服务器/客户端完成会话销毁后回到主菜单
			MultiplayerSessionsSubsystem->MultiplayerOnDestorySessionComplete.AddDynamic(this, &UReturnToMainMenu::OnDestroySession);
			
		}
	}
}

// MenuTearDown
// 说明：从视口移除 Widget 并恢复 InputMode；同时解除绑定事件以防悬挂回调
void UReturnToMainMenu::MenuTearDown()
{
	UWorld* World = GetWorld();
	if (World)
	{
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			// 恢复为仅游戏输入模式，隐藏鼠标
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
	// 解绑按钮点击事件
	if (ReturnButton && ReturnButton->OnClicked.IsBound())
	{
		ReturnButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	// 解绑会话销毁回调
	if (MultiplayerSessionsSubsystem && MultiplayerSessionsSubsystem->MultiplayerOnDestorySessionComplete.IsBound())
	{
		MultiplayerSessionsSubsystem->MultiplayerOnDestorySessionComplete.RemoveDynamic(this, &UReturnToMainMenu::OnDestroySession);
	}
	RemoveFromParent();
}

// Initialize
// 说明：Widget 初始化入口，按需在此处做控件检查或额外绑定
bool UReturnToMainMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// 如果需要在初始化时做进一步的检查或控件配置，可以在此处添加。

	return false;
}

// OnDestroySession
// 说明：会话销毁完成后的回调处理
// - 如果销毁失败，恢复按钮可用性
// - 如果销毁成功：
//   - 若当前是主机（存在 Auth GameMode），调用 GameMode->ReturnToMainMenuHost()
//   - 否则调用本地 PlayerController 的 ClientReturnToMainMenuWithTextReason
void UReturnToMainMenu::OnDestroySession(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		// 销毁失败时允许用户重新点击
		ReturnButton->SetIsEnabled(true);
		return;
	}
	UWorld* World = GetWorld();
	if (World)
	{
		AGameModeBase* GameMode = World->GetAuthGameMode<AGameModeBase>();
		if (GameMode)
		{
			// 主机直接调用 GameMode 的返回主菜单逻辑
			GameMode->ReturnToMainMenuHost();
		}
		else
		{
			// 客户端调用 PlayerController 的返回主菜单接口
			PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
			if (PlayerController)
			{
				PlayerController->ClientReturnToMainMenuWithTextReason(FText());
			}
		}
	}
}

// ReturnButtonClicked
// 说明：用户点击返回主菜单按钮后的处理逻辑
// 步骤：
// 1. 禁用按钮防止重复点击
// 2. 获取玩家 Pawn（期望为 ABlasterCharacter），调用其 ServerLeaveGame() 请求离开
// 3. 绑定角色的 OnLeftGame 委托以在角色离开后继续销毁会话
void UReturnToMainMenu::ReturnButtonClicked()
{
	ReturnButton->SetIsEnabled(false);
	
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* FirstPlayerController = World->GetFirstPlayerController();
		if (FirstPlayerController)
		{
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FirstPlayerController->GetPawn());
			if (BlasterCharacter)
			{
				// 请求服务器移除玩家并在角色确认离开后触发 OnPlayerLeftGame
				BlasterCharacter->ServerLeaveGame();
				BlasterCharacter->OnLeftGame.AddDynamic(this, &UReturnToMainMenu::OnPlayerLeftGame);
			}
			else
			{
				// 如果没有有效角色，恢复按钮可用性
				ReturnButton->SetIsEnabled(true);
			}
		}
	}
}

// OnPlayerLeftGame
// 说明：当角色确认离开并触发事件后调用，此处向会话子系统请求销毁会话
void UReturnToMainMenu::OnPlayerLeftGame()
{
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->DestorySession();
	}
}