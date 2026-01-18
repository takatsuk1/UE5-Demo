// 本文件中文注释：
// BlasterAnimInstance.h - 动画实例头文件
// 说明：
//  该头文件声明了 UBlasterAnimInstance，用于在动画蓝图与角色之间同步状态。
//  主要接口：
//   - NativeInitializeAnimation: 初始化动画实例时调用，用于缓存角色指针
//   - NativeUpdateAnimation: 每帧更新动画参数（Speed、bIsInAir、YawOffset 等）
//  重要属性（已用 UPROPERTY 暴露给动画蓝图）：
//   - BlasterCharacter: 目标角色引用（私有）
//   - Speed / bIsInAir / bIsAccelerating: 移动状态
//   - bWeaponEquipped / EquippedWeapon: 武器相关
//   - YawOffset / Lean / AO_Yaw / AO_Pitch: 用于AimOffset与移动混合
//   - LeftHandTransform / RightHandRotation: 用于双手IK与武器对齐
//   - bUseFABRIK / bUseAimOffsets / bTransformRightHand: 控制是否启用IK与AimOffset
//
// 注：此处仅为中/英说明注释，保持原始代码及修饰器不变。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BlasterLearing/BlasterTypes/TurningInPlace.h"
#include "BlasterLearing/BlasterTypes/CombatState.h"
#include "BlasterAnimInstance.generated.h"

UCLASS()
class BLASTERLEARING_API UBlasterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	// NativeInitializeAnimation: 动画实例初始化时调用
	// 目的：缓存当前持有该动画实例的Pawn（通常为角色），便于后续每帧从角色读取状态
	// 注意：这里只做指针缓存，不进行昂贵的运算
	virtual void NativeInitializeAnimation() override;

	// NativeUpdateAnimation: 每帧调用，用于从角色获取信息并更新动画蓝图所需的参数
	// 参数：
	//  - DeltaTime: 自上次帧更新以来经过的时间（秒）
	// 逻辑：计算速度、是否在空中、是否加速、武器状态、AimOffset、Lean等
	virtual void NativeUpdateAnimation(float DeltaTime) override;

private:
	// 指向拥有该动画实例的角色（ABlasterCharacter）
	// AllowPrivateAccess=true 允许动画蓝图读取该私有属性
	UPROPERTY(BlueprintReadOnly, Category = Character, meta = (AllowPrivateAccess = "true"))
	class ABlasterCharacter* BlasterCharacter;

	// Speed: 去掉Z分量后的平面移动速度（用于BlendSpace）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	float Speed;

	// bIsInAir: 是否处于空中（跳跃或下落）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bIsInAir;

	// 是否在加速（当前是否有输入加速度）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bIsAccelerating;

	// 是否装备武器（用于控制某些动画分支）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bWeaponEquipped;

	// 指向当前装备的武器（非UPROPERTY，内部使用）
	class AWeapon* EquippedWeapon;

	// 是否蹲下（由角色状态驱动）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bIsCrouched;

	// 是否瞄准（影响AimOffset/Aim相关动画）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bAiming;

	// YawOffset: 朝向偏差（移动方向与朝向的Yaw差），用于移动与射击混合
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	float YawOffset;

	// Lean: 根据角色旋转速度计算的倾斜量（用于在移动中呈现身体侧倾）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	float Lean;

	// 记录角色上一帧/当前帧朝向与差值（用于计算 Lean）
	FRotator CharacterRotationLastFrame;
	FRotator CharacterRotation;
	FRotator DeltaRotation;

	// Aim Offset 的 Yaw 与 Pitch（用于 AimOffset Blend）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	float AO_Yaw;

	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	float AO_Pitch;

	// 左手世界变换：用于双手IK，将左手对齐武器握持点
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	FTransform LeftHandTransform;

	// 转身状态（枚举：左/右/不转）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	ETurningInPlace TurningInPlace;

	// 右手朝向，用于在本地控制器时让右手朝向命中点（平滑插值）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	FRotator RightHandRotation;

	// 是否本地控制器（本地玩家），影响部分本地化 IK/对齐逻辑
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bLocallyController;

	// 是否旋转根骨骼（与转身/移动逻辑相关）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bRotateRootBone;

	// 是否已被淘汰（用于播放淘汰专用动画）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bElimmed;

	// 是否使用 FABRIK IK
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bUseFABRIK;

	// 是否使用 Aim Offsets
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bUseAimOffsets;

	// 是否对右手进行变换（武器对齐）
	UPROPERTY(BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	bool bTransformRightHand;
	
};
