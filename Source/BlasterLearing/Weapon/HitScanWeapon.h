// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "HitScanWeapon.generated.h"

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API AHitScanWeapon : public AWeapon
{
	GENERATED_BODY()
	
public:
	// 发射函数：基于目标点执行射线检测并按需造成伤害（可以被子类覆盖）
	virtual void Fire(const FVector& HitTarget) override;

protected:
	// 执行射线检测并返回命中信息（供 Fire 调用）
	void WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit);

	// 命中特效（在命中点生成）
	UPROPERTY(EditAnywhere)
	class UParticleSystem* ImpactParticles;

	// 命中音效（在命中点播放）
	UPROPERTY(EditAnywhere)
	USoundCue* HitSound;

private:
	// 光束粒子（用于从枪口到命中点的视觉光束）
	UPROPERTY(EditAnywhere)
	UParticleSystem* BeamParticles;

	// 枪口闪光特效
	UPROPERTY(EditAnywhere)
	UParticleSystem* MuzzleFlash;

	// 射击音效（在武器位置或玩家处播放）
	UPROPERTY(EditAnywhere)
	USoundCue* FireSound;
};
