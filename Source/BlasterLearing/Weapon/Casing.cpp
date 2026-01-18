// Fill out your copyright notice in the Description page of Project Settings.


#include "Casing.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

ACasing::ACasing()
{
	PrimaryActorTick.bCanEverTick = false;

	// 创建并设置静态网格组件作为根组件，用于渲染弹壳
	CasingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CasingMesh"));
	SetRootComponent(CasingMesh);
	// 弹壳不与相机发生碰撞，以免干扰视角
	CasingMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	// 开启物理仿真，使其被抛出后受物理与重力影响
	CasingMesh->SetSimulatePhysics(true);
	CasingMesh->SetEnableGravity(true);
	// 开启碰撞通知以便在击中地面时播放声音并销毁
	CasingMesh->SetNotifyRigidBodyCollision(true);
	// 弹壳弹出初始冲量大小，默认 10.f，可在编辑器调整
	ShellEjectionImpulse = 10.f;
}

void ACasing::BeginPlay()
{
	Super::BeginPlay();
	
	// 绑定碰撞回调：当弹壳与世界其它物体发生碰撞时触发 OnHit
	CasingMesh->OnComponentHit.AddDynamic(this, &ACasing::OnHit);
	// 在开始时给弹壳一个沿角色前方方向的冲量，模拟抛壳动作
	CasingMesh->AddImpulse(GetActorForwardVector() * ShellEjectionImpulse);
}

// OnHit: 弹壳碰撞回调，播放声音并销毁自身以避免场景中残留过多 Actor
// 参数说明：HitComp/OtherActor/OtherComp/NormalImpulse/Hit 包含碰撞信息与接触点
void ACasing::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 若设置了弹壳落地声音，则在碰撞点播放一次
	if (ShellSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShellSound, GetActorLocation());
	}
	// 播放完声音后立即销毁该弹壳 Actor，节省资源
	Destroy();
}
