// Fill out your copyright notice in the Description page of Project Settings.


#include "Shotgun.h"
#include "Engine/SkeletalMeshSocket.h"
#include "BlasterLearing/Character/BlasterCharacter.h"
#include "BlasterLearing/PlayerController/BlasterPlayerController.h"
#include "BlasterLearing/BlasterComponent/LagCompensationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Kismet/KismetMathLibrary.h"

// 文件说明：Shotgun 的核心逻辑包括：根据散射生成多条射线、记录每发子弹的命中并聚合到每个角色上、
// 对爆头和普通命中分别计数并计算总伤害，然后根据是否启用 SSR 选择本地直接伤害或发送回放请求。
// 注释在关键步骤中做逐步说明，方便理解与维护。

void AShotgun::FireShotgun(const TArray<FVector_NetQuantize>& HitTargets)
{
	AWeapon::Fire(FVector());
	AWeapon::Fire(FVector());
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket)
	{
		const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		const FVector Start = SocketTransform.GetLocation();

		// 使用两个 Map 分别记录每个角色被普通子弹/爆头命中的次数
		TMap<ABlasterCharacter*, uint32> HitMap;
		TMap<ABlasterCharacter*, uint32> HeadShotHitMap;
		// 遍历每个预计算的弹丸终点（来自 ShotgunTraceEndWithScatter）
		for (FVector_NetQuantize HitTarget : HitTargets)
		{
			FHitResult FireHit;
			// 对每个弹丸执行一次射线检测
			WeaponTraceHit(Start, HitTarget, FireHit);

			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
			if (BlasterCharacter)
			{
				// 判断是否爆头
				const float bHeadShot = FireHit.BoneName.ToString() == FString("head");

				// 将命中计数加入对应的 Map（用于后续按角色聚合伤害）
				if (bHeadShot)
				{
					if (HeadShotHitMap.Contains(BlasterCharacter)) HeadShotHitMap[BlasterCharacter]++;
					else HeadShotHitMap.Emplace(BlasterCharacter, 1);
				}
				else
				{
					if (HitMap.Contains(BlasterCharacter)) HitMap[BlasterCharacter]++;
					else HitMap.Emplace(BlasterCharacter, 1);
				}


				// 播放命中特效与音效（视觉/听觉反馈），在命中位置产生粒子与声音
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
						FireHit.ImpactPoint,
						0.5f,
						FMath::FRandRange(-0.5f, 0.5f)
					);
				}
			}
		}
		TArray<ABlasterCharacter*> HitCharacters;

		// DamageMap 用于存放每个被命中角色的总伤害（普通命中 + 爆头）
		TMap<ABlasterCharacter*, float> DamageMap;

		// 将普通命中计数转换为伤害值并加入 DamageMap
		for (auto HitPair : HitMap)
		{
			if (HitPair.Key)
			{
				DamageMap.Emplace(HitPair.Key, HitPair.Value * Damage);
				HitCharacters.AddUnique(HitPair.Key);
			}
		}

		// 将爆头计数转换为伤害并叠加到 DamageMap（若已有普通伤害则相加）
		for (auto HeadShotHitPair : HeadShotHitMap)
		{
			if (HeadShotHitPair.Key)
			{
				if (DamageMap.Contains(HeadShotHitPair.Key)) DamageMap[HeadShotHitPair.Key] += HeadShotHitPair.Value * HeadShotDamage;
				else DamageMap.Emplace(HeadShotHitPair.Key, HeadShotHitPair.Value * HeadShotDamage);

				HitCharacters.AddUnique(HeadShotHitPair.Key);
			}
		}

		// 对聚合后的每个角色进行一次性伤害应用（避免重复 ApplyDamage 导致不必要的副作用）
		for (auto DamagePair : DamageMap)
		{
			if (DamagePair.Key && InstigatorController)
			{
				// 如果在服务器且不使用 SSR 或者在本地权威环境则直接造成伤害
				bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
				if (HasAuthority() && bCauseAuthDamage)
				{
					UGameplayStatics::ApplyDamage(
						DamagePair.Key, // 被击中的角色
						DamagePair.Value, // 该角色本次应受的总伤害（所有弹丸汇总）
						InstigatorController,
						this,
						UDamageType::StaticClass()
					);
				}
			}
		}

		// 若客户端启用了 SSR，则由客户端发送请求给服务器让服务端回放验证命中与伤害
		if (!HasAuthority() && bUseServerSideRewind)
		{
			BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
			BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
			if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensation() && BlasterOwnerCharacter->IsLocallyControlled())
			{
				// 发送 ShotgunServerScoreRequest 包含：
				// - HitCharacters: 本次所有被命中的角色列表（不重复）
				// - Start: 枪口起点
				// - HitTargets: 客户端为每根弹丸计算得到的终点数组（用于回放检索）
				// - ServerTime: 近似服务器时间（客户端的 ServerTime - SingleTripTime）
				BlasterOwnerCharacter->GetLagCompensation()->ShotgunServerScoreRequest(
					HitCharacters,
					Start,
					HitTargets,
					BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime
				);
			}
		}
	}
}

void AShotgun::ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets)
{
	// 计算枪口位置及弹丸发散球心（SphereCenter）
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket == nullptr) return;

	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
	const FVector TraceStart = SocketTransform.GetLocation();

	// 计算从枪口到目标的单位方向，并在该方向上放置一个距离为 DistanceToSphere 的球心
	const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
	const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
	
	// 对每个弹丸生成一个随机位置（在球体半径内），并将该点投影到远端长度得到弹丸终点
	for (uint32 i = 0; i < NumberOfPellets; i++)
	{
		const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
		const FVector EndLoc = SphereCenter + RandVec;
		FVector ToEndLoc = EndLoc - TraceStart;
		ToEndLoc = TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size();

		// 将计算得到的终点加入 HitTargets 供 FireShotgun 遍历处理
		HitTargets.Add(ToEndLoc);
	}
}
