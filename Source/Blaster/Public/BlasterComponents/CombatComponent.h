// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HUD/BlasterHUD.h"
#include "Weapon/WeaponTypes.h"
#include "BlasterTypes/CombatState.h"
#include "CombatComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UCombatComponent : public UActorComponent
{
    // UE反射系统宏，生成类的基础模板代码
    GENERATED_BODY()

    // ==============================================================================
    // 1. 基础架构与核心引用 (Base Architecture & Core References)
    // ==============================================================================
public: 
    // 构造函数，用于初始化组件默认值
    UCombatComponent(); 
    // 声明友元类，允许 ABlasterCharacter 直接访问本组件的私有/保护成员
    friend class ABlasterCharacter; 
    // 组件的帧更新函数
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override; 
    // 注册需要进行网络同步（Replication）的变量
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override; 

protected:
    // 组件初始化时调用，通常用于绑定事件或获取引用
    virtual void BeginPlay() override;

private:
    // 指向拥有此组件的角色指针（使用 UPROPERTY 防止被垃圾回收）
    UPROPERTY()
    class ABlasterCharacter* BlasterCharacter; 

    // 指向玩家控制器的指针
    UPROPERTY()
    class ABlasterPlayerController* BlasterPlayerController; 

    // 指向玩家 HUD（UI界面）的指针
    UPROPERTY()
    class ABlasterHUD* HUD; 

    // ==============================================================================
    // 2. 武器装备与切换 (Weapon Lifecycle)
    // ==============================================================================
public:
    // 装备指定武器（通常在服务器调用）
    void EquipWeapon(class ABlasterWeapon* WeaponToEquip); 
    // 主副武器切换逻辑
    void SwapWeapons(); 
    // 判断当前状态是否允许切换武器
    bool ShouldSwapWeapons() const; 
    // 获取当前手中装备的武器
    ABlasterWeapon* GetEquippedWeapon() const { return EquippedWeapon; } 

    // 暴露给蓝图的函数，作为动画通知 (AnimNotify) 的回调，用于标记切枪动作彻底结束
    UFUNCTION(BlueprintCallable)
    void FinishSwap(); 

    // 暴露给蓝图的函数，作为动画通知的回调：当手摸到背后的枪时，执行实际的武器指针与模型位置交换
    UFUNCTION(BlueprintCallable)
    void FinishSwapAttachWeapons(); 

protected:
    // 网络回调函数：当 EquippedWeapon (主武器) 从服务器同步到客户端时触发
    UFUNCTION()
    void OnRep_EquippedWeapon(); 
    
    // 网络回调函数：当 SecondaryWeapon (副武器) 从服务器同步到客户端时触发
    UFUNCTION()
    void OnRep_SecondaryWeapon() const; 

    // 丢弃当前装备的武器
    void DropEquippedWeapon(); 
    // 将指定 Actor (武器) 挂载到角色的右手骨骼插槽上
    void AttachActorToRightHand(class AActor* ActorToAttach) const; 
    // 将指定 Actor (武器) 挂载到角色的左手骨骼插槽上
    void AttachActorToLeftHand(class AActor* ActorToAttach) const; 
    // 夺旗模式专用：将旗帜挂载到左手上
    void AttachFlagToLeftHand(class AActor* Flag); 
    // 将指定 Actor (副武器) 挂载到角色的背包（背部）插槽上
    void AttachActorToBackpack(class AActor* ActorToAttach) const; 
    // 内部逻辑：装备主武器
    void EquipPrimaryWeapon(class ABlasterWeapon* WeaponToEquip); 
    // 内部逻辑：装备副武器
    void EquipSecondaryWeapon(class ABlasterWeapon* WeaponToEquip); 
    // 播放装备武器时的音效
    void PlayEquipWeaponSound(const class ABlasterWeapon* WeaponToEquip) const; 

private:
    // 当前手持的主武器，ReplicatedUsing 表示该变量会被网络同步，且同步时客户端会调用 OnRep_EquippedWeapon
    UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
    class ABlasterWeapon* EquippedWeapon; 
    
    // 当前背在背后的副武器，同样支持网络同步和回调
    UPROPERTY(ReplicatedUsing = OnRep_SecondaryWeapon)
    class ABlasterWeapon* SecondaryWeapon; 

    // ==============================================================================
    // 3. 射击核心逻辑 (Firing System)
    // ==============================================================================
public:
    // 玩家按下或松开开火键时调用
    void FireButtonPressed(bool bPressed); 

protected:
    // 执行开火的主逻辑分支
    void Fire(); 
    // 发射抛射物类武器（如火箭筒）
    void FireProjectileWeapon(); 
    // 发射射线检测类武器（如步枪、手枪）
    void FireHitScanWeapon(); 
    // 发射霰弹枪（多发弹丸射线检测）
    void FireShotgun(); 
    
    // 本地端执行开火表现（播放音效、枪口火焰等），参数使用经过网络优化的 FVector
    void LocalFire(const FVector_NetQuantize& TraceHitTarget); 
    // 霰弹枪的本地端开火表现（处理多个命中目标）
    void ShotgunLocalFire(const TArray<FVector_NetQuantize>& HitTargets); 

    // 服务器 RPC：客户端请求服务器开火。
    // Reliable: 必定送达；WithValidation: 启用安全校验（防作弊）；FireDelay: 传入射击间隔用于服务器验证射速
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerFire(const FVector_NetQuantize& TraceHitTarget, float FireDelay); 

    // 多播 RPC：服务器通知所有客户端播放某人的开火表现
    UFUNCTION(NetMulticast, Reliable)
    void MulticastFire(const FVector_NetQuantize& TraceHitTarget); 
    
    // 霰弹枪的服务器 RPC 请求（携带多个弹丸落点）
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerShotgunFire(const TArray<FVector_NetQuantize>& HitTargets, float FireDelay); 
    
    // 霰弹枪的多播 RPC 通知
    UFUNCTION(NetMulticast, Reliable)
    void MulticastShotgunFire(const TArray<FVector_NetQuantize>& HitTargets); 

private:
    // 控制射速的定时器句柄
    FTimerHandle FireTimer; 
    // 标识当前是否可以开火（受射速、换弹等状态影响）
    bool bCanFire = true; 
    // 记录玩家当前是否按住了开火键（用于全自动武器）
    bool bFireButtonPressed; 

    // 启动射击间隔定时器
    void StartFireTimer(); 
    // 定时器结束回调：重置 bCanFire 状态，如果按住左键则继续开火
    void FireTimerFinished(); 
    // 判断当前弹药和状态是否满足开火条件
    bool CanFire() const; 

    // ==============================================================================
    // 4. 准星与瞄准 (Aiming & HUD)
    // ==============================================================================
public:
    // 获取当前是否处于机瞄状态
    bool GetAiming() const { return bAiming; } 

protected:
    // 设置瞄准状态（本地先行预测）
    void SetAiming(bool bIsAiming); 

    // 服务器 RPC：通知服务器玩家改变了瞄准状态
    UFUNCTION(Server, Reliable)
    void ServerSetAiming(bool bIsAiming); 

    // 从屏幕中心发出射线检测命中目标，用于更新准星变红等 UI 提示
    void TraceUnderCrosshairs(FHitResult& TraceHitResult); 
    // 计算并设置 HUD 准星的扩散与收缩（受移动、跳跃、开火影响）
    void SetHUDCrosshairs(float DeltaTime); 

private:
    // 是否正在瞄准。ReplicatedUsing 同步给其他客户端用于播放动画
    UPROPERTY(ReplicatedUsing = OnRep_Aiming)
    bool bAiming = false; 
    
    // 玩家是否按下了瞄准键
    bool bAimButtonPressed = false; 
    
    // 瞄准状态的网络回调
    UFUNCTION()
    void OnRep_Aiming(); 

    // 以下四个变量用于计算准星的扩散程度
    float CrosshairVelocityFactor;  // 速度影响因子
    float CrosshairInAirFactor;     // 空中跳跃影响因子
    float CrosshairAimFactor;       // 瞄准收缩因子
    float CrosshairShootingFactor;  // 连续开火后坐力扩散因子
    
    // 准星射线检测命中的三维世界坐标
    FVector HitTarget; 
    // 打包传递给 HUD 的准星纹理等数据
    FHUDPackage HUDPackage; 

    // 玩家的默认视野范围 (Field of View)
    float DefaultFOV; 
    // 瞄准时的视野范围 (放大效果)
    UPROPERTY(EditAnywhere, Category = "Combat")
    float ZoomedFOV = 30.f; 
    // 当前帧的实际视野范围
    float CurrentFOV; 
    // 瞄准时 FOV 缩放的平滑插值速度
    UPROPERTY(EditAnywhere, Category = "Combat")
    float ZoomInterpSpeed = 20.f; 
    // 执行 FOV 平滑过渡的函数
    void InterpFOV(float DeltaTime); 

    // ==============================================================================
    // 5. 换弹与弹药管理 (Reload & Ammo)
    // ==============================================================================
public:
    // 触发换弹逻辑
    void Reload(); 
    // 蓝图可调用的动画通知回调：常规武器换弹动作结束时调用
    UFUNCTION(BlueprintCallable)
    void FinishReloading(); 
    
    // 蓝图可调用的动画通知回调：霰弹枪单发装填动画循环时调用
    UFUNCTION(BlueprintCallable)
    void ShotgunShellReload(); 
    // 霰弹枪装填完毕后，跳转到结束动画片段
    void JumpToShotgunEnd() const; 
    
    // 拾取弹药时的处理函数
    void PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount); 
    // 标识本地客户端是否正在播放换弹动画
    bool bLocallyReloading = false; 

protected:
    // 服务器 RPC：处理实际的换弹扣除备弹逻辑
    UFUNCTION(Server, Reliable)
    void ServerReload(); 
    // 内部处理换弹状态切换与动画播放
    void HandleReload() const; 
    // 计算当前需要补充多少发子弹
    int32 AmountToReload(); 
    // 武器弹匣为空时自动换弹
    void ReloadEmptyWeapon(); 

private:
    // 玩家当前持有的备弹量（当前持有武器类型对应的备弹）
    UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
    int32 CarriedAmmo; 
    
    // 备弹量更新的网络回调，用于刷新 UI
    UFUNCTION()
    void OnRep_CarriedAmmo(); 
    
    // 存储玩家每种武器类型对应的备弹数量字典
    TMap<EWeaponType, int32> CarriedAmmoMap; 

    // 修复弹药拾取 Bug 所需的备弹上限控制
    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 MaxCarriedAmmo = 500; 
    
    // 换弹完成后，更新武器弹匣和备弹的数值
    void UpdateAmmoValues(); 
    // 霰弹枪单发装填的数值更新逻辑
    void UpdateShotgunAmmoValues(); 
    // 切换武器时，更新当前 UI 显示的备弹量
    void UpdateCarriedAmmo(); 
    // 初始化角色出生时的备弹配置
    void InitializeCarriedAmmo(); 

    // ==============================================================================
    // 6. 手雷系统 (Grenades)
    // ==============================================================================
public:
    // 内联函数：快速获取当前手雷数量
    FORCEINLINE int32 GetGrenades() const { return Grenades; } 
    
    // 动画通知回调：投掷手雷动作结束，恢复正常状态
    UFUNCTION(BlueprintCallable)
    void ThrowGrenadeFinished(); 
    
    // 动画通知回调：手雷脱手瞬间，生成手雷抛射物
    UFUNCTION(BlueprintCallable)
    void LaunchGrenade(); 

    // 服务器 RPC：在服务器端实际生成手雷抛射物（带目标方向）
    UFUNCTION(Server, Reliable)
    void ServerLaunchGrenade(const FVector_NetQuantize& Target); 

protected:
    // 触发投掷手雷的逻辑与动画
    void ThrowGrenade(); 
    // 服务器 RPC：通知服务器玩家开始投掷手雷
    UFUNCTION(Server, Reliable)
    void ServerThrowGrenade(); 
    
    // 投掷准备期间，控制角色手上用来做表现的假手雷模型的显示与隐藏
    void ShowAttachedGrenade(bool bShowGrenade) const; 
    // 更新 UI 上的手雷数量显示
    void UpdateHUDGrenades(); 

private:
    // 玩家当前持有的手雷数量，带网络同步
    UPROPERTY(ReplicatedUsing = OnRep_Grenades)
    int32 Grenades = 4; 

    // 手雷最大携带上限
    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 MaxGrenades = 4; 
    
    // 手雷数量同步回调，刷新 UI
    UFUNCTION()
    void OnRep_Grenades(); 
    
    // 手雷抛射物的类模板（在蓝图中配置具体的蓝图类）
    UPROPERTY(EditAnywhere)
    TSubclassOf<class AProjectile> GrenadeClass; 

    // ==============================================================================
    // 7. 战斗状态与属性配置 (States & Config)
    // ==============================================================================
private:
    // 角色当前的战斗状态（空闲、换弹中、扔手雷中等），控制逻辑锁
    UPROPERTY(ReplicatedUsing = OnRep_CombatState)
    ECombatState CombatState = ECombatState::ECS_Unoccupied; 

    // 战斗状态同步回调
    UFUNCTION()
    void OnRep_CombatState(); 

    // CTF (Capture The Flag) 夺旗模式属性：是否正在拿旗
    UPROPERTY(ReplicatedUsing = OnRep_HoldingTheFlag)
    bool bHoldingTheFlag = false;

    // 拿旗状态同步回调
    UFUNCTION()
    void OnRep_HoldingTheFlag() const;

    // 夺旗模式中的旗帜实体引用（旗帜通常被视为一种特殊的近战武器或道具）
    UPROPERTY(ReplicatedUsing = OnRep_Flag)
    ABlasterWeapon* TheFlag;

    // 旗帜同步回调
    UFUNCTION()
    void OnRep_Flag();

    // 角色默认的移动速度
    UPROPERTY(EditAnywhere)
    float BaseWalkSpeed; 

    // 角色瞄准时的移动速度（通常会减速）
    UPROPERTY(EditAnywhere)
    float AimWalkSpeed; 

    // 以下为不同武器在角色出生时默认携带的初始备弹量配置
    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 StartingARAmmo = 30;         // 突击步枪初始备弹

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 StartingRocketAmmo = 4;      // 火箭筒初始备弹

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 StartingPistolAmmo = 15;     // 手枪初始备弹

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 StartingSMGAmmo = 30;        // 冲锋枪初始备弹

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 StartingShotgunAmmo = 8;     // 霰弹枪初始备弹

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 StartingSniperAmmo = 10;     // 狙击枪初始备弹

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 StartingGrenadeLauncherAmmo = 4; // 榴弹发射器初始备弹
};