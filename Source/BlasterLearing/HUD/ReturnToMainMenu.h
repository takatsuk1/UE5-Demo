#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReturnToMainMenu.generated.h"

/**
 * UReturnToMainMenu
 * 说明：
 * - 这是一个用于在多人游戏中从游戏返回主菜单的 UI Widget。
 * - 负责显示「返回主菜单」按钮并处理玩家离开会话的流程：调用角色的 ServerLeaveGame -> 等待确认 -> 销毁会话 -> 返回主菜单。
 * - 与 `UMultiplayerSessionsSubsystem` 协作以在会话销毁完成后继续流程，并在主机/客户端上分别调用合适的返回接口。
 * 注意：本类只处理 UI 层与流程的编排，实际的会话创建/销毁逻辑在 `MultiplayerSessionsSubsystem` 中实现。
 */
UCLASS()
class BLASTERLEARING_API UReturnToMainMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	// 将 Widget 添加到视口并设置为可交互（显示鼠标、设置输入模式），同时绑定按钮与会话回调
	void MenuSetup();
	// 从视口移除并解绑事件，恢复输入模式
	void MenuTearDown();

protected:
	// Widget 初始化钩子，通常用于控件绑定检查
	virtual bool Initialize() override;

	// 会话销毁完成回调（由 MultiplayerSessionsSubsystem 触发）
	// bWasSuccessful：表示会话是否成功销毁
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);

	// 玩家离开游戏（角色确认已调用 ServerLeaveGame 并完成清理）后的回调
	UFUNCTION()
	void OnPlayerLeftGame();

private:
	// 绑定到 UMG 中的返回主菜单按钮（在蓝图中通过 BindWidget 关联）
	UPROPERTY(meta = (BindWidget))
	class UButton* ReturnButton;

	// 按钮点击处理：发起离开流程（禁用按钮防止重复点击）
	UFUNCTION()
	void ReturnButtonClicked();

	// 游戏会话子系统，用于请求销毁/管理会话生命周期
	UPROPERTY()
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	// 当前的 PlayerController 引用（仅用于修改输入模式/调用客户端返回接口）
	UPROPERTY()
	class APlayerController* PlayerController;
};
