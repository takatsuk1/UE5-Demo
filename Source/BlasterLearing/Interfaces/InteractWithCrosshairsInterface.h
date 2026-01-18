// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractWithCrosshairsInterface.generated.h"

// 文件说明（中文注释）：
// 该接口用于标记那些需要与游戏中十字准心（Crosshairs）交互的 Actor 或组件。
// 例如：武器、可拾取物体、目标指示器等可以实现该接口以响应 UI 层或射线检测得到的命中信息。
// 实现此接口的类通常需要在 Tick 或被射线检测到时，更新 HUD 上的瞄准提示（例如显示物体信息、交互提示等）。
//
// 使用建议：
// - 在实现类中添加一个接口函数（例如 OnCrosshairHover / OnCrosshairEndHover / GetCrosshairHitInfo 等）
//   并在被射线检测时由拥有射线逻辑的类（如 PlayerController 或 Character）调用。
// - 该头文件不强制定义具体函数签名，保持接口最小化有利于在不同类中按需扩展。
// - 注意：若在网络游戏中需要同步提示信息，请在拥有射线检测的客户端侧处理显示逻辑，避免不必要的网络负担。
// - 示例流程（仅示意，勿在此文件添加实现）：
//   1) PlayerController 做出射线检测（LineTrace），得到 Hit Actor / Hit Location。
//   2) 若 Hit Actor 实现了 IInteractWithCrosshairsInterface，则调用其自定义方法（例如 SetCrosshairFocus、ShowInteractWidget 等）。
//   3) 被命中的 Actor 根据本地状态在本地更新 UI（或通过 RPC 通知服务器需要的数据），并在玩家离开瞄准时清理显示。
//
// 注意事项：
// - 本接口文件仅作为标记接口（UInterface），不包含任何强制实现的方法签名，以保证最小侵入性。
// - 若需要强制实现统一的方法签名，可在此处添加纯虚函数声明；当前项目选择轻量化说明以便在蓝图/代码中灵活实现。

UINTERFACE(MinimalAPI)
class UInteractWithCrosshairsInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 接口说明（中文）：
 * - 继承此接口的类表示该对象可以与“十字准心”或“射线检测”交互/响应。
 * - 建议在实现类中添加并实现类似于以下的成员（仅为建议，非强制）：
 *     void OnCrosshairHover(FVector HitLocation);    // 当准心指向该对象时调用（本地显示）
 *     void OnCrosshairUnhover();                     // 当准心离开该对象时调用（清理显示）
 *     FText GetCrosshairDisplayText();               // 返回显示在 HUD 上的文字（例如物品名称）
 *
 * - 在多人环境下，尽量在客户端处理 UI 显示，只有必要时再向服务器请求或同步数据。
 */
class BLASTERLEARING_API IInteractWithCrosshairsInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
};
