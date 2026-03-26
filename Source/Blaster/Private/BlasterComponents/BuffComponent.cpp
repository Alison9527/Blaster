// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterComponents/BuffComponent.h"

#include "Character/BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UBuffComponent::UBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UBuffComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	HealRampUp(DeltaTime);
	ShieldRampUp(DeltaTime);
}

void UBuffComponent::Heal(float HealAmount, float HealingTime)
{
	bHealing = true;
	HealingRate = HealAmount / HealingTime;
	AmountToHeal += HealAmount;
}

void UBuffComponent::ReplenishShield(float ReplenishAmount, float ReplenishTime)
{
	bReplenishingShield = true;
	ShieldReplenishRate = ReplenishAmount / ReplenishTime;
	ShieldReplenishAmount += ReplenishAmount;
}

void UBuffComponent::HealRampUp(float DeltaTime)
{
	if (!bHealing || BlasterCharacter == nullptr || BlasterCharacter->IsElimmed()) return;
	
	const float HealThisFrame = HealingRate * DeltaTime;
	BlasterCharacter->SetHealth(FMath::Clamp(BlasterCharacter->GetHealth() + HealThisFrame, 0.f, BlasterCharacter->GetMaxHealth()));
	
	BlasterCharacter->UpdateHUDHealth();
	
	AmountToHeal -= HealThisFrame;
	
	if (AmountToHeal <= 0.f || BlasterCharacter->GetHealth() >= BlasterCharacter->GetMaxHealth())
	{
		bHealing = false;
		AmountToHeal = 0.f;
	}
}

void UBuffComponent::ShieldRampUp(float DeltaTime)
{
	if (!bReplenishingShield || BlasterCharacter == nullptr || BlasterCharacter->IsElimmed()) return;
	const float ReplenishThisFrame = ShieldReplenishRate * DeltaTime;
	BlasterCharacter->SetShield(FMath::Clamp(BlasterCharacter->GetShield() + ReplenishThisFrame, 0.f, BlasterCharacter->GetMaxShield()));
	ShieldReplenishAmount -= ReplenishThisFrame;
	
	if (ShieldReplenishAmount <= 0.f || BlasterCharacter->GetShield() >= BlasterCharacter->GetMaxShield())
	{
		bReplenishingShield = false;
		ShieldReplenishAmount = 0.f;
	}
}

void UBuffComponent::BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime)
{
	if (BlasterCharacter == nullptr) return;
	
	BlasterCharacter->GetWorldTimerManager().SetTimer(SpeedBuffTimer, this, &UBuffComponent::ResetSpeeds, BuffTime);
	
	if (BlasterCharacter->GetCameraComponent())
	{
		BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = BuffBaseSpeed;
		BlasterCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched = BuffCrouchSpeed;
	}
	MulticastSpeedBuff(BuffBaseSpeed, BuffCrouchSpeed);
}

void UBuffComponent::SetInitalSpeeds(float BaseSpeed, float CrouchSpeed)
{
	InitialBaseSpeed = BaseSpeed;
	InitialCrouchSpeed = CrouchSpeed;
}

void UBuffComponent::SetInitialJumpZVelocity(float JumpZVelocity)
{
	InitialJumpZVelocity = JumpZVelocity;
}

void UBuffComponent::ResetSpeeds()
{
	if (BlasterCharacter == nullptr || BlasterCharacter->GetCharacterMovement() == nullptr) return;
	
	BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = InitialBaseSpeed;
	BlasterCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched = InitialCrouchSpeed;
	
	MulticastSpeedBuff(InitialBaseSpeed, InitialCrouchSpeed);
}

void UBuffComponent::MulticastSpeedBuff_Implementation(float BuffBaseSpeed, float BuffCrouchSpeed)
{
	if (BlasterCharacter == nullptr || BlasterCharacter->GetCharacterMovement() == nullptr) return;
	BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = InitialBaseSpeed;
	BlasterCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched = InitialCrouchSpeed;
}

void UBuffComponent::BuffJump(float BuffJumpZVelocity, float BuffTime)
{
	if (BlasterCharacter == nullptr) return;
	
	BlasterCharacter->GetWorldTimerManager().SetTimer(JumpBuffTimer, this, &UBuffComponent::ResetJump, BuffTime);
	
	if (BlasterCharacter->GetCameraComponent())
	{
		BlasterCharacter->GetCharacterMovement()->JumpZVelocity = BuffJumpZVelocity;
	}
	MulticastJumpBuff(BuffJumpZVelocity);
}

void UBuffComponent::ResetJump()
{
	if (BlasterCharacter->GetCameraComponent())
	{
		BlasterCharacter->GetCharacterMovement()->JumpZVelocity = InitialJumpZVelocity;
	}
	MulticastJumpBuff(InitialJumpZVelocity);
}

void UBuffComponent::MulticastJumpBuff_Implementation(float BuffJumpZVelocity)
{
	if (BlasterCharacter && BlasterCharacter->GetCharacterMovement())
	{
		BlasterCharacter->GetCharacterMovement()->JumpZVelocity = BuffJumpZVelocity;
	}
}



