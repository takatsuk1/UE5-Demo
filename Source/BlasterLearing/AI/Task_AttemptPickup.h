// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Task_AttemptPickup.generated.h"

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API UTask_AttemptPickup : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UTask_AttemptPickup();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
