// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"

// 文件说明：ProjectileWeapon 的 Fire 实现负责在枪口位置生成投射物 Actor。
// 注释将解释不同网络角色（服务器/客户端、本地控制器/远端）以及 bUseServerSideRewind 的处理策略。
// 仅添加注释，不改变逻辑。

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	 // 获取拥有者 Pawn 以设置为投射物的 Instigator
	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	 const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	 UWorld* World = GetWorld();
	 if (MuzzleFlashSocket && World && InstigatorPawn)
	 {
		 FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		 // 计算从枪口到目标的方向旋转，用于 Spawn 出投射物时设置朝向
		 FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		 FRotator TargetRotation = ToTarget.Rotation();

		 FActorSpawnParameters SpawnParams;
		 SpawnParams.Owner = GetOwner();
		 SpawnParams.Instigator = InstigatorPawn;

		 AProjectile* SpawnedProjectile = nullptr;
		 if (bUseServerSideRewind)
		 {
			 // 当启用 ServerSideRewind 时，需要区分服务器与客户端、以及本地控制器或远端代理
			 if (InstigatorPawn->HasAuthority())// 在服务器上
			 {
				 if (InstigatorPawn->IsLocallyControlled())// 服务器上的本地控制器（通常 listen server）
				 {
				 	 // 服务器本地控制器可生成常规 Projectile，用服务器权威处理伤害（不使用 SSR）
					 SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					 if (SpawnedProjectile)
					 {
						SpawnedProjectile->bUseServerSideRewind = false;
						SpawnedProjectile->Damage = Damage;
						SpawnedProjectile->HeadShotDamage = HeadShotDamage;
					 }
				 }
				 else// 服务器上的远端代理（其他玩家的 Pawn）
				 {
					 // 为了兼容客户端回放请求，使用专门的 ServerSideRewindProjectileClass 并开启 SSR
					 SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					 if (SpawnedProjectile)
					 {
						SpawnedProjectile->bUseServerSideRewind = true;
					 }
				 }
			 }
			 else// 在客户端上
			 {
				 if (InstigatorPawn->IsLocallyControlled())// 客户端本地玩家发射
				 {
					 // 客户端本地玩家生成 SSR 专用投射物，并上传 TraceStart/InitialVelocity 以供服务器回放验证
					 SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					 if (SpawnedProjectile)
					 {
						SpawnedProjectile->bUseServerSideRewind = true;
						SpawnedProjectile->TraceStart = SocketTransform.GetLocation();
						SpawnedProjectile->InitialVelocity = SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed;
					 }
					 // 注意：客户端通常不直接设置 Damage（由服务器回放或服务器权威决定）
					 //SpawnedProjectile->Damage = Damage;//注释：不在客户端直接设权威伤害
				 }
				 else// 客户端上的远端代理（非本地玩家）
				 {
					 // 客户端的远端代理生成的投射物无需 SSR（可视同步用）
					 SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					 if (SpawnedProjectile)
					 {
						SpawnedProjectile->bUseServerSideRewind = false;
					 }
				 }
			 }
		 }
		 else// 不使用 ServerSideRewind 的普通流程（服务器权威伤害）
		 {
		 	 if (InstigatorPawn->HasAuthority())
			 {
				 SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				 if (SpawnedProjectile)
				 {
					SpawnedProjectile->bUseServerSideRewind = false;
					SpawnedProjectile->Damage = Damage;
					SpawnedProjectile->HeadShotDamage = HeadShotDamage;
				 }
			 }
		 }
	 }
}
