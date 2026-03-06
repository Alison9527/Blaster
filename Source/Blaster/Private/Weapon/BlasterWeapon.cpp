// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/BlasterWeapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Character/BlasterCharacter.h"


ABlasterWeapon::ABlasterWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(RootComponent);
	// Ensure the sphere can generate overlap events by default (helps avoid timing issues)
	AreaSphere->SetGenerateOverlapEvents(true);
	// Keep default responses ignored, we'll enable Pawn overlap on the authoritative side in BeginPlay
	AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
}

void ABlasterWeapon::ShowPickupWidget(bool bShowWidget)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void ABlasterWeapon::BeginPlay()
{
	Super::BeginPlay();
	
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}
	
	if (HasAuthority())
	{
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &ABlasterWeapon::OnSphereOverlap);
		AreaSphere->OnComponentEndOverlap.AddDynamic(this, &ABlasterWeapon::OnSphereEndOverlap);

		UE_LOG(LogTemp, Warning, TEXT("BlasterWeapon::BeginPlay Name=%s HasAuthority=%d AreaSphere.GenerateOverlap=%d CollisionEnabled=%d Radius=%.1f"),
			*GetName(), HasAuthority(), AreaSphere->GetGenerateOverlapEvents(), (int32)AreaSphere->GetCollisionEnabled(), AreaSphere->GetUnscaledSphereRadius());
		UE_LOG(LogTemp, Warning, TEXT("BlasterWeapon::BeginPlay Bound OnComponentBegin/EndOverlap (server)"));
	}
	else
	{
		// For debugging: log that this instance is not authoritative (client)
		UE_LOG(LogTemp, Warning, TEXT("BlasterWeapon::BeginPlay Name=%s HasAuthority=%d (no binding done on client) AreaSphere.GenerateOverlap=%d Radius=%.1f"),
			*GetName(), HasAuthority(), AreaSphere->GetGenerateOverlapEvents(), AreaSphere->GetUnscaledSphereRadius());
	}
	
}

void ABlasterWeapon::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("OnSphereOverlap Weapon=%s Other=%s HasAuthority=%d"), *GetName(), *GetNameSafe(OtherActor), HasAuthority());
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter)
	{
		// PickupWidget->SetVisibility(true);
		BlasterCharacter->SetOverlappingWeapon(this);
	}
}

void ABlasterWeapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("OnSphereEndOverlap Weapon=%s Other=%s HasAuthority=%d"), *GetName(), *GetNameSafe(OtherActor), HasAuthority());
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter)
	{
		// PickupWidget->SetVisibility(false);
		BlasterCharacter->SetOverlappingWeapon(nullptr);
	}
}

