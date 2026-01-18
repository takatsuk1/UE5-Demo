// 文件说明：
// 该文件实现基于 Projectile 的子类 ProjectileBullet，用于模拟子弹类投射物。
// 注释说明包括：构造器中对 ProjectileMovementComponent 的初始化意图、编辑器属性响应、
// 命中回调中的服务器/客户端分支逻辑、以及 BeginPlay 中潜在的弹道预测示例。
// 仅添加注释，不修改任何代码逻辑。

// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileBullet.h"
#include "Kismet/GameplayStatics.h"
#include "BlasterLearing/Character/BlasterCharacter.h"
#include "BlasterLearing/BlasterComponent/LagCompensationComponent.h"
#include "BlasterLearing/PlayerController/BlasterPlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"

AProjectileBullet::AProjectileBullet()
{
	// 构造函数说明：
	// 创建并配置 ProjectileMovementComponent：
	// - bRotationFollowsVelocity: 让投射物朝向随速度方向旋转（视觉上符合抛射物）
	// - SetIsReplicated(true): 使 ProjectileMovement 在网络中复制（保证物理位置同步）
	// - InitialSpeed/MaxSpeed 从 InitialSpeed 成员读取（编辑器可调）
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->SetIsReplicated(true);
	ProjectileMovementComponent->InitialSpeed =InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
}


void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// OnHit 说明：
	// - 该函数在投射物与其它物体发生碰撞时调用
	// - 根据拥有者与当前运行环境（服务器/客户端）以及是否启用 ServerSideRewind 决定伤害处理方式
	// - 关键分支：
	//    1) 若拥有者在服务器且未使用 SSR：在服务器直接造成权威伤害
	//    2) 若使用 SSR 且为本地控制器：向 LagCompensation 发送回放判定请求，由服务器回放核验命中
	//    3) 其他情况：调用父类 OnHit（例如播放特效、销毁等）
	ABlasterCharacter* OwnerCharacter = Cast<ABlasterCharacter>(GetOwner());
		if (OwnerCharacter)
	{
	AController* OwnerController = OwnerCharacter->Controller;
		if (OwnerController)
		{
				// 服务器端直接造成伤害的分支（权威判定）
			if ((OwnerCharacter->HasAuthority() && !bUseServerSideRewind) || !OwnerCharacter->IsPlayerControlled())
			{
						// 根据命中骨骼判断是否爆头，决定伤害值
				const float DamageToCause = Hit.BoneName.ToString() == FString("head") ? HeadShotDamage : Damage;

				UGameplayStatics::ApplyDamage(OtherActor, DamageToCause, OwnerController, this, UDamageType::StaticClass());
				// 调用父类以执行后续通用逻辑（例如销毁、特效等）
				Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
				return;
			}
			// 客户端使用 ServerSideRewind 的分支：发送回放请求以便服务器重放判定
			ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(OtherActor);
			if (bUseServerSideRewind && OwnerCharacter->GetLagCompensation() && OwnerCharacter->IsLocallyControlled() && HitCharacter)
			{
				ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(OwnerController);
				if (BlasterPlayerController)
				{
					// 发送 ProjectileServerScoreRequest，参数包含：
					// - 被击中的角色指针
					// - 客户端记录的 TraceStart（射线起点）
					// - 初速度 InitialVelocity（用于回放模拟）
					// - 近似的服务器时间（使用 OwnerController 的 ServerTime 减去单程延迟）
					OwnerCharacter->GetLagCompensation()->ProjectileServerScoreRequest(
						HitCharacter,
						TraceStart,
						InitialVelocity,
						BlasterPlayerController->GetServerTime() - BlasterPlayerController->SingleTripTime
					);
				}
			}
		}
	}
	// 兜底：调用父类默认处理（可能包含销毁/特效等）
	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}

void AProjectileBullet::BeginPlay()
{
	Super::BeginPlay();
}
