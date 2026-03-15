// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BlasterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	ABlasterPlayerState();
	/*
	 * Replication notifies
	 */
	virtual void OnRep_Score() override;
	
	UFUNCTION()
	void OnRep_Defeats();
	
	void AddToScore(float ScoreAmount);
	void AddToDefeats(float DefeatsAmount);
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
private:
    UPROPERTY(Transient)
    class ABlasterPlayerController* OwnerController;
    UPROPERTY(Transient)
    class ABlasterCharacter* OwnerCharacter;
    
    UPROPERTY(ReplicatedUsing = OnRep_Defeats)
    int32 Defeats;
};



