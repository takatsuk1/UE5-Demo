// 本文件中文注释：
// BlasterCharacter.cpp - 角色实现文件
// 说明：
//  该文件实现 ABlasterCharacter 的构造与游戏行为，包含：
//   - 相机、组件初始化（构造函数）
//   - 生命周期函数（BeginPlay、Tick、Destroyed）
//   - 输入绑定（移动、跳跃、瞄准、开火、换弹、投掷手雷）
//   - 角色状态同步（Replicate、OnRep 回调）
//   - 受伤与淘汰逻辑（ReceiveDamage、Elim、MulticastElim）
//   - 动画蒙太奇播放封装（PlayFireMontage、PlayReloadMontage、等）
//   - AimOffset/TurnInPlace/RotateInPlace 相关的朝向与动画修正
//   - HITBOX 初始化（用于服务器回溯判定命中）
//
// 重要成员说明：
//   - Combat: 战斗组件，管理武器、开火、装弹等
//   - Buff: 增益/减益组件
//   - LagCompensation: 服务器回溯判定组件
//   - Health / Shield: 玩家生命与护盾（含 RepNotify 回调）
//   - DissolveTimeline: 溶解特效时间线（淘汰效果）
//
// 注：以下注释为说明性文档，保持原代码不变。

#include "BlasterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "BlasterLearing/Weapon/Weapon.h"
#include "BlasterLearing/BlasterComponent/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterAnimInstance.h"
#include "BlasterLearing/BlasterLearing.h"
#include "BlasterLearing/PlayerController/BlasterPlayerController.h"
#include "BlasterLearing/GameMode/BlasterGameMode.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "BlasterLearing/PlayerState/BlasterPlayerState.h"
#include "BlasterLearing/Weapon/WeaponTypes.h"
#include "Components/BoxComponent.h"
#include "BlasterLearing/BlasterComponent/LagCompensationComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BlasterLearing/GameState/BlasterGameState.h"
#include "InputCoreTypes.h"

ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 摄像机吊臂设置：将相机附着于骨骼网格（默认挂点），并允许控制器旋转相机
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	// 跟随相机：不随Pawn本身的控制旋转，以保持第三人称视角的常规表现
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 新增：第一人称相机（默认不启用）
	// 为了避免头部动画导致的摄像机剧烈晃动，最佳实践是将相机附着到胶囊体而不是头部插槽
	// 这样可以保持稳定的第一人称视角，同时随胶囊体移动和下蹲
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent()); 
	// 设置相机高度（通常为 BaseEyeHeight，这里手动设置一个近似值，例如 64.f 或根据模型调整）
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight)); 
	FirstPersonCamera->bUsePawnControlRotation = true;
	
	// 默认禁用第一人称相机
	FirstPersonCamera->SetActive(false);
	FirstPersonCamera->SetVisibility(false);

	// 确保 FollowCamera 默认启用
	FollowCamera->SetActive(true);
	FollowCamera->SetVisibility(true);

	// 不使用控制器直接控制角色 Yaw，而是通过 Movement 旋转控制（根据装备武器会切换）
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// 新增：默认使用第三人称视角
	bIsFirstPerson = false;

	// 叠加Widget（例如玩家名牌）
	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	// Combat 与 Buff 组件创建，并设置为可复制（重要：影响网络同步）
	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	// Lag Compensation 组件（服务器回放命中使用）
	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));

	// 允许下蹲，以及设置碰撞通道忽略相机，以免相机被角色自身阻挡
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	// 控制旋转速率（影响朝向插值）
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);

	// 初始转身状态与网络更新频率
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	// 溶解时间轴组件（用于淘汰效果）
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	// 绑定手雷静态网格并关闭碰撞（投掷时再显示）
	AttachedGrenade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Attached Grenade"));
	AttachedGrenade->SetupAttachment(GetMesh(), FName("GrenadeSocket"));
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/*
	* Hit Boxes 初始化（用于服务器回放/回溯命中判定）
	* 每个命中盒附着到对应骨骼，并添加到 HitCollisionBoxes 映射中
	*/

	head = CreateDefaultSubobject<UBoxComponent>(TEXT("head"));
	head->SetupAttachment(GetMesh(), FName("head"));
	HitCollisionBoxes.Add(FName("head"), head);

	pelvis = CreateDefaultSubobject<UBoxComponent>(TEXT("pelvis"));
	pelvis->SetupAttachment(GetMesh(), FName("pelvis"));
	HitCollisionBoxes.Add(FName("pelvis"), pelvis);

	spine_02 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_02"));
	spine_02->SetupAttachment(GetMesh(), FName("spine_02"));
	HitCollisionBoxes.Add(FName("spine_02"), spine_02);

	spine_03 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_03"));
	spine_03->SetupAttachment(GetMesh(), FName("spine_03"));
	HitCollisionBoxes.Add(FName("spine_03"), spine_03);

	upperarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_l"));
	upperarm_l->SetupAttachment(GetMesh(), FName("upperarm_l"));
	HitCollisionBoxes.Add(FName("upperarm_l"), upperarm_l);

	upperarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_r"));
	upperarm_r->SetupAttachment(GetMesh(), FName("upperarm_r"));
	HitCollisionBoxes.Add(FName("upperarm_r"), upperarm_r);

	lowerarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_l"));
	lowerarm_l->SetupAttachment(GetMesh(), FName("lowerarm_l"));
	HitCollisionBoxes.Add(FName("lowerarm_l"), lowerarm_l);

	lowerarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_r"));
	lowerarm_r->SetupAttachment(GetMesh(), FName("lowerarm_r"));
	HitCollisionBoxes.Add(FName("lowerarm_r"), lowerarm_r);

	hand_l = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_l"));
	hand_l->SetupAttachment(GetMesh(), FName("hand_l"));
	HitCollisionBoxes.Add(FName("hand_l"), hand_l);

	hand_r = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_r"));
	hand_r->SetupAttachment(GetMesh(), FName("hand_r"));
	HitCollisionBoxes.Add(FName("hand_r"), hand_r);

	blanket = CreateDefaultSubobject<UBoxComponent>(TEXT("blanket"));
	blanket->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("blanket"), blanket);

	backpack = CreateDefaultSubobject<UBoxComponent>(TEXT("backpack"));
	backpack->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("backpack"), backpack);

	thigh_l = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_l"));
	thigh_l->SetupAttachment(GetMesh(), FName("thigh_l"));
	HitCollisionBoxes.Add(FName("thigh_l"), thigh_l);

	thigh_r = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_r"));
	thigh_r->SetupAttachment(GetMesh(), FName("thigh_r"));
	HitCollisionBoxes.Add(FName("thigh_r"), thigh_r);

	calf_l = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_l"));
	calf_l->SetupAttachment(GetMesh(), FName("calf_l"));
	HitCollisionBoxes.Add(FName("calf_l"), calf_l);

	calf_r = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_r"));
	calf_r->SetupAttachment(GetMesh(), FName("calf_r"));
	HitCollisionBoxes.Add(FName("calf_r"), calf_r);

	foot_l = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_l"));
	foot_l->SetupAttachment(GetMesh(), FName("foot_l"));
	HitCollisionBoxes.Add(FName("foot_l"), foot_l);

	foot_r = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_r"));
	foot_r->SetupAttachment(GetMesh(), FName("foot_r"));
	HitCollisionBoxes.Add(FName("foot_r"), foot_r);

	for (auto& Box : HitCollisionBoxes)
	{
		if (Box.Value)
		{
			// 将命中盒设置为专用碰撞通道，默认关闭碰撞（按需打开）
			Box.Value->SetCollisionObjectType(ECC_HitBox);
			Box.Value->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
			Box.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			Box.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate 特定属性（例如 OverlappingWeapon 只给 Owner 可见）
	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(ABlasterCharacter, Health);
	DOREPLIFETIME(ABlasterCharacter, bDisableGameplay);
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	// 当角色的 ReplicatedMovement 更新时，同步模拟代理的转身处理
	SimProxiesTurn();
	// 重置计时器以避免短时间内重复强制同步
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::Elim(bool bPlayerLeftGame)
{
	// 淘汰入口：先掉落或销毁武器（服务器逻辑），然后广播 MulticastElim 给客户端播放特效/禁用控制
	DropOrDestroyWeapons();
	MulticastElim(bPlayerLeftGame);
}

void ABlasterCharacter::MulticastElim_Implementation(bool bPlayerLeftGame)
{
	// Multicast 在所有客户端执行的淘汰效果（例如播放溶解、粒子、声音等）
	bLeftGame = bPlayerLeftGame;
	if (BlasterPlayerController)
	{
		// 将 HUD 上武器子弹显示置 0
		BlasterPlayerController->SetHUDWeaponAmmo(0);
	}
	bElimmed = true;
	PlayElimMontage();
	// 开始溶解材质的动态实例与时间线（仅当有材质时）
	if (DissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();

	// 禁用角色动作与碰撞以防止继续参与游戏
	bDisableGameplay = true;
	GetCharacterMovement()->DisableMovement();
	if (Combat)
	{
		// 停止开火输入
		Combat->FireButtonPressed(false);
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 生成淘汰粒子与音效（例如机器人消失）
	if (ElimBotEffect)
	{
		FVector ElimBotSpawnPoint(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z + 200.f);
		ElimBotComponent = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ElimBotEffect,
			ElimBotSpawnPoint,
			GetActorRotation()
		);
	}
	if (ElimBotSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(
			this,
			ElimBotSound,
			GetActorLocation()
		);
	}

	// 销毁皇冠粒子（如果存在）
	if (CrownComponent)
	{
		CrownComponent->DestroyComponent();
	}
	// 启动淘汰计时器，计时结束后执行回收或重生逻辑
	GetWorldTimerManager().SetTimer(
		ElimTimer,
		this,
		&ABlasterCharacter::ElimTimerFinished,
		ElimDelay
	);
}

void ABlasterCharacter::ElimTimerFinished()
{
	// 淘汰计时结束：根据是否离开游戏决定回收还是广播离开事件
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	if (BlasterGameMode && !bLeftGame)
	{
		BlasterGameMode->RequestRespawn(this, Controller);
	}
	if (bLeftGame && IsLocallyControlled())
	{
		OnLeftGame.Broadcast();
	}
}

void ABlasterCharacter::ServerLeaveGame_Implementation()
{
	// 服务端处理玩家离开游戏，通知 GameMode
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	BlasterPlayerState = BlasterPlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;
	if (BlasterGameMode && BlasterPlayerState)
	{
		BlasterGameMode->PlayerLeftGame(BlasterPlayerState);
	}
}

void ABlasterCharacter::DropOrDestroyWeapon(AWeapon* Weapon)
{
	// 安全检查：如果武器为空直接返回
	if (Weapon == nullptr) return;
	// 如果标记为需要销毁则直接 Destroy，否则调用 Dropped 让武器进入掉落状态
	if (Weapon->bDestroyWeapon)
	{
		Weapon->Destroy();
	}
	else
	{
		Weapon->Dropped();
	}
}

void ABlasterCharacter::DropOrDestroyWeapons()
{
	// 逐个处理装备武器、备用武器、以及旗子（如果携带）
	if (Combat)
	{
		if (Combat->EquippedWeapon)
		{
			DropOrDestroyWeapon(Combat->EquippedWeapon);
		}
		if (Combat->SecondaryWeapon)
		{
			DropOrDestroyWeapon(Combat->SecondaryWeapon);
		}

	}
}

void ABlasterCharacter::Destroyed()
{
	Super::Destroyed();

	// 清除淘汰机器人组件（如果存在）
	if (ElimBotComponent)
	{
		ElimBotComponent->DestroyComponent();
	}

	// 如果比赛还未开始并且服务端存在装备武器，则销毁武器以避免残留
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	bool bMatchNotInProgress = BlasterGameMode && BlasterGameMode->GetMatchState() != MatchState::InProgress;
	if (Combat && Combat->EquippedWeapon && bMatchNotInProgress)
	{
		Combat->EquippedWeapon->Destroy();
	}
}

void ABlasterCharacter::MulticastGainedTheLead_Implementation()
{
	// 获得领先时生成皇冠特效并激活
	if (CrownSystem == nullptr) return;
	if (CrownComponent == nullptr)
	{
		CrownComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			CrownSystem,
			GetMesh(),
			FName(),
			GetActorLocation() + FVector(0.f, 0.f, 110.f),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false
		);
	}
	if (CrownComponent)
	{
		CrownComponent->Activate();
	}
}

void ABlasterCharacter::MulticastLostTheLead_Implementation()
{
	// 失去领先后销毁皇冠特效
	if (CrownComponent)
	{
		CrownComponent->DestroyComponent();
	}
}


void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// 仅服务器绑定受伤回调，用于处理伤害逻辑（包括 Shield/Health 变化）
	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::ReceiveDamage);
	}
	// 初始把手雷隐藏，投掷时再显示
	if (AttachedGrenade)
	{
		AttachedGrenade->SetVisibility(false);
	}
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 每帧处理转身、相机遮挡隐藏以及延迟初始化（如 PlayerState/Controller）
	RotateInPlace(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}

void ABlasterCharacter::RotateInPlace(float DeltaTime)
{

	// 装备武器时改为角色朝向受控制器驱动，不再根据移动方向自动旋转
	if (Controller && Controller->IsA<APlayerController>())
	{
		// 装备武器时改为角色朝向受控制器驱动，不再根据移动方向自动旋转
		if (Combat && Combat->EquippedWeapon) 
		{
						GetCharacterMovement()->bOrientRotationToMovement = false;
			bUseControllerRotationYaw = true;
		}
	}
	// 如果禁用玩法（例如已淘汰），则重置转身状态
	if (bDisableGameplay)
	{
		bUseControllerRotationYaw = false;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
	// 本地控制器角色处理 AimOffset（本地更精确）
	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled())
	{
			AimOffset(DeltaTime);
	}
	else
	{
		// 代理（非本地）使用网络复制的移动信息推断转身
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f)
		{
			OnRep_ReplicatedMovement();
		}
		// 计算俯仰角 AO_Pitch（使用复制的 BaseAimRotation）
		CalculateAO_Pitch();
	}
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 绑定按键/轴输入到对应函数（解释：IE_Pressed/Released 为输入事件类型）
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlasterCharacter::Jump);

	PlayerInputComponent->BindAxis("MoveForward", this, &ABlasterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABlasterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ABlasterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ABlasterCharacter::LookUp);

	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ABlasterCharacter::EquipButtonPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ABlasterCharacter::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ABlasterCharacter::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ABlasterCharacter::AimButtonReleased);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABlasterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ABlasterCharacter::FireButtonReleased);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ABlasterCharacter::ReloadButtonPressed);
	PlayerInputComponent->BindAction("ThrowGrenade", IE_Pressed, this, &ABlasterCharacter::GrenadeButtonPressed);

	// 绑定 V 键切换视角（TogglePerspective）
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ABlasterCharacter::TogglePerspective);
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// 将角色引用传递给组件（Combat/Buff/LagCompensation）以便组件内可以访问角色属性
	if (Combat)
	{
		Combat->Character = this;
	}

	if (LagCompensation)
	{
		LagCompensation->Character = this;
		if (Controller)
		{
			// 将控制器转换为专用的 BlasterPlayerController，便于拉取回放信息
			LagCompensation->Controller = Cast<ABlasterPlayerController>(Controller);
		}
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	// 如果没有武器或 Combat 为空则不应该播放射击蒙太奇
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		// 播放蒙太奇并跳转到对应段（瞄准/非瞄准对应不同的动画帧）
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayReloadMontage()
{
	// 装弹蒙太奇会根据当前装备的武器类型选择正确的段落（Rifle / Pistol / Sniper 等）
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadMontage)
	{
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;

		switch (Combat->EquippedWeapon->GetWeaponType())
		{
		case EWeaponType::EWT_AssaultRifle:
			SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_RocketLauncher:
			SectionName = FName("RocketLauncher");
			break;
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_Shotgun:
			SectionName = FName("Shotgun");
			break;
		case EWeaponType::EWT_GrenadeLauncher:
			SectionName = FName("GrenadeLauncher");
			break;
		default:
			// 默认使用 Rifle 段落以保证不会出现未处理情况
			SectionName = FName("Rifle");
			break;
		}

		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayElimMontage()
{
	// 播放淘汰动画蒙太奇
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}

void ABlasterCharacter::PlayThrowGrenadeMontage()
{
	// 播放投掷手雷动画蒙太奇
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ThrowGrenadeMontage)
	{
		AnimInstance->Montage_Play(ThrowGrenadeMontage);
	}
}

void ABlasterCharacter::PlaySwapMontage()
{
	// 播放换枪动作蒙太奇
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && SwapMontage)
	{
		AnimInstance->Montage_Play(SwapMontage);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	// 受到伤害时的击中反应动画（仅当装备武器时触发）
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::GrenadeButtonPressed()
{
	// 投掷手雷按键逻辑：如果携带旗子或禁用玩法则忽略
	if (Combat)
	{
		Combat->ThrowGrenade();
	}
}

void ABlasterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	// 服务器端的受伤处理：计算最终伤害、先扣护盾后扣生命、更新 HUD、播放反应动画、处理淘汰
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	// 已淘汰或 GameMode 未初始化时不处理
	if (bElimmed || BlasterGameMode == nullptr) return;
	// 调用 GameMode 的 CalculateDamage 来根据游戏规则（如团队伤害/远程惩罚）调整伤害值
	// Damage = BlasterGameMode->CalculateDamage(InstigatorController, Controller, Damage);

	float DamageToHealth = Damage;

	// 扣减健康值并保证在合理范围内
	Health = FMath::Clamp(Health - DamageToHealth, 0.f, MaxHealth);

	// 更新 HUD 上的生命与护盾显示，并播放受击动画
	UpdateHUDHealth();
	//UpdateHUDShield();
	PlayHitReactMontage();

	// 若生命降到 0，则通知 GameMode 处理淘汰逻辑（计分/重生等）
	if (Health == 0.f)
	{
		if (BlasterGameMode)
		{
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
			ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatorController);
			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController);
		}
	}
}

void ABlasterCharacter::MoveForward(float Value)
{
	// 前后移动：如果被禁用则忽略；否则使用控制器的Yaw方向来计算沿 X 轴方向的移动向量
	if (bDisableGameplay) return;
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	// 左右移动，类似 MoveForward，只是使用 Y轴方向
	if (bDisableGameplay) return;
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::Turn(float Value)
{
	// 鼠标/摇杆水平转向输入，直接交给控制器处理
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
	// 鼠标/摇杆垂直视角输入，直接交给控制器处理
	AddControllerPitchInput(Value);
}

void ABlasterCharacter::EquipButtonPressed()
{
	// 拾取/切换武器按键逻辑（受 Combat 组件控制）
	if (bDisableGameplay) return;
	if (Combat)
	{
		// 如果当前未被占用则向服务器请求拾取武器（ServerEquipButtonPressed）
		if (Combat->CombatState == ECombatState::ECS_Unoccuiped) ServerEquipButtonPressed();
		bool bSwap = Combat->ShouldSwapWeapons() &&
			!HasAuthority() &&
			Combat->CombatState == ECombatState::ECS_Unoccuiped &&
			OverlappingWeapon == nullptr;

		if (bSwap)
		{
			// 本地播放换枪蒙太奇，并把状态设为正在交换
			PlaySwapMontage();
			Combat->CombatState = ECombatState::ECS_SwappingWeapon;
			bFinishedSwapping = false;
		}
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	// 服务端真正执行装备/切换逻辑：如果有重叠武器則拾取，否则判断是否需要交换
	if (Combat)
	{
		if (OverlappingWeapon)
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else if (Combat->ShouldSwapWeapons())
		{
			Combat->SwapWeapons();
		}
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	if (bDisableGameplay) return;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABlasterCharacter::ReloadButtonPressed()
{
	// 请求换弹（由 Combat 组件处理，组件内部会做权限/状态判断）
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->Reload();
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	// 瞄准开始：设置 Combat 组件的瞄准状态
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased()
{
	// 瞄准结束
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->SetAiming(false);
	}
}

float ABlasterCharacter::CalculateSpeed()
{
	// 计算平面速度（剔除 Z 分量），用于动画与判定
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	// AimOffset 逻辑：根据速度与是否在空中决定是否进入转身逻辑（TurnInPlace）
	if (Combat && Combat->EquippedWeapon == nullptr) return;
	float Speed = CalculateSpeed();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	// 静止且不在空中：进入站立转身判断
	if (Speed == 0.f && !bIsInAir) // 静止不动，不跳跃
	{
		// 启用根骨骼旋转，这样角色朝向会跟随控制器旋转
		bRotateRootBone = true;
		// 当前瞄准旋转（仅 Yaw）
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		// 计算相对起始瞄准方向的差值（归一化）
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			// 当未处于转身动画时，初始化插值值以保持平滑
			InterpAO_Yaw = AO_Yaw;
		}
		// 始终允许控制器旋转 Yaw（确保角色面向控制器视角）
		if(IsPlayerControlled()) 
		{
			bUseControllerRotationYaw = true;
		}
		// 执行转身判定与处理（会改变 TurningInPlace 与 AO_Yaw）
		TurnInPlace(DeltaTime);
	}
	// 移动或者在空中：重置与移动相关的 AimOffset 状态
	if (Speed > 0.f || bIsInAir) // 跑或者跳
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		if(IsPlayerControlled()) 
		{
			bUseControllerRotationYaw = true;
		}
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	// 计算俯仰角（客户端/服务器需要不同映射）
	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	// 获取基本瞄准俯仰角（BaseAimRotation.Pitch）
	AO_Pitch = GetBaseAimRotation().Pitch;
	// 在远端客户端上当 Pitch 在 [270,360) 时映射到 [-90,0)
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		// 将 map 值从 [270, 360) 转换为 [-90, 0)
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::SimProxiesTurn()
{
	// 非本地代理的转身判断：当速度为0时比较 ProxyRotation 的变化，超过阈值即设置转身
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	bRotateRootBone = false;
	float Speed = CalculateSpeed();
	if (Speed > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	// 记录上帧与当前帧的 ProxyRotation 来计算 Yaw 变化
	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	// 若偏差超过阈值则判定为左/右转身
	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		if (ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

}

void ABlasterCharacter::Jump()
{
	// 跳跃逻辑：携旗或禁用时不允许跳跃；蹲下时先站起再跳
	if (bDisableGameplay) return;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	// 开火按下：传递给 Combat 组件（组件内部处理射击节流/子弹发射）
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->FireButtonPressed(true);
	}
}

void ABlasterCharacter::FireButtonReleased()
{
	// 开火释放：停止持续开火（对于自动武器）
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	// 根据 AO_Yaw 的大小判断是否需要播放转身动画（超过 +/-90 度）
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		// 平滑插值 AO_Yaw 回到 0（表示完成转身），插值速率为 4.f
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		// 当角度足够小（<15）时，认为转身完成并重置起始瞄准旋转
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void ABlasterCharacter::HideCameraIfCharacterClose()
{
	// 当相机与角色距离过近时隐藏角色网格（避免穿模），并且设置武器的 OwnerNoSee
	if (!IsLocallyControlled()) return;

	// // 如果当前为第一人称模式，强制隐藏角色网格并隐藏第三人称武器网格
	// if (bIsFirstPerson)
	// {
	// 	GetMesh()->SetVisibility(false);
	// 	if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
	// 	{
	// 		// 隐藏第三人称武器模型，第一人称通常使用独立模型
	// 		Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
	// 	}
	// 	if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
	// 	{
	// 		Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = true;
	// 	}
	// 	return;
	// }

	// 使用 FollowCamera 的位置判断是否需要隐藏（原有逻辑）
	if (!FollowCamera) return;

 	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
 	{
 		GetMesh()->SetVisibility(false);
 		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
 		{
 			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
 		}
 		if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
 		{
 			Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = true;
 		}
 	}
 	else
 	{
 		// 恢复显示与武器可见性
 		GetMesh()->SetVisibility(true);
 		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
 		{
 			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
 		}
 		if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
 		{
 			Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = false;
 		}
 	}
}

void ABlasterCharacter::OnRep_Health()
{
	// Health RepNotify：当客户端收到 Health 变化时更新 HUD，并在受到伤害时播放被击动画
	UpdateHUDHealth();
		PlayHitReactMontage();
	
}


void ABlasterCharacter::UpdateHUDHealth()
{
	// 通过缓存的 BlasterPlayerController 更新 HUD（若为空则尝试从 Controller 获取）
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::UpdateHUDAmmo()
{
	// 更新弹药显示（携带量与当前武器弹匣）
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController && Combat && Combat->EquippedWeapon)
	{
		BlasterPlayerController->SetHUDCarriedAmmo(Combat->CarriedAmmo);
		BlasterPlayerController->SetHUDWeaponAmmo(Combat->EquippedWeapon->GetAmmo());
	}
}

void ABlasterCharacter::UpdateHUDGrenade()
{
	// 更新手雷数量显示
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDGrenades(Combat->GetGrenades());
	}
}

void ABlasterCharacter::PollInit()
{
	// 延迟初始化 PlayerState 与 Controller（可能在 BeginPlay 时尚不可用）
	if (BlasterPlayerState == nullptr)
	{
		BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
		if (BlasterPlayerState)
		{

			ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
			if (BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(BlasterPlayerState))
			{
				MulticastGainedTheLead();
			}
		}
	}
	if (BlasterPlayerController == nullptr)
	{
		BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
		if (BlasterPlayerController)
		{
			// 初始化 HUD 显示
			UpdateHUDAmmo();
			UpdateHUDHealth();
			UpdateHUDGrenade();
		}
	}
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	// 溶解曲线回调：将曲线值设置到动态材质参数中
	if (DynamicDissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void ABlasterCharacter::StartDissolve()
{
	// 绑定时间线曲线回调并播放（以产生溶解效果）
	DissolveTrack.BindDynamic(this, &ABlasterCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	// 当新的可拾取武器设置时，显示/隐藏拾取提示（只在本地控制器显示）
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	// 当 OverlappingWeapon 在客户端复制时，更新提示显示（收到新武器则显示，旧武器隐藏）
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

bool ABlasterCharacter::IsWeaponEquipped()
{
	// 判断是否有装备武器（Combat 组件管理）
	return (Combat && Combat->EquippedWeapon);
}

bool ABlasterCharacter::IsAiming()
{
	// 判断是否处于瞄准状态
	return (Combat && Combat->bAiming);
}

AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if (Combat == nullptr) return FVector();
	return Combat->HitTarget;
}

ECombatState ABlasterCharacter::GetCombatState() const
{
	if (Combat == nullptr) return ECombatState::ECS_MAX;
	return Combat->CombatState;
}

bool ABlasterCharacter::IsLocallyReloading()
{
	if (Combat == nullptr) return false;
	return Combat->bLocallyReloading;
}

// 切换视角实现：在本地控制器上切换 First/Third person
void ABlasterCharacter::TogglePerspective()
{
	if (!IsLocallyControlled()) return;

	bIsFirstPerson = !bIsFirstPerson;
	if (bIsFirstPerson)
	{
		SwitchToFirstPerson();
	}
	else
	{
		// 切换回第三人称
		SwitchToThirdPerson();
	}
}

void ABlasterCharacter::SwitchToFirstPerson()
{
	// 仅本地控制器执行相机可见性调整
	if (!IsLocallyControlled()) return;

	// 关闭第三人称 FollowCamera，启用第一人称 FirstPersonCamera
	if (FollowCamera)
	{
		FollowCamera->SetActive(false);
		FollowCamera->SetVisibility(false);
	}
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(true);
		FirstPersonCamera->SetVisibility(true);
	}

	// 第一人称下使用控制器驱动角色朝向，让摄像头（控制器）控制角色旋转
	bUseControllerRotationYaw = true;
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}

	// 确保角色网格可见（Body Awareness）
	GetMesh()->SetVisibility(true);
	GetMesh()->SetOwnerNoSee(false);

	// 可选：为了防止第一人称看到头部内部穿模，可以隐藏头部骨骼
	// GetMesh()->HideBoneByName(FName("head"), EPhysBodyOp::PBO_None);

	if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
	{
		// 确保武器可见
		Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
	}
	if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
	{
		Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = false;
	}
}

void ABlasterCharacter::SwitchToThirdPerson()
{
	if (!IsLocallyControlled()) return;

	// 启用 FollowCamera，禁用 FirstPersonCamera
	if (FollowCamera)
	{
		FollowCamera->SetActive(true);
		FollowCamera->SetVisibility(true);
	}
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(false);
		FirstPersonCamera->SetVisibility(false);
	}

	// 切回第三人称，恢复基于移动的朝向并禁用控制器直接控制 Yaw
	bUseControllerRotationYaw = false;
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	// 显示角色网格并恢复武器可见性
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetVisibility(true);

	// if (FirstPersonMesh)
	// {
	// 	FirstPersonMesh->SetVisibility(false);
	// }

	if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
	{
		Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
	}
	if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
	{
		Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = false;
	}
}
