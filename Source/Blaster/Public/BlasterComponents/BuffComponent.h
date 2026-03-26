// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	friend class ABlasterCharacter;
	
	UBuffComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Heal(float HealAmount, float HealingTime);
	void ReplenishShield(float ReplenishAmount, float ReplenishTime);
	void BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime);
	void SetInitalSpeeds(float BaseSpeed, float CrouchSpeed);
	void SetInitialJumpZVelocity(float JumpZVelocity);
	
	void BuffJump(float BuffJumpZVelocity, float BuffTime);

protected:
	virtual void BeginPlay() override;
	void HealRampUp(float DeltaTime);
	void ShieldRampUp(float DeltaTime);
	
private:
	UPROPERTY()
	class ABlasterCharacter* BlasterCharacter;
	
	/*
	 * Heal buff
	 */
	bool bHealing = false;
	float HealingRate = 0;
	float AmountToHeal = 0.f;
	
	/*
	 * Shield buff
	 */
	bool bReplenishingShield = false;
	float ShieldReplenishRate = 0.f;
	float ShieldReplenishAmount = 0.f;
	
	/*
	 * Speed buff
	 */
	FTimerHandle SpeedBuffTimer;
	void ResetSpeeds();
	float InitialBaseSpeed = 0.f;
	float InitialCrouchSpeed = 0.f;
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpeedBuff(float BuffBaseSpeed, float BuffCrouchSpeed);
	
	/*
	 * Jump buff
	 */

	FTimerHandle JumpBuffTimer;
	void ResetJump();
	float InitialJumpZVelocity = 0.f;
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpBuff(float BuffJumpZVelocity);
};
