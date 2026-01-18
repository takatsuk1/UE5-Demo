// Fill out your copyright notice in the Description page of Project Settings.


#include "Task_GetWanderLocation.h"
#include "DemoAIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UTask_GetWanderLocation::UTask_GetWanderLocation()
{
	NodeName = "Get Wander Location";
}

EBTNodeResult::Type UTask_GetWanderLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADemoAIController* Controller = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!Controller) return EBTNodeResult::Failed;

	FVector OutLoc;
	if (Controller->GetRandomWanderLocation(OutLoc, Radius))
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsVector(WanderLocationKey.SelectedKeyName, OutLoc);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
