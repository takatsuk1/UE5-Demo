// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Task_FindNearestWeapon.generated.h"

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API UTask_FindNearestWeapon : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UTask_FindNearestWeapon();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetWeaponKey;

	UPROPERTY(EditAnywhere, Category = "AI")
	float MaxDistance = 2000.f;
};
