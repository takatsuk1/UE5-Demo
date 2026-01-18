// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Casing.generated.h"

UCLASS()
class BLASTERLEARING_API ACasing : public AActor
{
	GENERATED_BODY()
	
public:	
	ACasing();

protected:
	virtual void BeginPlay() override;

	// 弹壳击中事件回调（绑定到静态网格的 OnComponentHit）
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
private:
	// 静态网格用于渲染弹壳，并承载物理仿真与碰撞
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* CasingMesh;

	// 弹壳弹出时的初始冲量，可在编辑器中调整
	UPROPERTY(EditAnywhere)
	float ShellEjectionImpulse;

	// 弹壳落地时播放的声音（可选）
	UPROPERTY(EditAnywhere)
	class USoundCue* ShellSound;
};
