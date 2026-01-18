// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerState.h"
#include "BlasterLearing/Character/BlasterCharacter.h"
#include "BlasterLearing/PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"

// GetLifetimeReplicatedProps: 注册需要在客户端复制的属性（Defeats, Team）
void ABlasterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Defeats：被击倒次数, 用于 HUD 显示
	DOREPLIFETIME(ABlasterPlayerState, Defeats);
}

// OnRep_Score: 当 Score 在客户端被复制更新时调用（父类实现 + 本地 HUD 更新）
// 说明：父类会处理内部 Score 属性，这里额外尝试把分数同步到 Character -> Controller -> HUD
void ABlasterPlayerState::OnRep_Score()
         {
         	Super::OnRep_Score();
         
         	// 如果尚未缓存 Character 指针则从 Pawn 获取
         	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
         	if (Character)
         	{
         		// 缓存 Controller 并调用其 SetHUDScore 更新本地 HUD 显示
         		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
         		if (Controller)
         		{
         			Controller->SetHUDScore(Score);
         		}
         	}
         }
         
         // OnRep_Defeats: 当 Defeats 被复制到客户端时，更新 HUD 的被击倒次数显示
         void ABlasterPlayerState::OnRep_Defeats()
         {
         	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
         	if (Character)
         	{
         		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
         		if (Controller)
         		{
         			Controller->SetHUDDefeats(Defeats);
         		}
         	}
         }
         
         // AddToScore: 在服务器为该 PlayerState 增加分数并通知本地 HUD 更新
         void ABlasterPlayerState::AddToScore(float ScoreAmount)
         {
         	// 修改 PlayerState 的 Score（父类接口），这将触发复制/OnRep_Score 在客户端更新
         	SetScore(GetScore() + ScoreAmount);
         	// 同步更新本地 HUD（若 Controller 可用）
         	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
         	if (Character)
         	{
         		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
         		if (Controller)
         		{
			Controller->SetHUDScore(GetScore());
		}
	}
}

// AddToDefeats: 增加被击倒次数并更新 HUD
void ABlasterPlayerState::AddToDefeats(int32 DefeatsAmount)
{
	Defeats += DefeatsAmount;
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

