// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Menu.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()
	/**
	 * 在蓝图中调用：初始化并展示多人联机菜单。
	 * @param NumOfPublicConnections 公开连接数（房间最大可加入玩家数）。
	 * @param TypeOfMatch            自定义匹配类型（用于在搜索结果中筛选房间）。
	 * @param LobbyPath              Lobby 地图路径（会自动拼上 ?listen 作为服务器监听地图）。
	 */
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("FreeForAll")), FString LobbyPath = FString(TEXT("/Game/ThirdPerson/Maps/Lobby")));

protected:
	// UUserWidget 生命周期：这里做按钮事件绑定
	virtual bool Initialize() override;
	// Widget 销毁前回调：这里做输入模式/鼠标光标恢复
	virtual void NativeDestruct() override;

	//
	// 由子系统（UMultiplayerSessionsSubsystem）触发的回调：菜单负责处理 UI 状态与 Travel
	//
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful); // 创建房间完成（成功则 ServerTravel 到 Lobby）
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful); // 搜索房间完成（筛选 MatchType）
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result); // 加入房间完成（解析 connect string 后 ClientTravel）
	UFUNCTION()
	void OnDestorySession(bool bWasSuccessful); // 销毁房间完成（当前未使用，可扩展）
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful); // 开始房间完成（当前未使用，可扩展）

private:

	// UMG 里绑定的 Host 按钮（需要在蓝图 Widget 中同名变量并勾选 Is Variable）
	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;

	// UMG 里绑定的 Join 按钮
	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	// 点击 Host：调用子系统创建 Session
	UFUNCTION()
	void HostButtonClicked();

	// 点击 Join：调用子系统搜索并尝试加入 Session
	UFUNCTION()
	void JoinButtonClicked();

	// 菜单收尾：移除 UI，并把输入模式从 UIOnly 切回 GameOnly
	void MenuTearDown();

	// 负责 Create/Find/Join/Destroy 等在线会话逻辑的 GameInstanceSubsystem
	// UPROPERTY：让 UObject GC 知道这里持有引用，避免被回收后变成悬空指针
	UPROPERTY(Transient)
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	// 公开连接数：从 MenuSetup 传入，供 CreateSession 使用
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	int32 NumPublicConnections{4};

	// 匹配类型：从 MenuSetup 传入，供 CreateSession 写入 SessionSettings，FindSessions 时筛选
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FString MatchType{TEXT("FreeForAll")};

	// Lobby 地图路径（最终会成为 /Game/.../Lobby?listen）
	FString PathToLobby{TEXT("")};
};
