// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickups/HealthPickup.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BlasterComponents/BuffComponent.h"
#include "BlasterComponents/CombatComponent.h"
#include "Character/BlasterCharacter.h"
#include "PlayerController/BlasterPlayerController.h"


AHealthPickup::AHealthPickup()
{
	bReplicates = true;
}


void AHealthPickup::OverlapSphereOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::OverlapSphereOnOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor))
	{
		if (UBuffComponent* BuffComponent = BlasterCharacter->GetBuffComponent())
		{
			BuffComponent->Heal(HealAmount, HealingTime);
		}
	}
	Destroy();
}
