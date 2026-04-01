// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHighPingDelegate, bool, bPingTooHigh);

UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	// === HUD 更新相关函数 ===
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

	void HideTeamScores();
	void InitTeamScores();

	// === 网络与同步相关 ===
	virtual float GetServerTime(); 
	virtual void ReceivedPlayer() override; 

	void OnMatchStateSet(FName State, bool bTeamMatch = false);
	void HandleMatchHasStarted(bool bTeamMatch = false);
	void HandleCooldown();

	void BroadcastElim(APlayerState* Attacker, APlayerState* Victim);

	float SingleTripTime = 0.f; 

	FHighPingDelegate HighPingDelegate;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupInputComponent() override; 

	void SetHUDTime(); 
	void PollInit();   

	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

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

	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime, bool bIsTeamsMatch); 

	UFUNCTION(Client, Reliable)
	void ClientElimAnnouncement(APlayerState* Attacker, APlayerState* Victim);

	UPROPERTY(Replicated)
	bool bShowTeamScores = false;

	void HighPingWarning(); 
	void StopHighPingWarning(); 
	void CheckPing(float DeltaTime); 

	UFUNCTION(Server, Reliable)
	void ServerReportPingStatus(bool bHighPing);

	void ShowReturnToMainMenu();

	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players);
	FString GetTeamsInfoText(class ABlasterGameState* BlasterGameState);

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay; 

private:
	UPROPERTY()
	class ABlasterHUD* BlasterHUD = nullptr; 

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode = nullptr; 

	/* 返回主菜单相关 */
	// ✅ 修复拼写错误：Wdiget -> Widget
	UPROPERTY(EditAnywhere, Category = HUD)
	TSubclassOf<class UUserWidget> ReturnToMainMenuWidget; 
	UPROPERTY()
	class UReturnToMainMenu* ReturnToMainMenu; 
	bool bReturnToMainMenuOpen = false; 

	// === 游戏阶段时间变量 ===
	float WarmupTime = 0.f;
	float MatchTime = 120.f;
	float CooldownTime = 0.f;
	float LevelStartingTime = 0.f;
	
	// ✅ 修复严重 Bug：将 uint32 改为 int32，防止时间下溢出变成 4294967295
	int32 CountdownInt = 0; 

	// === HUD 数据缓存 ===
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

	// === Ping警告变量 ===
	float HighPingRunningTime = 0.f; 
	UPROPERTY(EditAnywhere)
	float CheckPingFrequency = 20.f; 

	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f; 

	float PingAnimationRunningTime = 0.f; 
	UPROPERTY(EditAnywhere)
	float HighPingDuration = 5.f; 
};