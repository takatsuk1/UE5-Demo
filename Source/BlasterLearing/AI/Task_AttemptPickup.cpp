// Fill out your copyright notice in the Description page of Project Settings.


#include "Task_AttemptPickup.h"
#include "DemoAIController.h"

UTask_AttemptPickup::UTask_AttemptPickup()
{
	NodeName = "Attempt Pickup";
}

EBTNodeResult::Type UTask_AttemptPickup::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADemoAIController* Controller = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!Controller) return EBTNodeResult::Failed;

	Controller->AttemptPickupWeapon();
	
	return EBTNodeResult::Succeeded;
}
