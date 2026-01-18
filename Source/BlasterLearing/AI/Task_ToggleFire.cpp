// Fill out your copyright notice in the Description page of Project Settings.


#include "Task_ToggleFire.h"
#include "DemoAIController.h"

UTask_ToggleFire::UTask_ToggleFire()
{
	NodeName = "Toggle Fire";
}

EBTNodeResult::Type UTask_ToggleFire::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADemoAIController* Controller = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!Controller) return EBTNodeResult::Failed;

	if (bStartFire)
	{
		Controller->StartFire();
	}
	else
	{
		Controller->StopFire();
	}

	return EBTNodeResult::Succeeded;
}
