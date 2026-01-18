#include "BlasterPlayerController.h"
#include "BlasterLearing/HUD/BlasterHUD.h"
#include "BlasterLearing/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "BlasterLearing/Character/BlasterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "BlasterLearing/GameMode/BlasterGameMode.h"
#include "BlasterLearing/PlayerState/BlasterPlayerState.h"
#include "BlasterLearing/HUD/Announcement.h"
#include "kismet/GameplayStatics.h"
#include "BlasterLearing/BlasterComponent/CombatComponent.h"
#include "BlasterLearing/GameState/BlasterGameState.h"
#include "Components/Image.h"
#include "BlasterLearing/HUD/ReturnToMainMenu.h"
#include "BlasterLearing/BlasterTypes/Announcement.h"
#include "Engine/StaticMeshActor.h"
#include "BlasterLearing/Map/BoxMap.h"

// BroadcastElim: 由 GameMode 或其他系统调用以在所有客户端显示淘汰信息
// 说明：本函数在服务器/主控上调用，内部会触发 Client RPC 在每个客户端本地显示淘汰公告
void ABlasterPlayerController::BroadcastElim(APlayerState* Attacker, APlayerState* Victim)
{
	// 调用客户端实现，向所有对应客户端发送淘汰信息
	ClientElimAnnouncement(Attacker, Victim);
}

// ClientElimAnnouncement_Implementation: 客户端收到淘汰信息后的处理
// 逻辑：判断自己是否为攻击者或受害者以显示本地化的文字（"You"/"you"/"yourself"），否则显示玩家名
void ABlasterPlayerController::ClientElimAnnouncement_Implementation(APlayerState* Attacker, APlayerState* Victim)
{
	// 获取本地 PlayerState（用于判断“我”与传入的 Attacker/Victim 的关系）
	APlayerState* Self = GetPlayerState<APlayerState>();
	if (Attacker && Victim && Self)
	{
		// 缓存 HUD 指针（懒加载模式）
		BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
		if (BlasterHUD)
		{
			// 如果攻击者是自己且受害者不是自己，显示 "You"
			if (Attacker == Self && Victim != Self)
			{
				BlasterHUD->AddElimAnnouncement("You", Victim->GetPlayerName());
				return;
			}
			// 如果受害者是自己且攻击者不是自己，显示 "you"
			if (Victim == Self && Attacker != Self)
			{
				BlasterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), "you");
				return;
			}
			// 自杀（攻击者与受害者同为自己）
			if (Attacker == Victim && Attacker == Self)
			{
				BlasterHUD->AddElimAnnouncement("You", "yourself");
				return;
			}
			// 其他玩家自杀（攻击者==受害者但不是自己）
			if (Attacker == Victim && Attacker != Self)
			{
				BlasterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), "themselves");
				return;
			}
			// 一般情况：显示攻击者与受害者的玩家名
			BlasterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), Victim->GetPlayerName());
		}
	}
}

// BeginPlay: 控制器 BeginPlay，用于缓存 HUD 并向服务器请求比赛状态
void ABlasterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 缓存 HUD 指针以便后续频繁访问
	BlasterHUD = Cast<ABlasterHUD>(GetHUD());
	// // 请求服务器同步当前的比赛状态
	 ServerCheckMatchState();
	
	// Start periodic check to notify server when this client has received/generated the BoxMap
	if (IsLocalController())
	{
		BoxMapReadyAttempts = 0;
		GetWorldTimerManager().SetTimer(BoxMapReadyTimerHandle, this, &ABlasterPlayerController::TryNotifyBoxMapReady, BoxMapReadyCheckInterval, true);
	}
}

void ABlasterPlayerController::TryNotifyBoxMapReady()
{
	UE_LOG(LogTemp, Log, TEXT("Notify Box Map Ready"));
  BoxMapReadyAttempts++;
    // Prefer checking for replicated ABoxMap actor presence (more reliable than tags)
    TArray<AActor*> FoundBoxMaps; UGameplayStatics::GetAllActorsOfClass(this, ABoxMap::StaticClass(), FoundBoxMaps);
    if (FoundBoxMaps.Num() > 0)
    {
         ServerNotifyBoxMapReady();
        GetWorldTimerManager().ClearTimer(BoxMapReadyTimerHandle);
        return;
    }

    // Fallback: look for WFCTile actors via tag (may not always replicate)
    TArray<AActor*> FoundTiles;
    UGameplayStatics::GetAllActorsWithTag(this, FName(TEXT("WFCTile")), FoundTiles);
    if (FoundTiles.Num() > 0)
    {
      ServerNotifyBoxMapReady();
        GetWorldTimerManager().ClearTimer(BoxMapReadyTimerHandle);
        return;
    }

    // previous fallback: check for any AStaticMeshActor owned by an actor named BoxMap
    TArray<AActor*> SMAs; UGameplayStatics::GetAllActorsOfClass(this, AStaticMeshActor::StaticClass(), SMAs);
    for (AActor* A : SMAs)
    {
        AStaticMeshActor* SMA = Cast<AStaticMeshActor>(A);
        if (!SMA) continue;
        AActor* SMAOwner = SMA->GetOwner();
        if (SMAOwner && SMAOwner->GetName().Contains(TEXT("BoxMap")))
        {
            ServerNotifyBoxMapReady();
            GetWorldTimerManager().ClearTimer(BoxMapReadyTimerHandle);
            return;
        }
    }

    if (BoxMapReadyAttempts >= BoxMapReadyMaxAttempts)
    {
        // Timeout: notify server anyway to avoid blocking indefinitely
        ServerNotifyBoxMapReady();
        GetWorldTimerManager().ClearTimer(BoxMapReadyTimerHandle);
    }
}

void ABlasterPlayerController::ServerNotifyBoxMapReady_Implementation()
{
     // Inform GameMode that this client is ready
    if (!GetWorld()) return;
    AGameModeBase* GMBase = Cast<AGameModeBase>(UGameplayStatics::GetGameMode(GetWorld()));
    ABlasterGameMode* GM = GMBase ? Cast<ABlasterGameMode>(GMBase) : nullptr;
    if (GM)
    {
    	UE_LOG(LogTemp, Log, TEXT("Notify Client Ready from PC"));
        // GameMode will expose a public function to receive ready notifications
        GM->NotifyClientReady(this);
    }
}

// GetLifetimeReplicatedProps: 注册需要网络复制的属性
void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// MatchState 会通过 RepNotify 回调在客户端响应
	DOREPLIFETIME(ABlasterPlayerController, MatchState);

}

// Tick: 每帧调用，负责更新时间、网络时间同步、初始化 HUD 元素与 Ping 检测
void ABlasterPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 更新 HUD 中显示的剩余时间（根据 MatchState 计算）
	SetHUDTime();
	// 周期性向服务器请求时间以计算客户端与服务器的时间差
	CheckTimeSync(DeltaTime);
	// 在 HUD 可用时进行初始化（把延迟初始化的 HUD 值写入控件）
	PollInit();

	// 检查并处理高 Ping 的提醒动画
	CheckPing(DeltaTime);
}

// CheckTimeSync: 周期性发送时间请求以便客户端计算与服务器的时间差 ClientServerDelta
void ABlasterPlayerController::CheckTimeSync(float DeltaTime)
{
	TimeSyncRunningTime += DeltaTime;
	// 仅本地控制器定期请求服务器时间
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		// 向服务器发送请求以获得服务器时间戳（Server RPC）
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

// HighPingWarning / StopHighPingWarning: 控制高延迟警告动画的显示与隐藏
void ABlasterPlayerController::HighPingWarning()
{
	// 懒加载 HUD 并验证相关控件与动画是否可用
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingImage &&
		BlasterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid)
	{
		// 使高 Ping 图标可见并播放动画若干次
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(1.f);
		BlasterHUD->CharacterOverlay->PlayAnimation(BlasterHUD->CharacterOverlay->HighPingAnimation, 0.f, 5);
	}
}

void ABlasterPlayerController::StopHighPingWarning()
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingImage &&
		BlasterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid)
	{
		// 隐藏图标并停止正在播放的动画
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(0.f);
		if (BlasterHUD->CharacterOverlay->IsAnimationPlaying(BlasterHUD->CharacterOverlay->HighPingAnimation))
		{
			BlasterHUD->CharacterOverlay->StopAnimation(BlasterHUD->CharacterOverlay->HighPingAnimation);
		}
	}
}

// CheckPing: 周期性检查 PlayerState 的压缩 Ping 并在超过阈值时触发高延迟逻辑与向服务器上报状态
void ABlasterPlayerController::CheckPing(float DeltaTime)
{
	// 服务器无需执行客户端的 Ping 检测逻辑
	if (HasAuthority()) return;
	HighPingRunningTime += DeltaTime;
	if (HighPingRunningTime > CheckPingFrequency)
	{
		// 懒加载 PlayerState 并获取压缩 Ping（GetCompressedPing 返回 1/4 ms 单位）
		PlayerState = PlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : PlayerState;
		if (PlayerState)
		{
			// 将压缩值恢复为 ms 并与阈值比较
			if (PlayerState->GetCompressedPing() * 4 > HighPingThreshold) // 压缩 ping 乘回实际 ms
			{
				// 触发本地高延迟显示，并向服务器报告当前高 Ping 状态
				HighPingWarning();
				PingAnimationRunningTime = 0.f;
				ServerReportPingStatus(true);
			}
			else
			{
				// 向服务器报告 Ping 正常
				ServerReportPingStatus(false);
			}
		}
		HighPingRunningTime = 0.f;
	}
	// 若高 Ping 动画正在播放，根据持续时间决定何时停止
	bool bHighPingAnimationPlaying =
		BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingAnimation &&
		BlasterHUD->CharacterOverlay->IsAnimationPlaying(BlasterHUD->CharacterOverlay->HighPingAnimation);
	if (bHighPingAnimationPlaying)
	{
		PingAnimationRunningTime += DeltaTime;
		if (PingAnimationRunningTime > HighPingDuration)
		{
			StopHighPingWarning();
		}
	}
}

// ShowReturnToMainMenu: 切换显示/隐藏返回主菜单的 Widget（按键触发）
void ABlasterPlayerController::ShowReturnToMainMenu()
{
	// 如果未设置 Widget 模板则直接返回
	if (ReturnToMainMenuWdiget == nullptr) return;
	// 懒加载或创建菜单实例
	if (ReturnToMainMenu == nullptr)
	{
		ReturnToMainMenu = CreateWidget<UReturnToMainMenu>(this, ReturnToMainMenuWdiget);
	}
	if (ReturnToMainMenu)
	{
		// 切换菜单打开状态并执行对应的 Setup / TearDown 操作
		bReturnToMainMenuOpen = !bReturnToMainMenuOpen;
		if (bReturnToMainMenuOpen)
		{
			ReturnToMainMenu->MenuSetup();
		}
		else
		{
			ReturnToMainMenu->MenuTearDown();
		}
	}
}

// ping���ߣ�
void ABlasterPlayerController::ServerReportPingStatus_Implementation(bool bHighPing)
{
	HighPingDelegate.Broadcast(bHighPing);
}

// ServerCheckMatchState_Implementation: 客户端启动时请求服务器告知当前比赛状态
void ABlasterPlayerController::ServerCheckMatchState_Implementation()
{
	ABlasterGameMode* GameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		// 从 GameMode 获取比赛时间参数并传回客户端
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		MatchState = GameMode->GetMatchState();
		// 客户端 RPC 通知客户端加入当前进行中的比赛（以初始化 HUD）
		ClientJoinMidgame(MatchState, WarmupTime, MatchTime,  LevelStartingTime);
	}
}

// ClientJoinMidgame_Implementation: 客户端接收服务器发来的比赛状态并初始化 HUD
void ABlasterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float StartingTime)
{
	// 将服务器传来的参数写入本地变量
	WarmupTime = Warmup;
	MatchTime = Match;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;
	OnMatchStateSet(MatchState);
	// 若处于等待开始状态，确保公告面板被创建显示热身倒计时
	if (BlasterHUD && MatchState == MatchState::WaitingToStart)
	{
		BlasterHUD->AddAnnouncement();
	}
}

// OnPossess: 当控制器 Possess 某 Pawn 时同步初始化 HUD 的生命值显示
void ABlasterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 尝试将 Possess 的 Pawn 转换为 ABlasterCharacter 并初始化 HUD 生命条
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(InPawn);
	if (BlasterCharacter)
	{
		SetHUDHealth(BlasterCharacter->GetHealth(), BlasterCharacter->GetMaxHealth());
	}
}

// SetHUDHealth / SetHUDShield 等：这些函数负责格式化数值并设置到 CharacterOverlay 的控件，若控件尚不可用则保存初始化标志以便 PollInit 后写回
void ABlasterPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HealthBar &&
		BlasterHUD->CharacterOverlay->HealthText;
	if (bHUDValid)
	{
		// 计算进度条百分比并把文本（如 "75/100"）写入 HUD
		const float HealthPercent = Health / MaxHealth;
		BlasterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		BlasterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
	else
	{
		// 若 HUD 还未初始化，保存数值并标记需要初始化
		bInitializeHealth = true;
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
	}
}

void ABlasterPlayerController::SetHUDScore(float Score)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->ScoreAmount;
	if (bHUDValid)
	{
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		BlasterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
	else
	{
		bInitializeScore = true;
		HUDScore = Score;
	}
}

void ABlasterPlayerController::SetHUDDefeats(int32 Defeats)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->DefeatsAmount;
	if (bHUDValid)
	{
		FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		BlasterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	}
	else
	{
		bInitializeDefeats = true;
		HUDDefeats = Defeats;
	}
}

void ABlasterPlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDMatchCountdown(float CountdownTime)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->MatchCountdownText;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}

		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDAnnouncementCountdown(float CountdownTime)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->Announcement &&
		BlasterHUD->Announcement->WarmupTime;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			BlasterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}

		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDGrenades(int32 Grenades)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->GrenadesText;
	if (bHUDValid)
	{
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		BlasterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	else
	{
		bInitializeGrenades = true;
		HUDGrenades = Grenades;
	}
}

void ABlasterPlayerController::SetHUDTime()
{
	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::InProgress) TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	if (HasAuthority())
	{
		if (BlasterGameMode == nullptr)
		{
			BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
			LevelStartingTime = BlasterGameMode->LevelStartingTime;
		}
		BlasterGameMode = BlasterGameMode == nullptr ? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this)) : BlasterGameMode;
		if (BlasterGameMode)
		{
			SecondsLeft = FMath::CeilToInt(BlasterGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}

	if (CountdownInt != SecondsLeft)
	{
		if (MatchState == MatchState::WaitingToStart )
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		if (MatchState == MatchState::InProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
		}
	}

	CountdownInt = SecondsLeft;
}

void ABlasterPlayerController::PollInit()
{
	if (CharacterOverlay == nullptr)
	{
		if (BlasterHUD && BlasterHUD->CharacterOverlay)
		{
			CharacterOverlay = BlasterHUD->CharacterOverlay;
			if (CharacterOverlay)
			{
				if (bInitializeHealth) SetHUDHealth(HUDHealth, HUDMaxHealth);
				if (bInitializeScore) SetHUDScore(HUDScore);
				if (bInitializeDefeats) SetHUDDefeats(HUDDefeats);
				if (bInitializeWeaponAmmo) SetHUDWeaponAmmo(HUDWeaponAmmo);
				if (bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);

				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
				if (BlasterCharacter && BlasterCharacter->GetCombat())
				{
					if (bInitializeGrenades) SetHUDGrenades(BlasterCharacter->GetCombat()->GetGrenades());
				}
			}
		}
	}
}

void ABlasterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent == nullptr) return;

	InputComponent->BindAction("Quit", IE_Pressed, this, &ABlasterPlayerController::ShowReturnToMainMenu);
}

// ServerRequestServerTime_Implementation: 服务端接收客户端请求的时间戳并计算往返时间，更新 ClientServerDelta
void ABlasterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

// ClientReportServerTime_Implementation: 客户端接收服务器的时间戳并计算当前服务器时间
void ABlasterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest)
{
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	SingleTripTime = 0.5f * RoundTripTime;
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

// GetServerTime: 获取当前服务器时间，客户端通过 ClientServerDelta 进行补偿
float ABlasterPlayerController::GetServerTime()
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void ABlasterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

// OnMatchStateSet: 当比赛状态改变时由服务器通知客户端，客户端根据状态执行相应的处理
void ABlasterPlayerController::OnMatchStateSet(FName State)
{
	MatchState = State;

	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
}

// OnRep_MatchState: MatchState 属性被更新时在客户端的响应函数
void ABlasterPlayerController::OnRep_MatchState()
{
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
}

// HandleMatchHasStarted: 处理比赛开始的逻辑，包括初始化 HUD、隐藏公告等
void ABlasterPlayerController::HandleMatchHasStarted()
{
	// if (HasAuthority()) bShowTeamScores = bTeamMatch;
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	if (BlasterHUD)
	{
		if (BlasterHUD->CharacterOverlay == nullptr) BlasterHUD->AddCharacterOverlay();
		if (BlasterHUD->Announcement)
		{
			BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

FString ABlasterPlayerController::GetInfoText(const TArray<class ABlasterPlayerState*>& Players)
{
	ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
	if (BlasterPlayerState == nullptr) return FString();
	FString InfoTextString;
	if (Players.Num() == 0)
	{
		InfoTextString = Announcement::ThereIsNoWinner;
	}
	else if (Players.Num() == 1 && Players[0] == BlasterPlayerState)
	{
		InfoTextString = Announcement::YouAreTheWinner;
	}
	else if (Players.Num() == 1)
	{
		InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *Players[0]->GetPlayerName());
	}
	else if (Players.Num() > 1)
	{
		InfoTextString = Announcement::PlayersTiedForTheWin;
		InfoTextString.Append(FString("\n"));
		for (auto TiedPlayer : Players)
		{
			InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
		}
	}
	return InfoTextString;
}

void ABlasterPlayerController::ClientRecheckBoxMap_Implementation()
{
	// Reset attempts and (re)start the periodic check just like BeginPlay does
	if (IsLocalController())
	{
		BoxMapReadyAttempts = 0;
		GetWorldTimerManager().ClearTimer(BoxMapReadyTimerHandle);
		GetWorldTimerManager().SetTimer(BoxMapReadyTimerHandle, this, &ABlasterPlayerController::TryNotifyBoxMapReady, BoxMapReadyCheckInterval, true);
		// Also perform an immediate check
		TryNotifyBoxMapReady();
	}
}
