// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "DemoAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class AWeapon;

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API ADemoAIController : public AAIController
{
	GENERATED_BODY()
public:
	ADemoAIController();
	virtual void OnPossess(APawn* InPawn) override;

	// 感知组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionComponent* PerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAISenseConfig_Sight* SightConfig;

	// 查找最近的武器
	UFUNCTION(BlueprintCallable, Category = "AI")
	AWeapon* FindNearestWeapon(float MaxDistance = 10000.f) const;

	// 获取随机漫游点
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool GetRandomWanderLocation(FVector& OutLoc, float Radius = 2000.f) const;

	// 尝试拾取武器（通常在移动到武器附近后调用）
	UFUNCTION(BlueprintCallable, Category = "AI")
	void AttemptPickupWeapon();

	UPROPERTY(EditAnywhere, Category = "AI")
	UBehaviorTree* BehaviorTree;

	// 控制开火
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void StopFire();

protected:
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	UPROPERTY()
	class ABoxMap* CachedBoxMap;

	UPROPERTY()
	AActor* CurrentTargetActor;
};
