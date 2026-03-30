// Fill out your copyright notice in the Description page of Project Settings.


#include "Blaster/Public/PlayerState/BlasterPlayerState.h"
#include "Character/BlasterCharacter.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"

ABlasterPlayerState::ABlasterPlayerState()
{
    OwnerController = nullptr;
    OwnerCharacter = nullptr;
    Defeats = 0;
}

void ABlasterPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ABlasterPlayerState, Defeats);
	DOREPLIFETIME(ABlasterPlayerState, Team);
}

void ABlasterPlayerState::OnRep_Score()
{
	Super::OnRep_Score();

	 OwnerCharacter =  OwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetPawn()) :  OwnerCharacter;
	if ( OwnerCharacter)
	{
		OwnerController = OwnerController == nullptr ? Cast<ABlasterPlayerController>( OwnerCharacter->Controller) : OwnerController;
		if (OwnerController)
		{
			OwnerController->SetHUDScore(GetScore());
		}
	}
}

void ABlasterPlayerState::OnRep_Defeats()
{
	 OwnerCharacter =  OwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetPawn()) :  OwnerCharacter;
	if ( OwnerCharacter)
	{
		OwnerController = OwnerController == nullptr ? Cast<ABlasterPlayerController>( OwnerCharacter->Controller) : OwnerController;
		if (OwnerController)
		{
			OwnerController->SetHUDDefeats(Defeats);
		}
	}
}

void ABlasterPlayerState::OnRep_Team()
{
	if (ABlasterCharacter* BCharacter = Cast<ABlasterCharacter>(GetPawn()))
	{
		BCharacter->SetTeamColor(Team);
	}
}

void ABlasterPlayerState::AddToScore(float ScoreAmount)
{
    // Use getter to obtain current score and set the new score
    const float NewScore = GetScore() + ScoreAmount;
    SetScore(NewScore);

    // Try to get the pawn (character) that this PlayerState belongs to
    ABlasterCharacter* PawnCharacter = Cast<ABlasterCharacter>(GetPawn());
    OwnerCharacter = (OwnerCharacter == nullptr) ? PawnCharacter : OwnerCharacter;

    if (PawnCharacter)
    {
        // Get the controller from the pawn and cache it
        ABlasterPlayerController* PawnController = Cast<ABlasterPlayerController>(PawnCharacter->GetController());
        OwnerController = (OwnerController == nullptr) ? PawnController : OwnerController;

        // Only update HUD on the local controller (prevents trying to access remote HUDs/null HUDs)
        if (PawnController && PawnController->IsLocalController())
        {
            PawnController->SetHUDScore(NewScore);
        }
    }
}

void ABlasterPlayerState::AddToDefeats(float DefeatsAmount)
{
	Defeats += DefeatsAmount;
	OwnerCharacter = OwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : OwnerCharacter;
	if (OwnerCharacter)
	{
		OwnerController = OwnerController == nullptr ? Cast<ABlasterPlayerController>(OwnerCharacter->GetController()) : OwnerController;
		if (OwnerController)
		{
			OwnerController->SetHUDDefeats(Defeats);
		}
	}
}

void ABlasterPlayerState::SetTeam(ETeam TeamToSet)
{
	Team = TeamToSet;

	if (ABlasterCharacter* BCharacter = Cast<ABlasterCharacter>(GetPawn()))
	{
		BCharacter->SetTeamColor(Team);
	}
}
