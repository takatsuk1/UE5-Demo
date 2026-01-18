#include "Projectile.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"
#include "BlasterLearing/Character/BlasterCharacter.h"
#include "BlasterLearing/BlasterLearing.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

AProjectile::AProjectile()
{

	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 创建碰撞箱并设置合适的碰撞响应，仅阻挡可见性与世界静态与骨骼网格
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECollisionResponse::ECR_Block);
}


void AProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 如果配置了弹迹 Tracer，则在根组件上附着生成粒子（客户端/服务器均可显示）
	if (Tracer)
	{
		TracerComponent = UGameplayStatics::SpawnEmitterAttached(Tracer, CollisionBox, FName(), GetActorLocation(), GetActorRotation(), EAttachLocation::KeepWorldPosition);
	}

	// 仅服务器监听碰撞事件以避免客户端重复判定伤害
	if (HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
	}
}

// OnHit: 投射物与世界发生碰撞后的响应（服务器上处理伤害/销毁/粒子音效）
void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{

	// 默认行为：直接销毁投射物，具体伤害通过 ExplodeDamage 或者碰撞逻辑在服务器端处理
	Destroy();
}

void AProjectile::SpawnTrailSystem()
{
	// 若配置了 Niagara 轨迹系统，附着到根组件以便随投射物移动显示轨迹
	if (TrailSystem)
	{
		TrailSystemComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailSystem,
			GetRootComponent(),
			FName(),
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false);
	}
}

// ExplodeDamage: 在爆炸武器（如榴弹）需要对周围造成区域伤害时调用（仅服务器执行）
void AProjectile::ExplodeDamage()
{
	APawn* FireingPawn = GetInstigator();
	if (FireingPawn && HasAuthority())
	{
		AController* FireingController = FireingPawn->GetController();
		if (FireingController)
		{
			// 使用 ApplyRadialDamageWithFalloff 在指定内外半径之间以衰减方式施加伤害
			UGameplayStatics::ApplyRadialDamageWithFalloff(
				this, // Damage Causer
				Damage, // Base Damage
				10.f, // Minimum Damage at outer radius
				GetActorLocation(), // Explosion Origin
				DamageInnerRadius, // Inner radius（全额伤害）
				DamageOutRadius, // Outer radius（最小伤害）
				1.f, // Damage falloff exponent
				UDamageType::StaticClass(), // Damage Type
				TArray<AActor*>(), // Actors to ignore
				this, // Damage Causer
				FireingController // Instigator Controller
			);
		}
	}
}

// Tick: 当前实现为空，可用于实现飞行轨迹校正或粒子同步
void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProjectile::StartDestroyTimer()
{
	// 启动计时器，在一定时间后自动销毁投射物（防止场景残留）
	GetWorldTimerManager().SetTimer(
		DestroyTimer,
		this,
		&AProjectile::DestroyTimerFinished,
		DestroyTime);
}

void AProjectile::DestroyTimerFinished()
{
	Destroy();
}

void AProjectile::Destroyed()
{
	Super::Destroyed();

	// 销毁时播放命中特效/音效（可在客户端表现）
	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, GetActorLocation());
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
}
