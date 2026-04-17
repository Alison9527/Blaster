// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/BlasterWeapon.h"
#include "ProjectileWeapon.generated.h"

class AProjectile;

/**
 * */
UCLASS()
class BLASTER_API AProjectileWeapon : public ABlasterWeapon
{
	
	GENERATED_BODY()

public:
	virtual void Fire(const FVector& HitTarget) override;

protected:
	// 用于封装子弹生成决策数据的结构体 (DTO)
	struct FProjectileSpawnLogic
	{
		TSubclassOf<AProjectile> ClassToSpawn = nullptr;
		bool bSetSSR = false;
		bool bSetDamage = false;
		bool bSetVelocity = false;
	};

	// 辅助函数声明
	bool GetMuzzleSocketTransform(FTransform& OutTransform) const;
	FProjectileSpawnLogic CalculateSpawnLogic(const APawn* InstigatorPawn) const;
	void SpawnProjectile(const FProjectileSpawnLogic& SpawnLogic, const FTransform& SocketTransform, const FVector& HitTarget, APawn* InstigatorPawn) const;

private:
	UPROPERTY(EditAnywhere)
	TSubclassOf<AProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<AProjectile> ServerSideRewindProjectileClass;
};