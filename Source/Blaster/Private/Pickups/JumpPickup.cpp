// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickups/JumpPickup.h"

#include "BlasterComponents/BuffComponent.h"
#include "Character/BlasterCharacter.h"

void AJumpPickup::OverlapSphereOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::OverlapSphereOnOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter)
	{
		UBuffComponent* BuffComponent = BlasterCharacter->GetBuffComponent();
		if (BuffComponent)
		{
			BuffComponent->BuffJump(JumpZVelocityBuff, JumpBuffTime);
		}
	}
	Destroy();
}
