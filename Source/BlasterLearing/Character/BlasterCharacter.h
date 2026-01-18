// 本文件中文注释：
// BlasterCharacter.h - 角色类声明
// 说明：
//  ABlasterCharacter 继承自 ACharacter，并且实现了 IInteractWithCrosshairsInterface 接口，
//  用于表示玩家可操作的角色实体。头文件中声明了大量属性与方法：
//   - 生命周期：BeginPlay/ Tick / Destroyed
//   - 输入绑定：Move/Turn/LookUp / Equip / Aim / Fire / Reload / ThrowGrenade
//   - HUD/状态同步：UpdateHUD*、GetLifetimeReplicatedProps、RepNotify 回调
//   - 战斗相关：Combat 组件、武器装备、投掷手雷、换弹、射击蒙太奇等
//   - 溶解与淘汰：溶解材质与时间线、Elim/MulitcastElim
//   - 命中盒（HitCollisionBoxes）：用于服务器端回溯命中检测
//
//  重要成员简述：
//   - Combat: 管理武器和开火逻辑的组件
//   - Buff: 控制移动速度、跳跃等临时状态的组件
//   - LagCompensation: 服务器回溯辅助组件
//   - Health/Shield: 玩家生命与护盾，带有 RepNotify
//   - DissolveTimeline: 淘汰效果的时间线控制器
//
// 注：此注释仅提供中文说明，未修改原始声明或语义。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BlasterLearing/BlasterTypes/TurningInPlace.h"
#include "BlasterLearing/Interfaces/InteractWithCrosshairsInterface.h"
#include "Components/TimelineComponent.h"
#include "BlasterLearing/BlasterTypes/CombatState.h"
#include "BlasterCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLeftGame);

UCLASS()
class BLASTERLEARING_API ABlasterCharacter : public ACharacter, public IInteractWithCrosshairsInterface
{
	GENERATED_BODY()

public:
	ABlasterCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;

	/*
	* 播放动画蒙太奇 (声明处注释)
	* 这些函数在CPP中实现具体的蒙太奇播放逻辑，根据装备武器或状态选择段落
	*/
	// PlayFireMontage: 根据是否瞄准选择不同射击段（RifleAim / RifleHip）并播放射击蒙太奇
	void PlayFireMontage(bool Aiming);
	// PlayReloadMontage: 根据当前装备武器类型跳转到对应的换弹段落
	void PlayReloadMontage();
	// PlayElimMontage: 播放淘汰（死亡）动画蒙太奇
	void PlayElimMontage();
	// PlayThrowGrenadeMontage: 播放投掷手雷动画
	void PlayThrowGrenadeMontage();
	// PlaySwapMontage: 播放换枪动作动画
	void PlaySwapMontage();
	void PlayHitReactMontage();

	// 当服务器/代理更新移动时的回调重载，用于同步转身动画
	virtual void OnRep_ReplicatedMovement() override;
	// Elim: 服务器端处理角色被淘汰的入口函数（会触发MulticastElim）
	void Elim(bool bPlayerLeftGame);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastElim(bool bPlayerLeftGame);
	virtual void Destroyed() override;

	UPROPERTY(Replicated)
	bool bDisableGameplay = false;

	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser);

	// PollInit: 延迟初始化 PlayerState 等
	void PollInit();
	void RotateInPlace(float DeltaTime);

	// Input functions
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void EquipButtonPressed();
	void CrouchButtonPressed();
	void ReloadButtonPressed();
	void AimButtonPressed();
	void AimButtonReleased();
	void AimOffset(float DeltaTime);
	void CalculateAO_Pitch();
	void SimProxiesTurn();
	virtual void Jump() override;
	void FireButtonPressed();
	void FireButtonReleased();
	void GrenadeButtonPressed();
	void DropOrDestroyWeapon(AWeapon* Weapon);
	void DropOrDestroyWeapons();

	void UpdateHUDHealth();
	void UpdateHUDAmmo();
	void UpdateHUDGrenade();
	

	// 命中盒映射，供服务器回溯命中使用
	UPROPERTY()
	TMap<FName, class UBoxComponent*> HitCollisionBoxes;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* head;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* pelvis;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* spine_02;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* spine_03;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* upperarm_l;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* upperarm_r;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* lowerarm_l;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* lowerarm_r;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* hand_l;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* hand_r;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* blanket;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* backpack;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* thigh_l;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* thigh_r;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* calf_l;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* calf_r;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* foot_l;

	UPROPERTY(EditAnywhere)
	class UBoxComponent* foot_r;

	bool bFinishedSwapping = false;

	UFUNCTION(Server, Reliable)
	void ServerLeaveGame();

	FOnLeftGame OnLeftGame;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastGainedTheLead();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastLostTheLead();

	// 摄像机与 UI 组件
	// 摄像机相关：CameraBoom 用于平滑第三人称相机，FollowCamera 作为实际视角
	UPROPERTY(VisibleAnyWhere, Category = Camera)
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnyWhere, Category = Camera)
	class UCameraComponent* FollowCamera;

	// 第一人称相机
	UPROPERTY(VisibleAnyWhere, Category = Camera)
	class UCameraComponent* FirstPersonCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bHideMeshWhenFirstPerson = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;

	// OverlappingWeapon: 当前可拾取的武器，仅 Owner 可见（COND_OwnerOnly）
	UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon;

	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeapon* LastWeapon);

	UFUNCTION(Server, Reliable)
	void ServerEquipButtonPressed();

	/*
	* 角色组件
	*/
	// Combat: 战斗组件负责开火、换弹、武器交换等逻辑（已设置为可复制）
	// Combat 组件、Buff、LagCompensation 等关键组件（简短注释）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCombatComponent* Combat;
	

	UPROPERTY(VisibleAnywhere)
	class ULagCompensationComponent* LagCompensation;

	// 下面是与动画逻辑紧密相关的变量（如 AO_Yaw/Interp/AO_Pitch 等）
	float AO_Yaw;
	float InterpAO_Yaw;
	float AO_Pitch;
	FRotator StartingAimRotation;

	// 转身枚举与处理函数
	ETurningInPlace TurningInPlace;
	void TurnInPlace(float DeltaTime);

	/*
	* 动画蒙太奇
	*/
	// 以下为可在编辑器设置的动画蒙太奇资源引用
	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* FireWeaponMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ReloadMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ElimMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ThrowGrenadeMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* SwapMontage;

	// 隐藏近距离角色的相机判定阈值与控制函数
	void HideCameraIfCharacterClose();

	UPROPERTY(EditAnywhere)
	float CameraThreshold = 200.f;

	bool bRotateRootBone;
	float TurnThreshold = 0.5f;
	FRotator ProxyRotationLastFrame;
	FRotator ProxyRotation;
	float ProxyYaw;
	float TimeSinceLastMovementReplication;
	float CalculateSpeed();

	// 新增：是否为第一人称视角（默认 false 表示第三人称）
	bool bIsFirstPerson = false;

	/*
	* 玩家生命值
	*/
	// 玩家生命与护盾（含 RepNotify）
	UPROPERTY(EditAnywhere, Category = "Player States")
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleAnywhere, Category = "Player States")
	float Health = 100.f;

	UFUNCTION()
	void OnRep_Health();
	

	// 记录本地 Controller 指针以便更新 HUD
	UPROPERTY()
	class ABlasterPlayerController* BlasterPlayerController;

	bool bElimmed = false;

	FTimerHandle ElimTimer;

	UPROPERTY(EditDefaultsOnly)
	float ElimDelay = 3.f;

	void ElimTimerFinished();

	bool bLeftGame = false;

	/*
	* 溶解效果
	*/

	// 溶解效果相关 Timeline 与材质
	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;
	FOnTimelineFloat DissolveTrack;

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;

	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	void StartDissolve();

	// 运行时动态材质实例（用于渐变溶解）
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance;

	//角色材质实例
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance;
	
	/*
	* 淘汰
	*/
	UPROPERTY(EditAnywhere)
	UParticleSystem* ElimBotEffect;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* ElimBotComponent;

	UPROPERTY(EditAnywhere)
	class USoundCue* ElimBotSound;

	UPROPERTY()
	class ABlasterPlayerState* BlasterPlayerState;

	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* CrownSystem;

	UPROPERTY()
	class UNiagaraComponent* CrownComponent;

	/*
	* 手雷
	*/
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* AttachedGrenade;
	

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode;

public: 
	// 对外访问方法（大量 inline 注释）
	// SetOverlappingWeapon: 更新当前可拾取武器并在本地显示/隐藏提示
	void SetOverlappingWeapon(AWeapon* Weapon);
	bool IsWeaponEquipped();
	bool IsAiming();
	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
	AWeapon* GetEquippedWeapon();
	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }
	FVector GetHitTarget() const;
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE bool ShouldRotateRootBone() const { return bRotateRootBone; }
	FORCEINLINE bool IsElimed() const { return bElimmed; }
	// 下面是 Health / Shield 的访问器（用于其他类访问，而不是直接访问成员）
	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE void SetHealth(float Amount) { Health = Amount; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	FORCEINLINE UCombatComponent* GetCombat() const { return Combat; }
	FORCEINLINE bool GetDisableGameplay() const { return bDisableGameplay; }
	FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
	FORCEINLINE UStaticMeshComponent* GetAttachedGrenade() const { return AttachedGrenade; }
	FORCEINLINE ULagCompensationComponent* GetLagCompensation() const { return LagCompensation; }
	ECombatState GetCombatState() const;
	bool IsLocallyReloading();

	// 切换视角：V 键触发（在CPP中实现）
	UFUNCTION()
	void TogglePerspective();

	// 切换为第一人称的函数（供代码内调用）
	UFUNCTION()
	void SwitchToFirstPerson();

	// 切换回第三人称
	UFUNCTION()
	void SwitchToThirdPerson();
};
