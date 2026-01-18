#include "HitScanWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "BlasterLearing/Character/BlasterCharacter.h"
#include "BlasterLearing/PlayerController/BlasterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "WeaponTypes.h"
#include "BlasterLearing/BlasterComponent/LagCompensationComponent.h"

//#include "DrawDebugHelpers.h"

void AHitScanWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	// 获取拥有者 Pawn 与控制器（用于应用伤害时传入 InstigatorController）
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	// 尝试获取武器网格上的 MuzzleFlash 插槽变换作为射线起点
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();

		// 进行射线检测并获得命中信息 FireHit
		FHitResult FireHit;
		WeaponTraceHit(Start, HitTarget, FireHit);

		// 若命中角色，则考虑直接在服务端造成伤害或使用服务器回放机制进行后端判定
		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
		if (BlasterCharacter  && InstigatorController)
		{
			// 是否在本地/服务器直接造成权威伤害（当不使用 SSR 或者本地拥有者在服务器上）
			bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
			if (HasAuthority() && bCauseAuthDamage)
			{
				// 根据命中骨骼决定是否为爆头伤害
				const float DamageToCause = FireHit.BoneName.ToString() == FString("head") ? HeadShotDamage : Damage;

				// 在服务端直接调用 ApplyDamage，传递 InstigatorController 与 DamageType
				UGameplayStatics::ApplyDamage(
					BlasterCharacter,
					DamageToCause,
					InstigatorController,
					this,
					UDamageType::StaticClass()
				);
			}
			// 若客户端检测并启用了 ServerSideRewind，则客户端向服务器提交回放请求以核算命中
			if (!HasAuthority() && bUseServerSideRewind)
			{
				BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
				BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
				// 仅当本地拥有者存在且具备 LagCompensation 组件时才发送请求
				if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensation() && BlasterOwnerCharacter->IsLocallyControlled())
				{
					// 向服务器请求回放并计算该次射击在服务器时间线上的有效性
					BlasterOwnerCharacter->GetLagCompensation()->ServerScoreRequest(
						BlasterCharacter,
						Start,
						HitTarget,
						// 发送时间需要减去 SingleTripTime 以近似客户端发送时刻的服务器时间
						BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime
					);
				}
			}
		}
		// 处理命中特效（粒子与音效）在客户端与服务器的播放（视觉反馈）
		if (ImpactParticles)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				ImpactParticles,
				FireHit.ImpactPoint,
				FireHit.ImpactNormal.Rotation()
			);
		}
		if (HitSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				HitSound,
				FireHit.ImpactPoint
			);
		}
		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				MuzzleFlash,
				SocketTransform
			);
		}
		if (FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				FireSound,
				GetActorLocation()
			);
		}
	}
}

// WeaponTraceHit: 基于起点与目标位置进行可视通道的 LineTrace 并输出命中信息与用于 Beam 粒子的最终点
void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit)
{
	UWorld* World = GetWorld();
	if (World)
	{
		// 将射线稍微拉长（1.25 倍），以覆盖跨越较远目标的视觉表现
		FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;
		// 执行单次射线检测，使用 Visibility 通道以匹配可见性判定
		World->LineTraceSingleByChannel(
			OutHit,
			TraceStart,
			End,
			ECollisionChannel::ECC_Visibility
		);
		// 如果发生阻挡，将 Beam 的结束点设为命中点，否则设为延伸终点
		FVector BeamEnd = End;
		if (OutHit.bBlockingHit)
		{
			BeamEnd = OutHit.ImpactPoint;
		}
		else
		{
			// 若未命中任何物体，将命中点设置为延伸终点以便特效显示
			OutHit.ImpactPoint = End;
		}
		

		// 若存在 Beam 粒子系统，则在起点生成并设置目标向量参数以连接粒子到命中点
		if (BeamParticles)
		{
			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				World,
				BeamParticles,
				TraceStart,
				FRotator::ZeroRotator,
				true
			);
			if (Beam)
			{
				// 设置名为 "Target" 的向量参数，供粒子系统用作束的终点
				Beam->SetVectorParameter(FName("Target"), BeamEnd);
			}
		}
	}
}
