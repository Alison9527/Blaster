// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterComponents/LagCompensationActorComponent.h"

#include "Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "Runtime/CoreUObject/Internal/UObject/PackageRelocation.h"

ULagCompensationActorComponent::ULagCompensationActorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void ULagCompensationActorComponent::BeginPlay()
{
	Super::BeginPlay();
	
}

void ULagCompensationActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (FrameHistory.Num() <= 1)
	{
		FFramePackage ThisFramePackage;
		SaveFramePackage(ThisFramePackage);
		FrameHistory.AddHead(ThisFramePackage);
	}
	else
	{
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		}
		FFramePackage ThisFramePackage;
		SaveFramePackage(ThisFramePackage);
		FrameHistory.AddHead(ThisFramePackage);
	}
}

void ULagCompensationActorComponent::SaveFramePackage(FFramePackage& FramePackage)
{
	BlasterCharacter = BlasterCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterCharacter;
	if (BlasterCharacter)
	{
		FramePackage.Time = GetWorld()->GetTimeSeconds();
		for (const auto& HitBox : BlasterCharacter->HitCollisionBoxes)
		{
			FBoxInformation BoxInformation;
			BoxInformation.Location = HitBox.Value->GetComponentLocation();
			BoxInformation.Rotation = HitBox.Value->GetComponentRotation();
			BoxInformation.BoxExtent = HitBox.Value->GetScaledBoxExtent();
			FramePackage.HitBoxInfo.Add(HitBox.Key, BoxInformation);
		}
	}
}

FFramePackage ULagCompensationActorComponent::InterpolateBetweenFrames(const FFramePackage& OlderFrame,
	const FFramePackage& YoungerFrame, float HitTime)
{
	
}

void ULagCompensationActorComponent::ShowFramePackage(const FFramePackage& FramePackage,
                                                      ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation,
                                                      float HitRadius)
{
	for (const auto& HitBox : HitCharacter->HitCollisionBoxes)
	{
		DrawDebugBox(GetWorld(), HitBox.Value->GetComponentLocation(), HitBox.Value->GetScaledBoxExtent(), FColor::Red, false, 5.f);
	}
}

void ULagCompensationActorComponent::ServerSideRewind(const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation, float HitRadius, ABlasterCharacter* HitCharacter, float HitTime)
{
	bool bReturn =
		HitCharacter == nullptr ||
			HitCharacter->GetLagCompensationActorComponent() == nullptr ||
				HitCharacter->GetLagCompensationActorComponent()->FrameHistory.GetHead() == nullptr ||
					HitCharacter->GetLagCompensationActorComponent()->FrameHistory.GetTail() == nullptr;
	
	// Frame package that we check to verify a hit
	FFramePackage FrameToCheck;
	bool bShouldInterpolate = true;
	// Frame history of the HitCharacter
	const TDoubleLinkedList<FFramePackage>& LocalFrameHistory = HitCharacter->GetLagCompensationActorComponent()->FrameHistory;
	const float OldestHistoryTime = FrameHistory.GetTail()->GetValue().Time;
	const float NewestHistoryTime = FrameHistory.GetHead()->GetValue().Time;
	
	if (OldestHistoryTime > HitTime)
	{
		// too for back - tool laggy to do SSR
		return;
	}
	
	if (OldestHistoryTime == HitTime)
	{
		FrameToCheck = FrameHistory.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	
	if (NewestHistoryTime <= HitTime)
	{
		// too far in the past - not enough lag compensation to do SSR
		FrameToCheck = FrameHistory.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = FrameHistory.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = FrameHistory.GetTail();
	while (Older->GetValue().Time > HitTime) // is Older still younger than the hit time?
	{
		// March back untill: OlderTime < HitTime < YoungerTime
		if (Older->GetNextNode() == nullptr) break;
		Older = Older->GetNextNode();
		if (Older->GetValue().Time > HitTime)
		{
			Younger = Older;
		}
	}
	if (Older->GetValue().Time == HitTime) // highly unlikely, but we found our frame to check
	{
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	
	if (bShouldInterpolate)
	{
		// Interpolate between Younger and Older
	}
	
	if (bReturn) return;

}

