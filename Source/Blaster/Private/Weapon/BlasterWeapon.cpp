// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/BlasterWeapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Character/BlasterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Weapon/Casing.h"

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

void ABlasterWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABlasterWeapon, WeaponState);
}

void ABlasterWeapon::Fire(const FVector& HitTarget)
{
	if (WeaponMesh && FireAnimation)
	{
		WeaponMesh->PlayAnimation(FireAnimation, false);
	}
	if (CasingClass)
	{
		const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject"));
		if (AmmoEjectSocket)
		{
			FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);
			GetWorld()->SpawnActor<ACasing>(CasingClass, SocketTransform.GetLocation(), SocketTransform.GetRotation().Rotator());
		}
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

void ABlasterWeapon::OnRep_WeaponState()
{
	switch (WeaponState)
	{
		case EWeaponState::EWS_Initial:
			// Handle initial state logic if needed
			break;
		case EWeaponState::EWS_Equipped:
			ShowPickupWidget(false);
			break;
		case EWeaponState::EWS_Dropped:
			// Logic for when the weapon is dropped
			break;
		default:
			break;
	}
	// Optionally, update the weapon's appearance or behavior based on the new state
}

void ABlasterWeapon::SetWeaponState(EWeaponState State)
{
	WeaponState = State;
	switch (WeaponState)
	{
		case EWeaponState::EWS_Initial:
			// Handle initial state logic if needed
			break;
		case EWeaponState::EWS_Equipped:
			ShowPickupWidget(false);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		case EWeaponState::EWS_Dropped:
			ShowPickupWidget(true);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
			// Optionally, you can set the weapon's visibility or other properties here
			// For example, you might want to make the weapon
			break;
		default:
			break;
	}
}

