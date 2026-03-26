// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupSpawnPoint.generated.h"

UCLASS()
class BLASTER_API APickupSpawnPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	APickupSpawnPoint();
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, Category = "Pickup")
	TArray<TSubclassOf<class APickup>> PickupClasses;
	
	void SpawnPickup();
	void SpawnPickupTimerFinished();
	
	UFUNCTION()
	void StartSpawnPickupTimer(AActor* DestroyedActor);

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY()
	APickup* SpawnedPickup;
	
	
private:
	FTimerHandle SpawnPickupTimer;
	
	UPROPERTY(EditAnywhere)
	float SpawnPickupTimeMin = 5.f;
	
	UPROPERTY(EditAnywhere)
	float SpawnPickupTimeMax = 10.f;
};
