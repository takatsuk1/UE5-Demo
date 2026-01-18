// 文件说明：定义武器相关的常量与自定义枚举，用于全局引用（Trace 长度、CustomDepth 描边值、武器类型枚举等）
// TRACE_LENGTH 为射线最大延展长度，CUSTOM_DEPTH_* 用于渲染层的自定义深度标识（描边颜色区分）。

#pragma once

#define TRACE_LENGTH 80000.f // 射线的最大长度，用于计算散射的远端点

#define CUSTOM_DEPTH_PURPLE 250 // 自定义深度值：紫色（用于材质/描边区分）
#define CUSTOM_DEPTH_BLUE 251   // 自定义深度值：蓝色
#define CUSTOM_DEPTH_TAN 252    // 自定义深度值：棕色

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	EWT_AssaultRifle UMETA(DisplayName = "Assault Rifle"),
	EWT_RocketLauncher UMETA(DisplayName = "Rocket Launcher"),
	EWT_Pistol UMETA(DisplayName = "Pistol"),
	EWT_Shotgun UMETA(DisplayName = "Shotgun"),
	EWT_GrenadeLauncher UMETA(DisplayName = "Grenade Launcher"),

	EWT_MAX UMETA(DisplayName = "DefaultMAX")
};