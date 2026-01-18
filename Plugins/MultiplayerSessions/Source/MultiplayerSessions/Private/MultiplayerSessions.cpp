// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerSessions.h"

#define LOCTEXT_NAMESPACE "FMultiplayerSessionsModule"

void FMultiplayerSessionsModule::StartupModule()
{
	// 插件模块启动：模块被加载进内存后会执行（可在这里做注册、初始化等）
}

void FMultiplayerSessionsModule::ShutdownModule()
{
	// 插件模块关闭：卸载模块前执行（清理注册项、释放资源等）
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMultiplayerSessionsModule, MultiplayerSessions)