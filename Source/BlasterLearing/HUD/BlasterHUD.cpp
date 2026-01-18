// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHUD.h"
#include "GameFramework/PlayerController.h"
#include "CharacterOverlay.h"
#include "Announcement.h"
#include "ElimAnnouncement.h"
#include "Components/HorizontalBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void ABlasterHUD::BeginPlay()
{
	Super::BeginPlay();

	// BeginPlay 说明：
	// HUD 在 BeginPlay 时通常会创建并添加 HUD 相关的 Widget（例如角色信息、公告栏）
	// 这里保留入口，实际创建在外部调用 AddCharacterOverlay/AddAnnouncement 等函数时进行，
	// 以保证在需要时按需创建并避免 BeginPlay 中创建顺序问题（例如 PlayerController 尚未就绪）。
}

void ABlasterHUD::AddCharacterOverlay()
{
	// 该函数负责将角色属性覆盖层（血条/护盾/弹药等）添加到视口
	// 先获取拥有该 HUD 的 PlayerController 再创建 Widget，确保 Widget 与正确的 PlayerContext 绑定
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && CharacterOverlayClass)
	{
		// CreateWidget 使用 PlayerController 作为 Outer，保证在多玩家场景中 Widget 归属正确
		CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
		// 将 Widget 添加到视口，使其对玩家可见
		CharacterOverlay->AddToViewport();
	}
}

void ABlasterHUD::AddAnnouncement()
{
	// 添加游戏公告（例如热身倒计时、比赛开始/结束信息）
	// 与角色覆盖层类似，通过 PlayerController 创建并显示
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && AnnouncementClass)
	{
		Announcement = CreateWidget<UAnnouncement>(PlayerController, AnnouncementClass);
		Announcement->AddToViewport();
	}
}

void ABlasterHUD::AddElimAnnouncement(FString Attacker, FString Victim)
{
	// 该函数用于在屏幕顶部堆叠显示“击杀/被击杀”消息
	// 首先缓存 OwningPlayer（如果尚未缓存），然后创建一个 ElimAnnouncement Widget
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	if (OwningPlayer && ElimAnnouncementClass)
	{
		UElimAnnouncement* ElimAnouncementWidget = CreateWidget<UElimAnnouncement>(OwningPlayer, ElimAnnouncementClass);
		if (ElimAnouncementWidget)
		{
			// 设置击杀文本（攻击者/受害者名字）并显示
			ElimAnouncementWidget->SetElimAnnouncementText(Attacker, Victim);
			ElimAnouncementWidget->AddToViewport();

			// 将已有的消息向上移动一个消息高度，以实现新消息从下方出现的堆叠效果
			for (UElimAnnouncement* Msg : ElimMessages)
			{
				if (Msg && Msg->AnnouncementBox)
				{
					UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Msg->AnnouncementBox);
					if (CanvasSlot)
					{
						// 读取当前位置并将 Y 值减去消息高度，使其上移
						FVector2D Position = CanvasSlot->GetPosition();
						FVector2D NewPosition(
							CanvasSlot->GetPosition().X,
							Position.Y - CanvasSlot->GetSize().Y
						);
						CanvasSlot->SetPosition(NewPosition);
					}
				}
			}

			// 将新消息加入消息数组并设置定时器在指定时间后移除该消息
			ElimMessages.Add(ElimAnouncementWidget);

			FTimerHandle ElimMsgTimer;
			FTimerDelegate ElimMsgDelegate;
			// 绑定定时器委托以在时间结束后调用 ElimAnnouncementTimerFinished，参数为要移除的消息 Widget
			ElimMsgDelegate.BindUFunction(this, FName("ElimAnnouncementTimerFinished"), ElimAnouncementWidget);
			GetWorldTimerManager().SetTimer(
				ElimMsgTimer,
				ElimMsgDelegate,
				ElimAnnouncementTime,
				false
			);
		}
	}
}

void ABlasterHUD::ElimAnnouncementTimerFinished(UElimAnnouncement* MsgToRemove)
{
	// 定时器回调：移除到期的击杀消息 Widget（从父级移除即可）
	if (MsgToRemove)
	{
		MsgToRemove->RemoveFromParent();
	}
}

void ABlasterHUD::DrawHUD()
{
	Super::DrawHUD();

	// DrawHUD 说明：
	// 该函数每帧被调用，用于绘制低层次的 HUD 元素（例如自定义绘制的十字准星）
	FVector2D ViewportSize;
	if (GEngine)
	{
		// 获取当前视口大小以计算中心点
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

		// 根据当前 HUDPackage 的 CrosshairSpread 值计算实际偏移量（显示准星扩散效果）
		float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;

		// 分别绘制中心和四个方向的准星纹理，传入不同的偏移量
		if (HUDPackage.CrosshairsCenter)
		{
			FVector2D Spread(0.f, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsLeft)
		{
			FVector2D Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsRight)
		{
			FVector2D Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsTop)
		{
			FVector2D Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsBottom)
		{
			FVector2D Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
	}
}

void ABlasterHUD::DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor)
{
	// 计算纹理尺寸并确定绘制点（以纹理中心对齐）
	const float TextureWidth = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();
	const FVector2D TextureDrawPoint(ViewportCenter.X - (TextureWidth / 2.f) + Spread.X, ViewportCenter.Y - (TextureHeight / 2.f) + Spread.Y);

	// 使用 DrawTexture 在屏幕上绘制准星纹理，传入颜色以支持动态着色
	DrawTexture(Texture, TextureDrawPoint.X, TextureDrawPoint.Y, TextureWidth, TextureHeight, 0.f, 0.f, 1.f, 1.f, CrosshairColor);
}