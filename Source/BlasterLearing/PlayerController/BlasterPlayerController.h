#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterLearing/PlayerState/BlasterPlayerState.h"
#include "BlasterPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHighPingDelegate, bool, bPingTooHigh);

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	// HUD 更新系列方法：用于外部（Character/PlayerState）在数据变化时更新本地 HUD（仅本地显示）
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDScore(float Score);
	void SetHUDDefeats(int32 Defeats);
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
	void SetHUDMatchCountdown(float CountdownTime);
	void SetHUDAnnouncementCountdown(float CountdownTime);
	void SetHUDGrenades(int32 Grenades);

	// Possess 回调：当控制器持有 Pawn 时进行必要的 HUD 初始化
	virtual void OnPossess(APawn* InPawn) override;

	// Tick：每帧调用，用于处理 HUD 更新、时间同步与 Ping 检测等
	virtual void Tick(float DeltaTime) override; 

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 时间同步：客户端请求服务器时间并在收到回复后计算 ClientServerDelta
	virtual float GetServerTime(); // 获取已校准的服务器时间（客户端使用 ClientServerDelta）
	virtual void ReceivedPlayer() override; // 玩家准备好时触发（用于请求初始服务器时间）

	// MatchState 变化处理：当 MatchState 更新时通知 HUD 做对应转换
	void OnMatchStateSet(FName State);
	void HandleMatchHasStarted();

	float SingleTripTime = 0.f; // 单程延迟估计（RTT/2）

	FHighPingDelegate HighPingDelegate; // 高延迟状态广播委托（订阅者可用于显示/记录）
	
	// 用于广播淘汰信息到客户端显示
	void BroadcastElim(APlayerState* Attacker, APlayerState* Victim);
 
protected:
	virtual void BeginPlay() override;
	void SetHUDTime();
	void PollInit();
	virtual void SetupInputComponent() override;

	/*
	* 客户端与服务器间的时间同步 RPC
	*/
	// ����ͬ��������ʱ�䣬��������ʱ����ͻ��˵�ǰʱ��
	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	// �����������ǰʱ�䵽�ͻ�����Ӧ���������������ʱ��
	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	float ClientServerDelta = 0.f; // �ͻ��˺ͷ�������ʱ���

	UPROPERTY(EditAnywhere, Category = Time)
	float TimeSyncFrequency = 5.f; // ����һ�Σ�����ʱ��

	float TimeSyncRunningTime = 0.f;
	void CheckTimeSync(float DeltaTime);

	// 请求服务器返回当前比赛状态（用于中途加入）
	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();
	
	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float StartingTime);

	// 高延迟提示相关方法
	void HighPingWarning();
	void StopHighPingWarning();
	void CheckPing(float DeltaTime);

	// 显示返回主菜单 UI（按键触发）
	void ShowReturnToMainMenu();

	// 客户端淘汰公告 RPC（服务器调用后每个客户端执行）
	UFUNCTION(Client, Reliable)
	void ClientElimAnnouncement(APlayerState* Attacker, APlayerState* Victim);
	

	// HUD 文本生成辅助
	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players);

public:
	// Notify server when client has received/seen the BoxMap (handshake to avoid spawn race)
	UFUNCTION(Server, Reliable)
	void ServerNotifyBoxMapReady();

	// Server can ask client to re-check for BoxMap presence immediately (used after server spawns new BoxMap)
	UFUNCTION(Client, Reliable)
	void ClientRecheckBoxMap();

protected:
	// Repeatedly check for BoxMap / WFCTile presence on client; when detected call server RPC
	void TryNotifyBoxMapReady();
	FTimerHandle BoxMapReadyTimerHandle;
	float BoxMapReadyCheckInterval = 0.2f; // seconds between checks
	int32 BoxMapReadyMaxAttempts = 25; // max checks (~5s)
	int32 BoxMapReadyAttempts = 0;

private:
	// 缓存的 HUD 指针，避免每次都调用 GetHUD()
	UPROPERTY()
	class ABlasterHUD* BlasterHUD;

	// 返回主菜单 Widget 模板与实例
	UPROPERTY(EditAnywhere, Category = HUD)
	TSubclassOf<class UUserWidget> ReturnToMainMenuWdiget;

	UPROPERTY()
	class UReturnToMainMenu* ReturnToMainMenu;

	bool bReturnToMainMenuOpen = false;

	// 缓存 GameMode 指针与比赛时间参数
	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode;

	float LevelStartingTime = 0.f;
	float MatchTime = 0.f;
	float WarmupTime = 0.f;
	uint32 CountdownInt = 0;

	// MatchState 复制并触发 OnRep_MatchState 回调
	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;

	UFUNCTION()
	void OnRep_MatchState();

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;
	bool bInitializeCharacterOverlay = false;

	// HUD 延迟初始化缓存字段（当 HUD 尚未创建时暂存数值）
	float HUDHealth;
	bool bInitializeHealth = false;
	float HUDMaxHealth;
	float HUDScore;
	bool bInitializeScore = false;
	int32 HUDDefeats;
	bool bInitializeDefeats = false;
	int32 HUDGrenades;
	bool bInitializeGrenades = false;
	float HUDCarriedAmmo;
	bool bInitializeCarriedAmmo = false;
	float HUDWeaponAmmo;
	bool bInitializeWeaponAmmo = false;

	// Ping 检测计时器
	float HighPingRunningTime = 0.f;

	UPROPERTY(EditAnywhere)
	float HighPingDuration = 5.f;

	float PingAnimationRunningTime = 0.f;

	UPROPERTY(EditAnywhere)
	float CheckPingFrequency = 20.f;

	UFUNCTION(Server, Reliable)
	void ServerReportPingStatus(bool bHighPing);

	// 超过该阈值视为高延迟（ms）
	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f;
};
