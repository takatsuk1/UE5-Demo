// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Task_ToggleFire.generated.h"

/**
 * 
 */
UCLASS()
class BLASTERLEARING_API UTask_ToggleFire : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UTask_ToggleFire();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "AI")
	bool bStartFire = true;
};
