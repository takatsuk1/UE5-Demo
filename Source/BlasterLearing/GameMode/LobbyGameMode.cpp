// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "MultiplayerSessionsSubsystem.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 获取当前连接的玩家数量（通过 GameState 的 PlayerArray）
	int32 NumberOfPlayer = GameState.Get()->PlayerArray.Num();

	// 获取自定义的会话子系统（存放期望玩家数和比赛类型等信息）
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		// 确保子系统存在（会在运行时提供会话配置）
		check(Subsystem);
		UE_LOG(LogTemp, Warning, TEXT("Number of Players: %d, DesiredNumber: %d"), NumberOfPlayer, Subsystem->DesiredNumPublicConnections);
		// 如果当前已连接玩家数达到子系统期望的公有连接数，则开始地图切换
		if (NumberOfPlayer == Subsystem->DesiredNumPublicConnections)
		{
			
			UWorld* World = GetWorld();
			if (World)
			{
				// 开启无缝传送，以便在服务器切换地图时保留玩家连接
				bUseSeamlessTravel = true;

				// 根据 DesiredMatchType 决定加载哪个地图并以 listen 模式启动
				FString MatchType = Subsystem->DesiredMatchType;
				if (MatchType == "FreeForAll")
				{
					// 自由对战地图
					World->ServerTravel(FString("/Game/Maps/BoxMap?listen"));
				}
				else
				{
					// 其他模式已移除，默认回退到 FreeForAll
					World->ServerTravel(FString("/Game/Maps/BoxMap?listen"));
				}
			}
		}
	}
}

void ALobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
}


