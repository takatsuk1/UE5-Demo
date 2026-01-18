// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

void UMenu::MenuSetup(int32 NumOfPublicConnections, FString TypeOfMatch, FString LobbyPath)
{
	// Lobby 地图加上 ?listen：服务器以监听模式开图，客户端才能通过 Join 后连接进来
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	// 保存创建房间/筛选房间所需的配置
	NumPublicConnections = NumOfPublicConnections;
	MatchType = TypeOfMatch;

	// 把菜单 Widget 显示到屏幕上
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();

	// 切换输入模式：菜单期间只接收 UI 输入，并显示鼠标
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	// 从 GameInstance 获取会话子系统（Subsystem 的生命周期跨关卡，适合管理 Session）
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	// 绑定子系统事件：当 Create/Find/Join/Destroy/Start 完成时，回调到本菜单
	if (MultiplayerSessionsSubsystem)
	{
		// Dynamic Multicast：目标函数必须是 UFUNCTION()
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		// 普通 Multicast：仅 C++ 使用，绑定 UObject 成员函数
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestorySessionComplete.AddDynamic(this, &ThisClass::OnDestorySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// 绑定 UI 按钮点击事件
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}
	return true;
}

void UMenu::NativeDestruct()
{
	// Widget 被销毁时恢复游戏输入模式
	MenuTearDown();
	Super::NativeDestruct();
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	// CreateSession 的异步结果：成功则开图（ServerTravel），失败则恢复 Host 按钮
	if (bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString::Printf(TEXT("Session created successful!"))
			);
		}

		UWorld* World = GetWorld();
		if (World)
		{
			// 服务器切换到 Lobby 地图（带 ?listen），客户端之后会通过 Join 连接进来
			World->ServerTravel(PathToLobby);
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString::Printf(TEXT("Session created failed!"))
			);
		}

		// 失败后允许再次点击 Host
		HostButton->SetIsEnabled(true);
	}
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}

	// 遍历搜索结果：读取自定义 SessionSettings（MatchType），匹配则尝试加入
	for (auto Result : SessionResults)
	{
		FString Id = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;
		FString SettingsValue;
		// 读取创建时写入的自定义键：MatchType
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Id: %s, User: %s"), *Id, *User)
			);
		}
		if (SettingsValue == MatchType)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					15.f,
					FColor::Cyan,
					FString::Printf(TEXT("Join Match Type: %s"), *MatchType)
				);
			}
			// 异步加入：完成后会触发 OnJoinSession
			MultiplayerSessionsSubsystem->JoinSession(Result);
			return;
		}
	}

	// 没找到/或搜索失败：恢复 Join 按钮
	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	// JoinSession 完成后，需要通过 SessionInterface 解析出 connect string（IP:Port 或平台地址）
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString Address;
			// 解析加入成功后的连接地址
			if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(
						-1,
						15.f,
						FColor::Red,
						FString::Printf(TEXT("Get Connect Error"))
					);
				}
				return;
			}

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					15.f,
					FColor::Yellow,
					FString::Printf(TEXT("Connect string: %s"), *Address)
				);
			}

			// 客户端跳转到服务器地址
			APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
			if (PlayerController)
			{
				PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
			}
		}
	}

	// 加入失败：恢复 Join 按钮
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenu::OnDestorySession(bool bWasSuccessful)
{
	// 当前菜单未用到 Destroy 回调：如需支持“退出房间/重开房间”可在这里恢复 UI 状态
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
	// 当前未使用：如实现 StartSession，可在这里提示“比赛开始”等
}

void UMenu::HostButtonClicked()
{
	// 防止重复点击：等待 CreateSession 回调再恢复
	HostButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
	}
}

void UMenu::JoinButtonClicked()
{
	// 防止重复点击：等待 FindSessions/JoinSession 回调再恢复
	JoinButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		// 搜索数量上限：数值越大越可能找到更多房间，但也更耗时
		MultiplayerSessionsSubsystem->FindSessions(20000);
	}
}

void UMenu::MenuTearDown()
{
	// 从屏幕移除菜单
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			// 恢复为游戏输入模式，隐藏鼠标
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
