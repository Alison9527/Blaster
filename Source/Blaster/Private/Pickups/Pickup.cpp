// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickups/Pickup.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

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
	
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(OverlaySphere);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
		FRotator Rotation = GetActorRotation();
		Rotation.Yaw += BaseTurnRate * DeltaTime;
		SetActorRotation(Rotation);
	}
}

void APickup::Destroyed()
{
	Super::Destroyed();
	
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}
}

