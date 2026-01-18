// Fill out your copyright notice in the Description page of Project Settings.


#include "Task_FindNearestWeapon.h"
#include "DemoAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BlasterLearing/Weapon/Weapon.h"

UTask_FindNearestWeapon::UTask_FindNearestWeapon()
{
	NodeName = "Find Nearest Weapon";
}

EBTNodeResult::Type UTask_FindNearestWeapon::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
		ADemoAIController* Controller = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!Controller) return EBTNodeResult::Failed;

	AActor* Weapon = Controller->FindNearestWeapon(MaxDistance);

if (Weapon)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsObject(TargetWeaponKey.SelectedKeyName, Weapon);
		return EBTNodeResult::Succeeded;
	}
	
	return EBTNodeResult::Failed;
}
