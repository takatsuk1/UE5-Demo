// 本文件中文注释：
// BlasterAnimInstance.cpp - 动画实例实现
// 说明：
//  该文件实现了 UBlasterAnimInstance 的初始化与每帧更新逻辑，
//  用于从角色（ABlasterCharacter）获取信息并驱动动画蓝图中使用的参数。
//  主要职责：
//   - 在 NativeInitializeAnimation 中缓存角色引用
//   - 在 NativeUpdateAnimation 中计算速度、是否在空中、是否加速、是否装备武器、蹲下、瞄准等状态
//   - 计算偏航偏移（YawOffset）、身体倾斜（Lean）、左右手姿态与FABRIK/AimOffset使用条件
//  重要变量说明（在动画蓝图中曝光用于Blend等）：
//   - Speed: 平面速度（剔除Z分量）
//   - bIsInAir: 是否处于空中（跳跃/下落）
//   - bIsAccelerating: 当前是否有加速度（玩家输入）
//   - bWeaponEquipped / EquippedWeapon: 是否装备武器及武器引用
//   - YawOffset: 角色朝向与移动方向之间的Yaw偏差，用于移动/射击动画偏移
//   - Lean: 根据角色旋转速度计算的身体侧倾度
//   - LeftHandTransform / RightHandRotation: 双手IK相关，用于将左手对齐武器握点及右手注视目标
//   - bUseFABRIK / bUseAimOffsets / bTransformRightHand: 控制是否启用IK与AimOffset
//
// 注：以下注释仅用于说明，每处具体实现保持原样，不对代码逻辑做任何修改。

#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterLearing/Weapon/Weapon.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 尝试从动画实例的拥有者（Pawn）获取角色指针，并缓存到 BlasterCharacter
	// 说明：TryGetPawnOwner 返回 APawn*，在多人情况下有可能为 nullptr，因此需要检查
	BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	// 每帧尝试确保我们有角色指针（首次可能为 nullptr，因为Pawn尚未初始化）
	if (BlasterCharacter == nullptr)
	{
		// 再次尝试获取角色指针
		BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
	}
	// 如果仍然没有角色则直接返回（不可继续计算动画参数）
	if (BlasterCharacter == nullptr) return;

	// 获取角色速度并剔除垂直分量（Z），用于动画的平面速度判断（BlendSpace）
	FVector Velocity = BlasterCharacter->GetVelocity();
	Velocity.Z = 0.f;
	Speed = Velocity.Size(); // 平面速度大小

	// 判断是否在空中（跳跃或下落）
	bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();

	// 判断当前是否有加速度（是否有按下移动键）
	// GetCurrentAcceleration 返回当前加速度向量，Size()>0 表示正在输入移动
	bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;

	// 是否装备武器（由角色的 Combat 组件管理）
	bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();
	// 缓存装备的武器指针（用于后续 IK/对齐计算）
	EquippedWeapon = BlasterCharacter->GetEquippedWeapon();

	// 蹲下/瞄准/转身/根骨骼旋转/淘汰/携旗等状态，直接从角色获取
	bIsCrouched = BlasterCharacter->bIsCrouched;
	bAiming = BlasterCharacter->IsAiming();
	TurningInPlace = BlasterCharacter->GetTurningInPlace();
	bRotateRootBone = BlasterCharacter->ShouldRotateRootBone();
	bElimmed = BlasterCharacter->IsElimed();

	// 计算移动方向与朝向之间的偏差（YawOffset）
	// AimRotation: 角色的朝向（玩家视角基准）
	FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();
	// MovementRotation: 由速度向量构建的朝向（单位X方向）
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());
	// 归一化的两个旋转之间的差值（[-180,180]）
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	// 平滑插值 DeltaRotation，以避免抖动
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 6.f);
	// 最终用来驱动动画混合的偏航差
	YawOffset = DeltaRotation.Yaw;

	// 计算 Lean（身体侧倾），基于角色每帧朝向变化的角速度
	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = BlasterCharacter->GetActorRotation();
	// 计算当前帧的旋转差（归一化）
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	// 目标值为每秒的Yaw变化速率（角速度）
	const float Target = Delta.Yaw / DeltaTime;
	// 使用插值平滑 Lean 的变化，系数 6.f 控制平滑速度
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);
	// 限制 Lean 范围，避免数值过大
	Lean = FMath::Clamp(Interp, -90.f, 90.f);

	// 直接从角色获取 Aim Offset 的 yaw/pitch（角色端已计算）
	AO_Yaw = BlasterCharacter->GetAO_Yaw();
	AO_Pitch = BlasterCharacter->GetAO_Pitch();

	// 如果有武器并且武器网格与角色网格可用，则计算左右手的 IK 对齐
	if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && BlasterCharacter->GetMesh())
	{
		// 获取武器的左手插槽在世界空间的变换（LeftHandSocket）
		LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
		// 将世界空间左手位置转换到角色骨骼的骨骼空间（hand_r）以便动画蓝图使用
		FVector OutPosition;
		FRotator OutRotation;
		BlasterCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation);
		// 更新 LeftHandTransform 的位置和旋转为骨骼空间值（储存用于动画蓝图）
		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation));

		// 如果是本地控制器，需要让右手朝向命中目标（用于本地看的瞄准对齐）
		if (BlasterCharacter->IsLocallyControlled())
		{
			bLocallyController = true;
			// 获取武器右手插槽世界变换
			FTransform RightHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("Hand_R"), ERelativeTransformSpace::RTS_World);
			// 计算右手应朝向的位置（当前右手到命中目标方向）
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
				RightHandTransform.GetLocation(),
				RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - BlasterCharacter->GetHitTarget())
			);
			// 平滑插值右手朝向，插值速率较高（30.f）以获得快速但平滑的跟踪
			RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaTime, 30.f);
		}

	}

	// bUseFABRIK: 在非投掷手雷且未处于装弹、本地优先控制时，是否启用 FABRIK IK
	bUseFABRIK = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccuiped;
	bool bFABRIKOverride = BlasterCharacter->IsLocallyControlled() && 
		BlasterCharacter->GetCombatState() != ECombatState::ECS_ThrowingGrenade && 
		BlasterCharacter->bFinishedSwapping;
	if (bFABRIKOverride)
	{
		// 如果本地正在装弹则不使用 FABRIK
		bUseFABRIK = !BlasterCharacter->IsLocallyReloading();
	}
	// AimOffsets 与 RightHand 变换仅在未处于特殊战斗状态且未禁用玩法时启用
	bUseAimOffsets = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccuiped && !BlasterCharacter->GetDisableGameplay();
	bTransformRightHand = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccuiped && !BlasterCharacter->GetDisableGameplay();
}
