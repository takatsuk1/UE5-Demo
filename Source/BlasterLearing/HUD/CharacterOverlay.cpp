// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterOverlay.h"

// 文件说明：CharacterOverlay Widget 的实现文件
// 该 Widget 绑定角色的各种状态信息（血量条、护盾、分数、弹药、倒计时等）
// 大多数更新逻辑在 PlayerController 或 Character 中触发并更新绑定的文本/进度条。
// 如果需要在 C++ 中做额外的更新（例如动画触发或自定义绑定），可以在此实现 NativeConstruct 或自定义函数。
