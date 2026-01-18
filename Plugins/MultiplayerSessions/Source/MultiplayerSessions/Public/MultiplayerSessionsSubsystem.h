// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "MultiplayerSessionsSubsystem.generated.h"

//
// 自定义委托：用于把 OnlineSubsystem 的异步回调转发给 UI/蓝图。
// 注意：Dynamic Multicast 委托可被蓝图绑定（需要 UFUNCTION + AddDynamic）。
//      普通 Multicast 委托仅 C++ 可用（可用 AddUObject / AddLambda 等）。
//
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWaSuccessful); // 创建 Session 完成
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful); // 查找 Session 完成（C++ only）
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Reuslt); // 加入 Session 完成（C++ only）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestorySessionComplete, bool, bWasSuccessful); // 销毁 Session 完成
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful); // StartSession 完成

/**
 * GameInstance 级别的子系统：集中管理 Online Session（创建/查找/加入/销毁）。
 * 设计目的：把 OnlineSubsystem 的异步回调封装起来，通过委托通知 UI 层（例如 UMenu）。
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UMultiplayerSessionsSubsystem();

	//
	// 对外 API：菜单会调用这些函数，子系统内部负责与 OnlineSubsystem 会话接口交互
	//
	void CreateSession(int32 NumPubilcConnections, FString MatchType);
	void FindSessions(int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestorySession(); // 注意：这里函数名拼写是 Destory（保持现有 API），内部会调用 DestroySession
	void StartSession();   // 当前未实现：预期用于把 Session 从 Pending/Setup 推进到 InProgress

	//
	// 事件：供 UI 绑定，用于接收异步完成通知
	//
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestorySessionComplete MultiplayerOnDestorySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;

	// 记录“用户想要的配置”，便于 Destroy 成功后自动重建
	int32 DesiredNumPublicConnections{};
	FString DesiredMatchType{};

protected:
	//
	// OnlineSubsystem 原生回调：只在子系统内部使用（负责清理 DelegateHandle 并 Broadcast 自定义事件）
	//
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestorySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

private:
	// OnlineSubsystem 的 Session 接口（由当前平台/子系统实现：NULL/Steam/EOS...）
	IOnlineSessionPtr SessionInterface;

	// 上一次 CreateSession 的设置（回调里可能需要读取；也方便调试/重建）
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	// 上一次 FindSessions 的搜索对象（回调里读取 SearchResults）
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	//
	// 下面这些 Delegate + Handle：用于绑定到 OnlineSubsystem，并在回调完成后解除绑定，避免重复触发
	//
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestorySessionCompleteDelegate;
	FDelegateHandle DestorySessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;

	// 如果创建 Session 时发现已经存在同名 Session，则先 Destroy；Destroy 成功后再自动 Create
	bool bCreateSessionOnDestory{ false };
	int32 LastNumPublicConnections;
	FString LastMatchType;
};
