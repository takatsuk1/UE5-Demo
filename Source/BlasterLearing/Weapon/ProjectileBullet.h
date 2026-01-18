// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileBullet.generated.h"

/**
 * 文件说明：ProjectileBullet 是 AProjectile 的子类，代表子弹类投射物。
 * 这里主要覆盖 OnHit 与 BeginPlay 以实现子弹专用的命中判定逻辑（包括 SSR 支持）。
 */

UCLASS()
class BLASTERLEARING_API AProjectileBullet : public AProjectile
{
	GENERATED_BODY()
	
public:
	AProjectileBullet();

protected:
	// 重载 OnHit：处理服务器直判或客户端发起的 ServerSideRewind 请求
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	// BeginPlay：可用于初始化弹道追踪/调试
	virtual void BeginPlay() override;
};
