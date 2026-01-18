// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include <Online/OnlineSessionNames.h>

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem() :
	// 把 OnlineSubsystem 的回调委托绑定到本 Subsystem 成员函数（CreateUObject 会持有 this）
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestorySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestorySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))
{
	// 获取当前平台的 OnlineSubsystem（NULL/Steam/EOS...），并拿到 SessionInterface
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPubilcConnections, FString MatchType)
{
	// 保存用户本次想创建的配置（也用于 Destroy 后自动重建）
	DesiredNumPublicConnections = NumPubilcConnections;
	DesiredMatchType = MatchType;

	if (!SessionInterface.IsValid())
	{
		return;
	}

	// 如果已经存在同名 Session，则需要先销毁；否则 CreateSession 会失败
	auto ExistingSesion = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSesion != nullptr)
	{
		// 标记：等 Destroy 成功后自动再 Create（避免用户手动重按）
		bCreateSessionOnDestory = true;
		LastNumPublicConnections = NumPubilcConnections;
		LastMatchType = MatchType;

		DestorySession();
		return;
	}

	// 保存 DelegateHandle，便于在回调里移除绑定（避免重复触发/泄漏）
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	// NULL 子系统通常用于本地/局域网测试；其他子系统（Steam/EOS）通常为在线
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	// 公开连接数：允许多少玩家加入
	LastSessionSettings->NumPublicConnections = NumPubilcConnections;
	// 是否允许游戏进行中加入（禁用可避免中途插入）
	LastSessionSettings->bAllowJoinInProgress = false;
	// Presence：允许通过好友/状态加入
	LastSessionSettings->bAllowJoinViaPresence = true;
	// 是否对外广播（不广播则别人搜不到）
	LastSessionSettings->bShouldAdvertise = true;
	// 是否使用 Presence
	LastSessionSettings->bUsesPresence = true;
	// 如果平台支持 Lobby（如 Steam），优先使用 Lobby 管理加入
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	// 写入自定义键：MatchType（FindSessions 后用它筛选匹配类型）
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// 发起异步创建 Session
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		// 创建请求失败：清理委托绑定，并广播失败
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	// 绑定 FindSessions 完成回调
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	// 最大返回数量
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	// 是否局域网搜索
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	// 只搜索启用 Presence 的房间（与 CreateSession 的 bUsesPresence 对应）
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// 发起异步查找 Session
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		// 查找请求失败：清理委托并广播空结果
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	// 绑定 JoinSession 完成回调
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// 发起异步加入 Session
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMultiplayerSessionsSubsystem::DestorySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestorySessionComplete.Broadcast(false);
		return;
	}

	// 绑定 DestroySession 完成回调
	DestorySessionCompleteDelegateHandle =  SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestorySessionCompleteDelegate);

	// 发起异步销毁
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestorySessionCompleteDelegateHandle);
		MultiplayerOnDestorySessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::StartSession()
{
	// TODO：如需要“开始比赛”语义，可在这里调用 SessionInterface->StartSession(NAME_GameSession)
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 完成后立刻解除绑定，避免下次 Create 时回调累计
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	// 转发给 UI 层
	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	// 查找结束后解除绑定
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	// 没有搜索到任何结果：直接广播空数组，UI 可据此恢复按钮
	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString::Printf(TEXT("find session is null"))
			);
		}
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	// 转发搜索结果
	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// 加入结束后解除绑定
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	// 转发结果（菜单里会解析 connect string 并 ClientTravel）
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestorySessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 销毁结束后解除绑定
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestorySessionCompleteDelegateHandle);
	}

	// 如果之前 CreateSession 发现已有 Session，则 Destroy 成功后在这里自动重建
	if (bWasSuccessful && bCreateSessionOnDestory)
	{
		bCreateSessionOnDestory = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}

	// 转发 Destroy 结果
	MultiplayerOnDestorySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// TODO：实现 StartSession 后，在此解除绑定并转发
}
