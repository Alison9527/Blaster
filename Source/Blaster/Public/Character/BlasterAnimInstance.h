// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BlasterTypes/TurningInPlace.h"
#include "BlasterAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API UBlasterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	class ABlasterCharacter* BlasterCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float Speed;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsInAir;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsAccelerating;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bWeaponEquipped;
	
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	class ABlasterWeapon* EquippedWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsCrouched;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bAiming;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float YawOffset;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float Lean;

	FRotator CharacterRotationLastFrame;
	FRotator CharacterRotation;
	FRotator DeltaRotation;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float AO_Yaw;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float AO_Pitch;
	
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	FTransform LeftHandTransform;
	
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	ETurningInPlace TurningInPlace;
	
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	FRotator RightHandRotation;
	
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bLocallyControlled;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bRotateRootBone;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bElimmed;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bUseFABRIK;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bUseAimOffsets;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bTransformRightHand;
};
