// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

// 声明一个动态多播委托，用于在Ping值过高时广播事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHighPingDelegate, bool, bPingTooHigh);

/**
 * 玩家控制器类，负责处理客户端输入、HUD更新以及与服务器的时间/状态同步
 */
UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 重写OnPossess，当控制器附身到Pawn时调用
	virtual void OnPossess(APawn* InPawn) override;
	// 重写Tick函数，每帧调用
	virtual void Tick(float DeltaTime) override;

	// === HUD 更新相关函数 ===
	void SetHUDHealth(float Health, float MaxHealth); // 更新生命值
	void SetHUDShield(float Shield, float MaxShield); // 更新护盾值
	void SetHUDScore(float Score);                    // 更新得分
	void SetHUDDefeats(int32 Defeats);                // 更新击败数
	void SetHUDWeaponAmmo(int32 Ammo);                // 更新当前武器弹药
	void SetHUDCarriedAmmo(int32 Ammo);               // 更新备用弹药
	void SetHUDMatchCountdown(float CountdownTime);   // 更新比赛倒计时
	void SetHUDAnnouncementCountdown(float CountdownTime); // 更新公告界面的倒计时（准备阶段）
	void SetHUDGrenades(int32 Grenades);              // 更新手雷数量
	void SetHUDRedTeamScore(int32 RedScore);          // 更新红队得分
	void SetHUDBlueTeamScore(int32 BlueScore);        // 更新蓝队得分

	// 隐藏或初始化团队比分UI
	void HideTeamScores();
	void InitTeamScores();

	// === 网络与同步相关 ===
	virtual float GetServerTime(); // 获取与服务器同步后的时间
	virtual void ReceivedPlayer() override; // 客户端接收到玩家时调用，用于尽早同步时间

	// 比赛状态处理
	void OnMatchStateSet(FName State, bool bTeamMatch = false);
	void HandleMatchHasStarted(bool bTeamMatch = false);
	void HandleCooldown();

	// 广播击杀信息（服务器调用）
	void BroadcastElim(APlayerState* Attacker, APlayerState* Victim);

	float SingleTripTime = 0.f; // 客户端到服务器的单程时间

	// Ping过高委托实例
	FHighPingDelegate HighPingDelegate;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupInputComponent() override; // 绑定输入

	void SetHUDTime(); // 计算并设置HUD显示的时间
	void PollInit();   // 轮询检查HUD和相关类是否就绪并初始化

	/*
	 * 客户端与服务器时间同步核心逻辑
	 */
	// 客户端请求服务器当前时间
	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	// 服务器报告当前时间给客户端
	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	float ClientServerDelta = 0.f; // 客户端与服务器的时间差值

	UPROPERTY(EditAnywhere, Category = "Time")
	float TimeSyncFrequency = 5.f; // 时间同步频率（秒）

	float TimeSyncRunningTime = 0.f; // 记录距上次同步经过的时间
	void CheckTimeSync(float DeltaTime); // 检查是否需要同步时间

	// === 比赛状态同步 ===
	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState; // 当前比赛状态

	UFUNCTION()
	void OnRep_MatchState(); // 比赛状态在客户端同步时的回调

	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState(); // 客户端请求服务器检查比赛状态

	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime, bool bIsTeamsMatch); // 客户端中途加入游戏时的初始化

	// === 击杀公告 ===
	UFUNCTION(Client, Reliable)
	void ClientElimAnnouncement(APlayerState* Attacker, APlayerState* Victim);

	// 是否显示团队比分（需要网络同步）
	UPROPERTY(Replicated)
	bool bShowTeamScores = false;

	// === Ping值检测逻辑 ===
	void HighPingWarning(); // 显示高Ping警告图标
	void StopHighPingWarning(); // 隐藏高Ping警告图标
	void CheckPing(float DeltaTime); // 检查当前Ping值

	// 向服务器报告Ping状态
	UFUNCTION(Server, Reliable)
	void ServerReportPingStatus(bool bHighPing);

	// 显示返回主菜单的Widget
	void ShowReturnToMainMenu();

	// 获取比赛结束时的结算文本
	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players);
	FString GetTeamsInfoText(class ABlasterGameState* BlasterGameState);

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay; // HUD覆盖层指针

private:
	UPROPERTY()
	class ABlasterHUD* BlasterHUD = nullptr; // HUD类指针

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode = nullptr; // 游戏模式指针（仅服务器有效）

	/* 返回主菜单相关 */
	UPROPERTY(EditAnywhere, Category = HUD)
	TSubclassOf<class UUserWidget> ReturnToMainMenuWdiget; // 菜单蓝图类
	UPROPERTY()
	class UReturnToMainMenu* ReturnToMainMenu; // 菜单实例
	bool bReturnToMainMenuOpen = false; // 菜单是否处于打开状态

	// === 游戏阶段时间变量 ===
	float WarmupTime = 0.f;
	float MatchTime = 120.f;
	float CooldownTime = 0.f;
	float LevelStartingTime = 0.f;
	uint32 CountdownInt = 0; // 用于避免每帧更新文本，只在秒数变化时更新

	// === HUD 数据缓存（防止HUD未加载完时数据丢失） ===
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
	float HighPingRunningTime = 0.f; // 检查Ping计时期
	UPROPERTY(EditAnywhere)
	float CheckPingFrequency = 20.f; // 每隔20秒检查一次Ping

	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f; // 触发警告的Ping值阈值

	float PingAnimationRunningTime = 0.f; // 警告动画播放计时期
	UPROPERTY(EditAnywhere)
	float HighPingDuration = 5.f; // 警告图标显示的持续时间
};