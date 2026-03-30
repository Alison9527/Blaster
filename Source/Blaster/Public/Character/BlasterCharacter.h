// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BlasterTypes/TurningInPlace.h"
#include "Components/TimelineComponent.h"
#include "Interfaces/InteractWithCrosshairsInterface.h"
#include "BlasterTypes/CombatState.h"
#include "BlasterTypes/Team.h"
#include "BlasterCharacter.generated.h"

class ABlasterWeapon;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLeftGame);

/**
 * 主要角色类，包含移动、射击、换弹、生命/护盾、团队系统、复活特效等
 */
UCLASS()
class BLASTER_API ABlasterCharacter : public ACharacter, public IInteractWithCrosshairsInterface
{
    GENERATED_BODY()

public:
    ABlasterCharacter();
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PostInitializeComponents() override;
    
    // 播放射击蒙太奇，bAiming 决定瞄准/腰射动画段
    void PlayFireMontage(bool bAiming);
    // 播放换弹蒙太奇
    void PlayReloadMontage() const;
    // 播放淘汰蒙太奇
    void PlayElimMontage();
    // 播放扔雷蒙太奇
    void PlayThrowGrenadeMontage();
    // 播放切枪蒙太奇
    void PlaySwapMontage();
    
    // 重写基于移动的复制，用于模拟代理转身
    virtual void OnRep_ReplicatedBasedMovement() override;

    // 角色淘汰入口，bPlayerLeftGame 表示是否因玩家主动退出而淘汰
    void Elim(bool bPlayerLeftGame);
    
    // 多播淘汰，同步所有客户端
    UFUNCTION(NetMulticast, Reliable)
    void MulticastElim(bool bPlayerLeftGame);
    // 角色销毁时处理
    virtual void Destroyed() override;

    // 是否禁用角色操作（死亡/淘汰时使用）
    UPROPERTY(Replicated)
    bool bDisableGameplay = false;

    // 蓝图实现：显示/隐藏狙击镜UI
    UFUNCTION(BlueprintImplementableEvent)
    void ShowSniperScopeWidget(bool bShowScope);
    
    // 更新HUD血量显示
    void UpdateHUDHealth();
    // 更新HUD护盾显示
    void UpdateHUDShield();
    // 更新HUD弹药显示
    void UpdateHUDAmmo();
    // 更新HUD手雷数量显示
    void UpdateHUDGrenade();
    
    // 生成默认武器
    void SpawnDefaultWeapon() const;
    
    // 所有命中碰撞盒子的映射表（服务器回滚用）
    UPROPERTY()
    TMap<FName, class UBoxComponent*> HitCollisionBoxes;

    // 切枪是否完成（用于动画状态）
    bool bFinishedSwapping = false;

    // 服务器请求离开游戏
    UFUNCTION(Server, Reliable)
    void ServerLeaveGame();

    // 玩家离开游戏的委托（本地客户端广播）
    FOnLeftGame OnLeftGame;

    // 多播：成为领先者（显示皇冠）
    UFUNCTION(NetMulticast, Reliable)
    void MulticastGainedTheLead();

    // 多播：失去领先者（隐藏皇冠）
    UFUNCTION(NetMulticast, Reliable)
    void MulticastLostTheLead();

    // 设置团队颜色材质
    void SetTeamColor(ETeam Team);
    
protected:
    virtual void BeginPlay() override;

    // 移动输入绑定
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void EquipButtonPressed();
    void CrouchButtonPressed();
    void ReloadButtonPressed();
    void AimButtonPressed();
    void AimButtonReleased();
    void AimOffset(float DeltaTime);
    void SimProxiesTurn();
    virtual void Jump() override;
    void FireButtonPressed();
    void FireButtonReleased();
    void PlayHitReactMontage();
    void GrenadeButtonPressed();
    // 丢弃或摧毁武器
    void DropOrDestroyWeapon(ABlasterWeapon* Weapon);
    void DropOrDestroyWeapons();
    
    void SetSpawnPoint(); // 根据团队设置出生点
    void OnPlayerStateInitialized(); // 玩家状态初始化后调用

    // 伤害接收函数（绑定到OnTakeAnyDamage）
    UFUNCTION()
    void ReceiveDamage(AActor* DamageActor, float Damage, const UDamageType* DamageType, class AController* InstigatorController, AActor* DamageCauser);
    // 轮询获取必要的组件和控制器
    void PollInit();
    void RotateInPlace(float DeltaTime);
    
    /*
     * 服务端回滚使用的碰撞盒
     */
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Head;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Pelvis;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Spine_02;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Spine_03;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* UpperArm_L;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* UpperArm_R;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* LowerArm_L;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* LowerArm_R;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Hand_L;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Hand_R;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Blanket;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* thigh_L;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* thigh_R;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Calf_L;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Calf_R;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Foot_L;
    UPROPERTY(EditAnywhere)
    class UBoxComponent* Foot_R;

private:
    // 弹簧臂组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    class USpringArmComponent* SpringArmComponent;
    // 摄像机组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent* CameraComponent;

    // 重叠武器（复制用）
    UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
    class ABlasterWeapon* OverlappingWeapon;
    UFUNCTION()
    void OnRep_OverlappingWeapon(ABlasterWeapon* LastWeapon) const;

    /*
     * 核心组件
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    class UCombatComponent* CombatComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    class UBuffComponent* BuffComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    class ULagCompensationComponent* LagCompensationComponent;

    // 服务器装备武器（RPC）
    UFUNCTION(Server, Reliable)
    void SeverEquipButtonPressed();

    float AO_Yaw;                // 瞄准偏移水平角度
    float InterpAO_Yaw;          // 插值的瞄准偏移水平角度
    float AO_Pitch;              // 瞄准偏移垂直角度
    FRotator StartingAimRotation;// 起始瞄准旋转

    ETurningInPlace TurningInPlace;  // 转身状态
    void TurnInPlace(float DeltaTime);
    
    void HideCameralIfCharacterClose();  // 摄像机过近时隐藏角色模型

    UPROPERTY(EditAnywhere)
    float CameraThreshold = 200.f;  // 摄像机距离阈值

    /*
     * 动画蒙太奇
     */
    UPROPERTY(EditAnywhere, Category = Combat)
    class UAnimMontage* FireWeaponMontage;
    UPROPERTY(EditAnywhere, Category = Combat)
    class UAnimMontage* ReloadMontage;
    UPROPERTY(EditAnywhere, Category = Combat)
    class UAnimMontage* HitReactMontage;
    UPROPERTY(EditAnywhere, Category = Combat)
    class UAnimMontage* ElimMontage;
    UPROPERTY(EditAnywhere, Category = Combat)
    class UAnimMontage* ThrowGrenadeMontage;
    UPROPERTY(EditAnywhere, Category = Combat)
    class UAnimMontage* SwapMontage;  // 切枪蒙太奇

    bool bRotateRootBone;            // 是否需要旋转根骨骼（用于转身动画）
    float TurnThreshold = 15.f;       // 转身阈值
    FRotator ProxyRotationLastFrame;  // 上一帧代理旋转
    FRotator ProxyRotation;           // 当前帧代理旋转
    float ProxyYaw;                   // 代理的Yaw变化
    float TimeSinceLastMovementReplication; // 距离上次移动复制的时间
    float CalculateSpeed();           // 计算水平速度大小

    /*
     * 角色生命值
     */
    UPROPERTY(EditAnywhere, Category = "Player States")
    float MaxHealth = 100.f;
    UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleAnywhere, Category = "Player States")
    float Health = 100.f;
    UFUNCTION()
    void OnRep_Health(float LastHealth);
    
    /*
     * 角色护盾
     */
    UPROPERTY(EditAnywhere, Category = "Player States")    
    float MaxShield = 100.f;
    UPROPERTY(ReplicatedUsing = OnRep_Shield, EditAnywhere, Category = "Player States")
    float Shield = 100.f;
    UFUNCTION()
    void OnRep_Shield(float LastShield);

    /*
     * 团队颜色材质
     */
    UPROPERTY(EditAnywhere, Category = Elim)
    UMaterialInstance* RedDissolveMathInst;
    UPROPERTY(EditAnywhere, Category = Elim)
    UMaterialInstance* RedMaterial;
    UPROPERTY(EditAnywhere, Category = Elim)
    UMaterialInstance* BlueDissolveMathInst;
    UPROPERTY(EditAnywhere, Category = Elim)
    UMaterialInstance* BlueMaterial;
    UPROPERTY(EditAnywhere, Category = Elim)
    UMaterialInstance* OriginalMaterial;
    
    UPROPERTY()
    class ABlasterPlayerController* BlasterPlayerController;

    bool bEliminated = false;   // 是否已淘汰
    bool bLeftGame = false;     // 是否主动退出游戏

    FTimerHandle ElimTimer;     // 淘汰计时器
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    float ElimDelay = 3.f;      // 淘汰后等待复活时间
    void ElimTimerFinished();

    /*
     * 溶解效果
     */
    UPROPERTY(VisibleAnywhere)
    UTimelineComponent* DissolveTimeline;
    FOnTimelineFloat DissolveTrack;
    UPROPERTY(EditAnywhere)
    UCurveFloat* DissolveCurve;
    UFUNCTION()
    void UpdateDissolveMaterial(float DissolveValue);
    void StartDissolve();

    UPROPERTY(VisibleAnywhere, Category = Elim)
    UMaterialInstanceDynamic* DynamicDissolveMaterialInstance;
    UPROPERTY(EditAnywhere, Category = Elim)
    UMaterialInstance* DissolveMaterialInstance;
    
    /*
     * 淘汰特效
     */
    UPROPERTY(EditAnywhere)
    UParticleSystem* ElimBotEffect;
    UPROPERTY(VisibleAnywhere)
    UParticleSystemComponent* ElimBotComponent;
    UPROPERTY(EditAnywhere)
    class USoundCue* ElimBotSound;

    UPROPERTY()
    class ABlasterPlayerState* BlasterPlayerState;
    
    // 领先者王冠特效
    UPROPERTY(EditAnywhere)
    class UNiagaraSystem* CrownSystem;
    UPROPERTY()
    class UNiagaraComponent* CrownComponent;

    /*
     * 手雷模型（附着在角色身上）
     */
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* AttachedGrenade;
    
    /*
     * 默认武器类
     */
    UPROPERTY(EditAnywhere)
    TSubclassOf<ABlasterWeapon> DefaultWeaponClass;
    
public:
    void SetOverlappingWeapon(ABlasterWeapon* BlasterWeapon);
    bool IsWeaponEquipped() const;
    bool IsAiming() const;
    FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
    FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
    FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }
    FVector GetHitTarget() const;
    FORCEINLINE UCameraComponent* GetCameraComponent() const { return CameraComponent; }
    FORCEINLINE bool ShouldRotateRootBone() const { return bRotateRootBone; }
    FORCEINLINE bool IsEliminated() const { return bEliminated; }
    FORCEINLINE float GetHealth() const { return Health; }
    FORCEINLINE void SetHealth(float HealthAmount) { Health = HealthAmount; }
    FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
    FORCEINLINE float GetShield() const { return Shield; }
    FORCEINLINE void SetShield(float ShieldAmount) { Shield = ShieldAmount; }
    FORCEINLINE float GetMaxShield() const { return MaxShield; }
    ECombatState GetCombatState() const;
    FORCEINLINE UCombatComponent* GetCombatComponent() const { return CombatComponent; }
    FORCEINLINE bool GetDisableGameplay() const { return bDisableGameplay; }
    FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
    FORCEINLINE UStaticMeshComponent* GetAttachedGrenade() const { return AttachedGrenade; }
    FORCEINLINE UBuffComponent* GetBuffComponent() const { return BuffComponent; }
    FORCEINLINE ULagCompensationComponent* GetLagCompensationComponent() const { return LagCompensationComponent; }
    
    bool IsLocallyReloading() const;
    
    // 旗帜相关
    bool IsHoldingTheFlag() const;
    void SetHoldingTheFlag(bool bHolding) const;
    ETeam GetTeam();
    
    ABlasterWeapon* GetEquippedWeapon() const;

    void CalculateAO_Pitch();
};