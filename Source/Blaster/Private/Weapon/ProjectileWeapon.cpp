// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/ProjectileWeapon.h"
#include "Weapon/Projectile.h"
#include "Engine/SkeletalMeshSocket.h"

// ==========================================
// 1. 指挥官函数：流程清晰，一目了然
// ==========================================
void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	if (InstigatorPawn == nullptr) return; 

	FTransform SocketTransform;
	if (!GetMuzzleSocketTransform(SocketTransform)) return; // 获取枪口失败则中止

	// 决策：需要生成什么类型的子弹？
	FProjectileSpawnLogic SpawnLogic = CalculateSpawnLogic(InstigatorPawn);

	// 执行：生成并初始化子弹
	if (SpawnLogic.ClassToSpawn)
	{
		SpawnProjectile(SpawnLogic, SocketTransform, HitTarget, InstigatorPawn);
	}
}

// ==========================================
// 2. 枪口位置获取模块
// ==========================================
bool AProjectileWeapon::GetMuzzleSocketTransform(FTransform& OutTransform) const
{
	if (USkeletalMeshComponent* Mesh = GetWeaponMesh())
	{
		if (const USkeletalMeshSocket* MuzzleFlashSocket = Mesh->GetSocketByName(FName("MuzzleFlash")))
		{
			OutTransform = MuzzleFlashSocket->GetSocketTransform(Mesh);
			return true;
		}
	}
	return false;
}

// ==========================================
// 3. 核心决策模块：完全解耦，只处理布尔逻辑
// ==========================================
AProjectileWeapon::FProjectileSpawnLogic AProjectileWeapon::CalculateSpawnLogic(const APawn* InstigatorPawn) const
{
	FProjectileSpawnLogic Logic;
	if (InstigatorPawn == nullptr) return Logic;

	const bool bIsServer = InstigatorPawn->HasAuthority();
	const bool bIsLocallyControlled = InstigatorPawn->IsLocallyControlled();

	if (bUseServerSideRewind)
	{
		if (bIsServer) 
		{
			if (bIsLocallyControlled) // 服务器主机玩家自己开枪
			{
				Logic.ClassToSpawn = ProjectileClass;
				Logic.bSetDamage = true;
			}
			else // 服务器处理客户端发来的开枪请求
			{
				Logic.ClassToSpawn = ServerSideRewindProjectileClass;
				Logic.bSetSSR = true;
			}
		}
		else // 客户端
		{
			Logic.ClassToSpawn = ServerSideRewindProjectileClass;
			
			if (bIsLocallyControlled) // 客户端本地玩家自己开枪
			{
				Logic.bSetSSR = true;
				Logic.bSetVelocity = true;
			}
			// 否则是模拟代理，仅保留 ClassToSpawn，属性全为 false
		}
	}
	else // 未启用延迟补偿
	{
		if (bIsServer) // 仅服务端生成真实子弹
		{
			Logic.ClassToSpawn = ProjectileClass;
			Logic.bSetDamage = true;
		}
	}

	return Logic;
}

// ==========================================
// 4. 执行生成模块：只管生成和赋值，不问为什么
// ==========================================
void AProjectileWeapon::SpawnProjectile(const FProjectileSpawnLogic& SpawnLogic, const FTransform& SocketTransform, const FVector& HitTarget, APawn* InstigatorPawn) const
{
	UWorld* World = GetWorld();
	if (World == nullptr) return;

	FVector ToTarget = HitTarget - SocketTransform.GetLocation();
	FRotator TargetRotation = ToTarget.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();          
	SpawnParams.Instigator = InstigatorPawn;

	if (AProjectile* SpawnedProjectile = World->SpawnActor<AProjectile>(SpawnLogic.ClassToSpawn, SocketTransform.GetLocation(), TargetRotation, SpawnParams))
	{
		SpawnedProjectile->bUseServerSideRewind = SpawnLogic.bSetSSR;
		
		if (SpawnLogic.bSetDamage)
		{
			SpawnedProjectile->Damage = Damage;
			SpawnedProjectile->HeadShotDamage = HeadShotDamage;
		}
		
		if (SpawnLogic.bSetVelocity)
		{
			SpawnedProjectile->TraceStart = SocketTransform.GetLocation();
			SpawnedProjectile->InitialVelocity = SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed;
		}
	}
}
