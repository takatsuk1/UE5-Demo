#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlasterLearing/HUD/BlasterHUD.h"
#include "BlasterLearing/Weapon/WeaponTypes.h"
#include "BlasterLearing/BlasterTypes/CombatState.h"
#include "CombatComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTERLEARING_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/*
	* 基础
	*/
	// 构造函数：初始化组件默认值（如基础移速/瞄准移速等）。
	UCombatComponent();
	// 友元声明：允许角色类直接访问战斗组件的私有数据与内部函数。
	friend class ABlasterCharacter;
	// Tick：本地受控时每帧更新准星射线、HUD 准星数据与相机 FOV 插值。
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// 注册网络复制属性：声明哪些成员需要在客户端/服务器间同步。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/*
	* 武器装备 / 切换
	*/
	// 装备武器入口：根据当前是否已有主/副武器决定装备到哪个槽位。
	void EquipWeapon(class AWeapon* WeaponToEquip);
	// 切换武器：服务器端触发切枪流程并进入切换状态。
	void SwapWeapons();
	// 切枪动画结束回调：重置战斗状态并标记切换完成。
	UFUNCTION(BlueprintCallable)
	void FinishSwap();
	// 切枪挂载回调：交换主副武器引用并重新挂载/刷新 HUD。
	UFUNCTION(BlueprintCallable)
	void FinishSwapAttachWeapons();

	/*
	* 开火输入
	*/
	// 开火键按下/松开：记录持续开火状态，并在按下时触发首发。
	void FireButtonPressed(bool bPressed);

	/*
	* 重装（对外接口/动画回调）
	*/
	// 发起重装：满足条件时通知服务器并播放重装流程。
	void Reload();
	// 重装动画结束回调：更新弹药/状态，并在按住开火时继续开火。
	UFUNCTION(BlueprintCallable)
	void FinishReloading();
	// 霰弹枪逐壳装填回调：服务器端每次装填都同步更新弹药。
	UFUNCTION(BlueprintCallable)
	void ShotgunShellReload();
	// 霰弹枪装填结束：跳到装填收尾动画段。
	void JumpToShotgunEnd();
	// 本地重装锁：避免客户端预测与服务器状态不一致导致重复重装。
	bool bLocallyReloading = false;

	/*
	* 手雷（动画回调/触发点）
	*/
	// 投掷动画结束回调：恢复状态并把武器重新挂回右手。
	UFUNCTION(BlueprintCallable)
	void ThrowGrenadeFinished();
	// 生成/发射手雷的触发点：本地端调用服务器生成投射物。
	UFUNCTION(BlueprintCallable)
	void LaunchGrenade();
	// 服务器 RPC：在服务器上按目标方向生成手雷投射物。
	UFUNCTION(Server, Reliable)
	void ServerLaunchGrenade(const FVector_NetQuantize& Target);

protected:
	/*
	* 生命周期
	*/
	// BeginPlay：初始化角色移速、默认 FOV，并在服务器初始化携带弹药。
	virtual void BeginPlay() override;

	/*
	* 瞄准
	*/
	// 设置瞄准：更新移速与本地输入状态，并通知服务器同步。
	void SetAiming(bool bIsAiming);
	// 服务器 RPC：同步瞄准状态（bAiming）并更新角色移动速度。
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);

	/*
	* 武器复制回调
	*/
	// 主武器复制回调：设置状态、挂载到右手并刷新 HUD 等。
	UFUNCTION()
	void OnRep_EquippedWeapon();
	// 副武器复制回调：设置状态、挂载到背包并播放音效。
	UFUNCTION()
	void OnRep_SecondaryWeapon();

	/*
	* 开火（本地/服务器/多播）
	*/
	// 开火入口：按武器开火类型（投射物/射线/霰弹）分发，并启动射速计时器。
	void Fire();
	// 投射物武器开火：本地表现 + 请求服务器广播。
	void FireProjectileWeapon();
	// 命中扫描武器开火：本地表现 + 请求服务器广播。
	void FireHitScanWeapon();
	// 霰弹枪开火：计算散射多命中点并请求服务器广播。
	void FireShotgun();
	// 本地开火表现：播放动画并让武器执行 Fire（用于预测/多播）。
	void LocalFire(const FVector_NetQuantize& TraceHitTarget);
	// 霰弹枪本地开火表现：播放动画并执行多弹道开火。
	void ShotgunLocalFire(const TArray<FVector_NetQuantize>& TraceHitTargets);
	// 服务器 RPC：接收开火请求并验证参数后广播给所有客户端。
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget, float FireDelay);
	// 多播 RPC：在各端播放开火表现（本地预测端会被跳过）。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);
	// 服务器 RPC：霰弹枪多目标开火请求（带参数校验）。
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay);
	// 多播 RPC：霰弹枪在各端播放多弹道开火表现。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets);

	/*
	* 准星射线与 HUD
	*/
	// 从屏幕中心（或眼睛视角）射线检测，得到准星命中点。
	void TraceUnderCrosshairs(FHitResult& TraceHitResult);
	// 设置 HUD 准星：根据速度/空中/瞄准/开火扩散更新准星数据包。
	void SetHUDCrosshairs(float DeltaTime);

	/*
	* 重装（服务器/内部实现）
	*/
	// 服务器 RPC：进入重装状态并驱动非本地端播放重装。
	UFUNCTION(Server, Reliable)
	void ServerReload();
	// 执行重装表现：播放重装蒙太奇。
	void HandleReload();
	// 计算本次可装填子弹数：弹匣空位与携带弹药取最小。
	int32 AmountToReload();

	/*
	* 手雷（服务器/配置）
	*/
	// 投掷手雷入口：检查数量与状态，播放动画并按需通知服务器。
	void ThrowGrenade();
	// 服务器 RPC：驱动投掷流程（动画/挂载/扣除手雷数量）。
	UFUNCTION(Server, Reliable)
	void ServerThrowGrenade();
	// 手雷投射物类：用于服务器生成手雷（可在编辑器中配置）。
	UPROPERTY(EditAnywhere)
	TSubclassOf<class AProjectile> GrenadeClass;

	/*
	* 挂载/丢弃/装备细分
	*/
	// 丢弃当前主武器：用于武器替换/拾取。
	void DropEquippedWeapon();
	// 把指定 Actor 挂到右手插槽（主武器/物品）。
	void AttachActorToRightHand(AActor* ActorToAttach);
	// 把指定 Actor 挂到左手/手枪插槽（视武器类型选择插槽）。
	void AttachActorToLeftHand(AActor* ActorToAttach);
	// void AttachFlagToLeftHand(AActor* Flag);
	// 把指定 Actor 挂到背包插槽（副武器）。
	void AttachActorToBackpack(AActor* ActorToAttach);
	// 刷新当前武器类型对应的携带弹药，并通知 HUD。
	void UpdateCarriedAmmo();
	// 播放装备武器音效（若武器配置了 EquipSound）。
	void PlayEquipWeaponSound(AWeapon* WeaponToEquip);
	// 若弹匣为空则自动尝试重装。
	void ReloadEmptyWeapon();
	// 显示/隐藏角色手上的可视化手雷组件。
	void ShowAttachedGrenade(bool bShowGrenade);
	// 装备为主武器：掉落旧主武器、挂右手并刷新 HUD/弹药。
	void EquipPrimaryWeapon(AWeapon* WeaponToEquip);
	// 装备为副武器：挂到背包并设置副武器状态。
	void EquipSecondaryWeapon(AWeapon* WeaponToEquip);

private:
	/*
	* Owner 缓存
	*/
	// 组件所属角色缓存：用于访问动画/相机/移动组件等。
	UPROPERTY()
	class ABlasterCharacter* Character;
	// 控制器缓存：用于设置 HUD（携带弹药、手雷等）。
	UPROPERTY()
	class ABlasterPlayerController* Controller;
	// HUD 缓存：用于设置准星包数据。
	UPROPERTY()
	class ABlasterHUD* HUD;

	/*
	* 武器 (复制)
	*/
	// 当前主武器（复制）：变化时在 OnRep 中挂载并刷新 HUD。
	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	class AWeapon* EquippedWeapon;
	// 当前副武器（复制）：变化时在 OnRep 中挂载到背包。
	UPROPERTY(ReplicatedUsing = OnRep_SecondaryWeapon)
	AWeapon* SecondaryWeapon;

	/*
	* 瞄准状态 (复制)
	*/
	// 是否处于瞄准状态（复制）：影响移动速度/准星/视野。
	UPROPERTY(ReplicatedUsing = OnRep_Aiming)
	bool bAiming = false;
	// 本地瞄准按钮状态缓存：用于 OnRep 时纠正本地端显示/状态。
	bool bAimButtonPressed = false;
	// 瞄准状态复制回调：本地受控端以输入状态为准进行修正。
	UFUNCTION()
	void OnRep_Aiming();

	/*
	* 移动速度配置
	*/
	// 基础行走速度（非瞄准）。
	UPROPERTY(EditAnywhere)
	float BaseWalkSpeed;
	// 瞄准时行走速度。
	UPROPERTY(EditAnywhere)
	float AimWalkSpeed;

	/*
	* 输入状态
	*/
	// 是否按住开火键：用于自动武器连发。
	bool bFireButtonPressed;

	/*
	* HUD and crosshairs
	*/
	// 移动速度对准星扩散的影响系数。
	float CrosshairVelocityFactor;
	// 空中/跳跃对准星扩散的影响系数。
	float CrosshairInAirFactor;
	// 瞄准对准星收缩的影响系数。
	float CrosshairAimFactor;
	// 开火瞬间对准星扩散的影响系数。
	float CrosshairShootingFactor;
	// 当前准星命中点（射线检测结果），用于作为开火目标。
	FVector HitTarget;
	// HUD 需要的准星资源与扩散等数据包。
	FHUDPackage HUDPackage;

	/*
	* FOV
	*/
	// 默认视野（非瞄准）缓存。
	float DefaultFOV;
	// 默认瞄准视野（可被武器自身的 ZoomedFOV 覆盖）。
	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomedFOV = 30.f;
	// 当前视野：每帧按瞄准状态插值更新。
	float CurrentFOV;
	// 视野插值速度：用于从瞄准回到默认视野的平滑过渡。
	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomInterpSpeed = 20.f;
	// 按瞄准状态对相机 FOV 做平滑插值。
	void InterpFOV(float DeltaTime);

	/*
	* 开火节流（射速/冷却）
	*/
	// 开火冷却/射速控制定时器句柄。
	FTimerHandle FireTimer;
	// 是否允许开火：冷却未结束或状态受限时为 false。
	bool bCanFire = true;
	// 启动射速计时器，延迟后恢复可开火。
	void StartFireTimer();
	// 射速计时结束回调：恢复开火并处理自动连发/空弹重装。
	void FireTimerFinished();
	// 是否满足开火条件：弹药、冷却、战斗状态（含霰弹枪重装特例）。
	bool CanFire();

	/*
	* 弹药 (复制)
	*/
	// 当前武器类型对应的携带弹药数量（OwnerOnly 复制）。
	UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
	int32 CarriedAmmo;
	// 携带弹药复制回调：更新 HUD，并处理霰弹枪无弹跳转装填结束。
	UFUNCTION()
	void OnRep_CarriedAmmo();
	// 按武器类型存储携带弹药池。
	TMap<EWeaponType, int32> CarriedAmmoMap;
	// 步枪初始携带弹药配置。
	UPROPERTY(EditAnywhere)
	int32 StartingARAmmo = 30;
	// 火箭筒初始携带弹药配置。
	UPROPERTY(EditAnywhere)
	int32 StartingRocketAmmo = 0;
	// 手枪初始携带弹药配置。
	UPROPERTY(EditAnywhere)
	int32 StartingPistolAmmo = 0;
	// 霰弹枪初始携带弹药配置。
	UPROPERTY(EditAnywhere)
	int32 StartingShotgunAmmo = 0;
	// 榴弹/手雷类武器初始携带弹药配置。
	UPROPERTY(EditAnywhere)
	int32 StartingGrenadeAmmo = 0;
	// 服务器端初始化携带弹药映射表。
	void InitializeCarriedAmmo();

	/*
	* 战斗状态机 (复制)
	*/
	// 战斗状态机（复制）：控制重装/切枪/投掷等行为与输入限制。
	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	ECombatState CombatState = ECombatState::ECS_Unoccuiped;
	// 战斗状态复制回调：驱动远端播放对应动画/挂载逻辑。
	UFUNCTION()
	void OnRep_CombatState();
	// 常规武器重装后更新弹药：扣携带弹药、加弹匣并刷新 HUD。
	void UpdateAmmoValues();
	// 霰弹枪逐壳装填时更新弹药：每次装填扣 1 发并可能结束装填。
	void UpdateShotgunAmmoValues();

	/*
	* 手雷数量 (复制)
	*/
	// 当前手雷数量（复制）：用于投掷逻辑与 HUD 显示。
	UPROPERTY(ReplicatedUsing = OnRep_Grenades)
	int32 Grenades = 4;
	// 手雷数量复制回调：刷新 HUD 手雷显示。
	UFUNCTION()
	void OnRep_Grenades();
	// 最大手雷携带数量上限。
	UPROPERTY(EditAnywhere)
	int32 MaxGrenades = 4;
	// 更新 HUD 上的手雷数量显示。
	void UpdateHUDGrenade();

public:
	/*
	* 只读查询
	*/
	// 获取当前手雷数量（内联 getter）。
	FORCEINLINE int32 GetGrenades() const { return Grenades; }
	// 是否满足切枪条件：同时拥有主武器与副武器。
	bool ShouldSwapWeapons();
};
