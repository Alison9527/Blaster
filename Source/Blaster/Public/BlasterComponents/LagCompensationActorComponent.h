// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/List.h"
#include "LagCompensationActorComponent.generated.h"

USTRUCT(BlueprintType)
struct FBoxInformation
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector Location;
	
	UPROPERTY()
	FRotator Rotation;
	
	UPROPERTY()
	FVector BoxExtent;
};

USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()
	
	UPROPERTY()
	float Time;
	
	UPROPERTY()
	TMap<FName, FBoxInformation> HitBoxInfo;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API ULagCompensationActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULagCompensationActorComponent();
	friend class ABlasterCharacter;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ShowFramePackage(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitRadius);
	void ServerSideRewind(const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitRadius, ABlasterCharacter* HitCharacter, float HitTime);


	TDoubleLinkedList<FFramePackage> FrameHistory;
	
	UPROPERTY()
	class ABlasterPlayerController* BlasterPlayerController;
protected:
	virtual void BeginPlay() override;
	void SaveFramePackage(FFramePackage& FramePackage);
	FFramePackage InterpolateBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime);
private:
	UPROPERTY()
	ABlasterCharacter* BlasterCharacter;
	
	UPROPERTY(EditAnywhere)
	float MaxRecordTime = 4.f;
};
