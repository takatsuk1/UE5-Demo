// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HitScanWeapon.h"
#include "Shotgun.generated.h"

/**
 * 文件说明：Shotgun 继承自 HitScanWeapon，支持多弹丸散射逻辑。
 * FireShotgun 接收一组预计算的子弹终点并逐一检测命中，然后进行伤害聚合与 SSR 处理。
 * ShotgunTraceEndWithScatter 根据目标生成散射终点数组（在客户端/服务器可用于预测和同步）。
 */
UCLASS()
class BLASTERLEARING_API AShotgun : public AHitScanWeapon
{
	GENERATED_BODY()
	
public:
	// 根据外部计算好的 HitTargets（每根弹丸的终点）执行逐弹命中判定并聚合伤害
	virtual void FireShotgun(const TArray<FVector_NetQuantize>& HitTargets);
	// 给定目标点，生成 NumberOfPellets 条弹丸终点（散射算法）
	void ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets);

private:
	// 发射时的弹丸数（可在编辑器调整）
	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	uint32 NumberOfPellets = 10;
};
