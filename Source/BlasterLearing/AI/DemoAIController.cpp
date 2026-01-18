// Fill out your copyright notice in the Description page of Project Settings.


#include "DemoAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BlasterLearing/Weapon/Weapon.h"
#include "BlasterLearing/Character/BlasterCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BlasterLearing/Map/BoxMap.h"

ADemoAIController::ADemoAIController()
{
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	if (SightConfig)
	{
		SightConfig->SightRadius = 3000.f;
		SightConfig->LoseSightRadius = 3500.f;
		SightConfig->PeripheralVisionAngleDegrees = 90.f;
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

		PerceptionComp->ConfigureSense(*SightConfig);
		PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	}
}

void ADemoAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (PerceptionComp)
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ADemoAIController::OnPerceptionUpdated);
	}

	if (BehaviorTree)
	{
		RunBehaviorTree(BehaviorTree);
	}
}

AWeapon* ADemoAIController::FindNearestWeapon(float MaxDistance) const
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return nullptr;

	TArray<AActor*> FoundWeapons;
	UGameplayStatics::GetAllActorsOfClass(this, AWeapon::StaticClass(), FoundWeapons);

	AWeapon* NearestWeapon = nullptr;
	float MinDistSq = MaxDistance * MaxDistance;

	for (AActor* Actor : FoundWeapons)
	{
		AWeapon* Weapon = Cast<AWeapon>(Actor);
		if (!Weapon) continue;

		EWeaponState State = Weapon->GetWeaponState();
		// 武器处于 Initial 或 Dropped 状态时均可被拾取
		bool bCanPickup = (State == EWeaponState::EWS_Initial || State == EWeaponState::EWS_Dropped);
		bool bIsRifle = (Weapon->GetWeaponType() == EWeaponType::EWT_AssaultRifle);

		if (bCanPickup && bIsRifle)
		{
			float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), Weapon->GetActorLocation());
			if (DistSq < MinDistSq)
			{
				MinDistSq = DistSq;
				NearestWeapon = Weapon;
			}
		}
	}

	return NearestWeapon;
}

bool ADemoAIController::GetRandomWanderLocation(FVector& OutLoc, float Radius) const
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return false;

	// 尝试获取缓存的 BoxMap，如果没有则查找
	ABoxMap* Map = CachedBoxMap;
	if (!Map)
	{
		TArray<AActor*> FoundMaps;
		UGameplayStatics::GetAllActorsOfClass(this, ABoxMap::StaticClass(), FoundMaps);
		if (FoundMaps.Num() > 0)
		{
			// 这里使用了 const_cast 是因为这个函数是 const 的，但我们想要更新 mutable 缓存或者局部使用
			// 由于 C++ const 成员函数的限制，我们最好直接用局部变量 Map，不强制更新成员变量 CachedBoxMap
			// 或者将 CachedBoxMap 设为 mutable。这里为了简单直接使用局部查找结果，能在下一次非 const 调用中缓存会更好，
			// 但现在先每次查找也没关系，或者使用 mutable。
			// 既然不能修改成员变量，就直接用局部变量。
			Map = Cast<ABoxMap>(FoundMaps[0]);
		}
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return false;

	if (Map)
	{
		// 在 BoxMap 范围内生成一个随机点
		// 假设 BoxMap 中心就是其 ActorLocation
		FVector Origin = Map->GetActorLocation();
		float SafeMargin = 100.f; // 稍微向内缩一点，避免正好在墙上
		float HalfX = FMath::Max(0.f, Map->Xlength / 2.0f - SafeMargin);
		float HalfY = FMath::Max(0.f, Map->Ylength / 2.0f - SafeMargin);

		for (int32 i = 0; i < 10; i++) // 尝试几次以防投射失败
		{
			FVector RandomPt = Origin;
			RandomPt.X += FMath::FRandRange(-HalfX, HalfX);
			RandomPt.Y += FMath::FRandRange(-HalfY, HalfY);
			// Z 轴保持在 Origin.Z 附近或者向下射线检测，这里直接用 Origin.Z
			
			// 将随机点投射到导航网格上
			FNavLocation Result;
			// 使用较大的 Extent 确保能投射到附近的 NavMesh
			if (NavSys->ProjectPointToNavigation(RandomPt, Result, FVector(200.f, 200.f, 500.f)))
			{
				OutLoc = Result.Location;
				return true;
			}
		}
	}
	else
	{
		// 如果找不到 BoxMap，回退到原来的逻辑基于半径漫游
		FNavLocation Result;
		if (NavSys->GetRandomPointInNavigableRadius(MyPawn->GetActorLocation(), Radius, Result))
		{
			OutLoc = Result.Location;
			return true;
		}
	}
	return false;
}

void ADemoAIController::AttemptPickupWeapon()
{
	ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterChar)
	{
		// 模拟按下装备键，前提是已经移动到了武器附近并触发了 Overlap
		BlasterChar->EquipButtonPressed();

		// 如果成功装备了武器，将黑板键 HasWeapon 设为 true
		if (BlasterChar->GetEquippedWeapon())
		{
			if (Blackboard)
			{
				Blackboard->SetValueAsBool(FName("HasWeapon"), true);
			}
		}
	}
}

void ADemoAIController::StartFire()
{
	ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterChar)
	{
		BlasterChar->FireButtonPressed();
	}
}

void ADemoAIController::StopFire()
{
	ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterChar)
	{
		BlasterChar->FireButtonReleased();
	}
}

void ADemoAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	ABlasterCharacter* SensedCharacter = Cast<ABlasterCharacter>(Actor);
	if (SensedCharacter)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			// 看见玩家
			CurrentTargetActor = SensedCharacter;
			// 这里可以更新黑板键，例如 "TargetEnemy"
			if (Blackboard)
			{
				Blackboard->SetValueAsObject(FName("TargetEnemy"), SensedCharacter);
			}
		}
		else
		{
			// 丢失视野
			if (CurrentTargetActor == SensedCharacter)
			{
				CurrentTargetActor = nullptr;
				if (Blackboard)
				{
					Blackboard->ClearValue(FName("TargetEnemy"));
				}
			}
		}
	}
}
