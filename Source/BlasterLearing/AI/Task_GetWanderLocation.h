// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Task_GetWanderLocation.generated.h"

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API UTask_GetWanderLocation : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UTask_GetWanderLocation();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector WanderLocationKey;

	UPROPERTY(EditAnywhere, Category = "AI")
	float Radius = 2000.f;
};
