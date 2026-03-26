// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickups/AmmoPickup.h"

#include "BlasterComponents/CombatComponent.h"
#include "Character/BlasterCharacter.h"

void AAmmoPickup::OverlapSphereOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                              UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Call the base APickup implementation
    Super::OverlapSphereOnOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

    if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor))
    {
        if (UCombatComponent* CombatComponent = BlasterCharacter->GetCombatComponent())
        {
            CombatComponent->PickupAmmo(WeaponType, AmmoAmount);
        }
    }
    Destroy();
}
