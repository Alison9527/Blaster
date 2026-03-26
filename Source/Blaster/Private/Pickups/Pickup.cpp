// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickups/Pickup.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Weapon/WeaponTypes.h"

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	OverlaySphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlaySphere"));
	OverlaySphere->SetupAttachment(RootComponent);
	OverlaySphere->SetSphereRadius(150.f);
	
	OverlaySphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	OverlaySphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	OverlaySphere->AddLocalOffset(FVector(0.0f, 0.0f, 85.0f));
	
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(OverlaySphere);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetRelativeScale3D(FVector(5.f, 5.f, 5.f));
	
	PickupMesh->SetRenderCustomDepth(true);
	PickupMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);
	
	PickupEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PickupEffectComponent"));
	PickupEffectComponent->SetupAttachment(RootComponent);

}

void APickup::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		OverlaySphere->OnComponentBeginOverlap.AddDynamic(this, &APickup::OverlapSphereOnOverlapBegin);
	}
}

void APickup::OverlapSphereOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
}

void APickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PickupMesh)
	{
		PickupMesh->AddWorldRotation(FRotator(0.f, BaseTurnRate * DeltaTime, 0.f));
	}
}

void APickup::Destroyed()
{
	Super::Destroyed();
	
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}
	
	if (PickupEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, 
			PickupEffect, 
			GetActorLocation(), 
			GetActorRotation()
			);
	}
}

