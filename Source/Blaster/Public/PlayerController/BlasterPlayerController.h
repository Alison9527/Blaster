// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDShield(float Shield, float MaxShield);
	void SetHUDScore(float Score);
	void SetHUDDefeats(int32 Defeats);
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
	void SetHUDMatchCountdown(float CountdownTime);
	void SetHUDAnnouncementCountdown(float CountdownTime);
	void SetHUDGrenades(int32 Grenades);
	void SetHUDRedTeamScore(int32 RedScore);
	void SetHUDBlueTeamScore(int32 BlueScore);

	
	virtual float GetServerTime();
	virtual void ReceivedPlayer() override;
	void HandleMatchHasStarted(bool bTeamMatch = false);
	void OnMatchStateSet(FName State);
	void HandleCooldown();
	
	float SingleTripTime = 0.f;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetHUDTime();
	// Poll for any relelvant classes and initialize our HUD
	void PollInit();

	/*
	 * Sync time between client and server
	 */

	// Requests the current server time, passing in the client's time when the request was sent
	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	// Repoarts the current server time to the client in response to ServerRequestServerTime
	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	float ClientServerDelta = 0.f;

	UPROPERTY(EditAnywhere, Category = "Time")
	float TimeSyncFrequency = 5.f;

	float TimeSyncRunningTime = 0.f;
	void CheckTimeSync(float DeltaTime);

	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;

	UFUNCTION()
	void OnRep_MatchState();

	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();

	// BlasterPlayerController.h
	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime);

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;

	float HUDHealth = 0.f;
	bool bInitializeHealth = false;
	float HUDMaxHealth = 0.f;
	
	float HUDScore = 0.f;
	bool bInitializeScore = false;
	int32 HUDDefeats = 0;
	bool bInitializeDefeats = false;
	
	int32 HUDGrenades = 0;
	bool bInitializeGrenades = false;
	
	float HUDShield = 0.f;
	bool bInitializeShield = false;
	float HUDMaxShield = 0.f;
	
	float HUDCarriedAmmo = 0.f;
	bool bInitializeCarriedAmmo = false;
	
	int32 HUDWeaponAmmo = 0;
	bool bInitializeWeaponAmmo = false;
	
	void HighPingWarning();
	void StopHighPingWarning();
	void CheckPing(float DeltaTime);
	
private:
	UPROPERTY()
	class ABlasterHUD* BlasterHUD = nullptr;

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode = nullptr;

	float WarmupTime = 0.f;
	float MatchTime = 120.f;
	float CooldownTime = 0.f;
	float LevelStartingTime = 0.f;
	uint32 CountdownInt = 0;
	
	float HighPingRunningTime = 0.f;
	
	UPROPERTY(EditAnywhere)
	float HighPingDuration = 5.f;
	
	float PingAnimationRunningTime = 0.f;
	
	UPROPERTY(EditAnywhere)
	float CheckPingFrequency = 20.f;
	
	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f;
	
	
};


