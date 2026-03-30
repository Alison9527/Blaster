// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterComponents/CombatComponent.h"
#include "Character/BlasterCharacter.h"
#include "Weapon/BlasterWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "HUD/BlasterHUD.h"
#include "Sound/SoundCue.h"
#include "Weapon/Projectile.h"
#include "Weapon/Shotgun.h"
#include "Weapon/WeaponTypes.h"

// ==============================================================================
// 1. 初始化与生命周期 (Initialization & Lifecycle)
// ==============================================================================

// 构造函数
UCombatComponent::UCombatComponent()
{
    // 允许组件每帧调用 TickComponent()
    PrimaryComponentTick.bCanEverTick = true;
    // 设定角色不瞄准时的默认行走速度
    BaseWalkSpeed = 600.f;
    // 设定角色瞄准时的行走速度（减速）
    AimWalkSpeed = 450.f;
}

// 游戏开始或组件生成时调用
void UCombatComponent::BeginPlay()
{
    // 必须调用父类的 BeginPlay
    Super::BeginPlay();

    // 如果成功获取到持有此组件的角色指针
    if (BlasterCharacter)
    {
        // 初始化角色的最大行走速度为基础速度
        BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
       
        // 如果角色拥有摄像机组件
        if (BlasterCharacter->GetCameraComponent())
        {
            // 记录当前摄像机的初始 FOV 作为默认 FOV
            DefaultFOV = BlasterCharacter->GetCameraComponent()->FieldOfView;
            // 初始化当前 FOV
            CurrentFOV = DefaultFOV;
        }
        
        // 只有服务器有权限初始化备弹（防止客户端作弊）
        if (BlasterCharacter->HasAuthority())
        {
            InitializeCarriedAmmo();
        }
    }
}

// 每帧更新函数
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // 必须调用父类的 TickComponent
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // 准星射线检测和 UI 更新只在本地控制的玩家端执行（节省服务器性能，且 UI 本就是本地的）
    if (BlasterCharacter && BlasterCharacter->IsLocallyControlled())
    {
        // 声明一个 HitResult 用于接收射线检测结果
        FHitResult HitResult;
        // 执行从屏幕中心发出的射线检测
        TraceUnderCrosshairs(HitResult);   
        // 记录射线命中的三维坐标，如果没有命中任何东西，在 TraceUnderCrosshairs 内部会被强制赋予一个远点坐标
        HitTarget = HitResult.ImpactPoint; 
        
        // 根据角色状态（移动、跳跃、开火）计算并更新准星的扩散程度
        SetHUDCrosshairs(DeltaTime);       
        // 平滑过渡摄像机的 FOV（用于瞄准放大效果）
        InterpFOV(DeltaTime);              
    }
}

// 注册需要网络同步的变量
void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 同步主武器指针给所有客户端
    DOREPLIFETIME(UCombatComponent, EquippedWeapon); 
    // 同步瞄准状态给所有客户端（用于播放瞄准动画）
    DOREPLIFETIME(UCombatComponent, bAiming);        
    // 同步备弹量。COND_OwnerOnly 表示只同步给拥有该组件的客户端本身，不需要让其他玩家知道你有多少子弹
    DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly); 
    // 同步角色的战斗状态（开火、换弹、切枪等）
    DOREPLIFETIME(UCombatComponent, CombatState);    
    // 同步手雷数量给所有客户端
    DOREPLIFETIME(UCombatComponent, Grenades);       
    // 同步副武器指针给所有客户端
    DOREPLIFETIME(UCombatComponent, SecondaryWeapon);
    // 夺旗模式：同步玩家是否拿着旗帜
    DOREPLIFETIME(UCombatComponent, bHoldingTheFlag);
    // 夺旗模式：同步旗帜武器的实体引用
    DOREPLIFETIME(UCombatComponent, TheFlag);
}

// ==============================================================================
// 2. 武器装备与切换 (Weapon Equipping & Swapping)
// ==============================================================================

// 装备武器（通常由服务器调用）
void UCombatComponent::EquipWeapon(ABlasterWeapon* WeaponToEquip)
{
    // 安全检查
    if (BlasterCharacter == nullptr || WeaponToEquip == nullptr) return;
    // 只有在空闲状态下才能拾取/装备新武器
    if (CombatState != ECombatState::ECS_Unoccupied) return;

    // CTF夺旗模式特殊处理
    if (WeaponToEquip->GetWeaponType() == EWeaponType::EWT_Flag)
    {
        // 拿旗时强制角色下蹲（通常是设计要求）
        BlasterCharacter->Crouch(); 
        // 标记正在拿旗
        bHoldingTheFlag = true;
        // 设置武器状态为已装备
        WeaponToEquip->SetWeaponState(EWeaponState::EWS_Equipped);
        // 将旗帜挂载到特定的左手插槽
        AttachFlagToLeftHand(WeaponToEquip);
        // 设置旗帜的 Owner 为当前角色
        WeaponToEquip->SetOwner(BlasterCharacter);
        // 记录旗帜引用
        TheFlag = WeaponToEquip;
    }
    // 常规武器处理
    else
    {
        // 如果手里已经有主武器，且背上没有副武器，则将新武器装备为副武器
        if (EquippedWeapon != nullptr && SecondaryWeapon == nullptr)
        {
            EquipSecondaryWeapon(WeaponToEquip);
        }
        // 否则（手里没武器，或者主副武器都有），直接替换或装备为主武器
        else
        {
            EquipPrimaryWeapon(WeaponToEquip);
        }
        
        // 拿到枪后，角色朝向不再跟随移动方向，而是跟随控制器的 Yaw（鼠标转向）
        BlasterCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
        BlasterCharacter->bUseControllerRotationYaw = true;
    }
}

// 触发主副武器切换逻辑
void UCombatComponent::SwapWeapons()
{
    // 只有空闲状态下，且有权威性（服务器）才能执行切枪
    if (CombatState != ECombatState::ECS_Unoccupied || BlasterCharacter == nullptr || !BlasterCharacter->HasAuthority()) return;    
    
    // 播放角色蓝图中的切枪蒙太奇动画
    BlasterCharacter->PlaySwapMontage();
    // 改变战斗状态为正在切枪，锁定其他动作（如开火）
    CombatState = ECombatState::ECS_SwappingWeapon;
    
    // 如果有副武器，关闭其自定义深度（即关闭掉落物高亮描边等效果）
    if (SecondaryWeapon) SecondaryWeapon->EnableCustomDepth(false);
}

// 动画通知 (AnimNotify) 回调：切枪动作彻底结束
void UCombatComponent::FinishSwap()
{
    // 在服务器端恢复空闲状态
    if (BlasterCharacter && BlasterCharacter->HasAuthority())
    {
        CombatState = ECombatState::ECS_Unoccupied;
    }
    // 恢复副武器的高亮或物理状态（如果有需求）
    if (SecondaryWeapon) SecondaryWeapon->EnableCustomDepth(true);
}

// 动画通知回调：手摸到背后瞬间，执行实际的武器指针与模型插槽交换
void UCombatComponent::FinishSwapAttachWeapons()
{
    if (BlasterCharacter == nullptr || !BlasterCharacter->HasAuthority()) return;
    // 播放拿出新武器的音效
    PlayEquipWeaponSound(SecondaryWeapon);
    
    // 交换主副武器的指针
    ABlasterWeapon* TempWeapon = EquippedWeapon;
    EquippedWeapon = SecondaryWeapon;
    SecondaryWeapon = TempWeapon;

    // 更新新主武器的状态和位置
    EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
    AttachActorToRightHand(EquippedWeapon);
    EquippedWeapon->SetHUDAmmo(); // 刷新 UI 弹匣弹药
    UpdateCarriedAmmo();          // 刷新 UI 备弹量

    // 更新新副武器的状态和位置（挂在背上）
    SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
    AttachActorToBackpack(SecondaryWeapon);
}

// 内部逻辑：装备为主武器
void UCombatComponent::EquipPrimaryWeapon(ABlasterWeapon* WeaponToEquip)
{
    if (WeaponToEquip == nullptr) return;
    // 如果手里本来有武器，先丢弃它
    DropEquippedWeapon(); 
    
    // 指针赋值
    EquippedWeapon = WeaponToEquip;
    // 设置武器状态
    EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped); 
    // 将武器挂载到右手
    AttachActorToRightHand(EquippedWeapon); 
    // 设置所有者，以便进行网络 RPC
    EquippedWeapon->SetOwner(BlasterCharacter); 
    // 初始化 UI 弹药显示
    EquippedWeapon->SetHUDAmmo();               
    // 更新备弹显示
    UpdateCarriedAmmo();                        
    // 播放音效
    PlayEquipWeaponSound(WeaponToEquip);        
    // 如果捡起的枪是空的，尝试自动换弹
    ReloadEmptyWeapon();                        
}

// 内部逻辑：装备为副武器
void UCombatComponent::EquipSecondaryWeapon(ABlasterWeapon* WeaponToEquip)
{
    if (WeaponToEquip == nullptr) return;
    SecondaryWeapon = WeaponToEquip;
    // 设置为副武器状态
    SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary); 
    // 挂载到背包（背后）插槽
    AttachActorToBackpack(WeaponToEquip); 
    PlayEquipWeaponSound(WeaponToEquip);  
    SecondaryWeapon->SetOwner(BlasterCharacter); 
}

// 判断当前是否满足切枪条件（必须主副两把枪都有）
bool UCombatComponent::ShouldSwapWeapons() const
{
    return EquippedWeapon != nullptr && SecondaryWeapon != nullptr;
}

// 丢弃手里的主武器
void UCombatComponent::DropEquippedWeapon()
{
    if (EquippedWeapon)
    {
        // 调用武器自身的丢弃逻辑（脱离父级、开启物理模拟等）
        EquippedWeapon->Dropped();
    }
}

// 网络回调：客户端收到服务器同步的主武器 (EquippedWeapon) 时执行
void UCombatComponent::OnRep_EquippedWeapon()
{
    if (EquippedWeapon && BlasterCharacter)
    {
        // 客户端本地更新武器状态并挂载到右手
        EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
        AttachActorToRightHand(EquippedWeapon);
        
        // 客户端本地修改角色朝向规则
        BlasterCharacter->GetCharacterMovement()->bOrientRotationToMovement = false; 
        BlasterCharacter->bUseControllerRotationYaw = true; 

        PlayEquipWeaponSound(EquippedWeapon);
        EquippedWeapon->EnableCustomDepth(false); // 关闭掉落物高亮
        EquippedWeapon->SetHUDAmmo(); // 客户端更新 UI 弹药
    }
}

// 网络回调：客户端收到服务器同步的副武器 (SecondaryWeapon) 时执行
void UCombatComponent::OnRep_SecondaryWeapon() const
{
    if (SecondaryWeapon && BlasterCharacter)
    {
        // 客户端本地挂载副武器到背上
        SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
        AttachActorToBackpack(SecondaryWeapon);
        PlayEquipWeaponSound(SecondaryWeapon);
    }
}

// ==============================================================================
// 3. 射击与开火逻辑 (Firing Logic & Anti-Cheat Validation)
// ==============================================================================

// 玩家按下或松开左键时调用
void UCombatComponent::FireButtonPressed(bool bPressed)
{
    bFireButtonPressed = bPressed; 
    // 如果按下了，立刻尝试开火
    if (bFireButtonPressed)
    {
        Fire(); 
    }
}

// 开火主路由逻辑
void UCombatComponent::Fire()
{
    // 如果当前状态、弹药、射速允许开火
    if (CanFire())
    {
        bCanFire = false; // 进入开火CD
        if (EquippedWeapon)
        {
            // 给准星施加一个开火扩散系数
            CrosshairShootingFactor = 0.75f; 

            // 根据武器类型调用不同的开火方式
            switch (EquippedWeapon->FireType)
            {
            case EFireType::EFT_Projectile:
                FireProjectileWeapon(); // 抛射物（如火箭筒）
                break;
            case EFireType::EFT_HitScan:
                FireHitScanWeapon();    // 射线检测（如步枪）
                break;
            case EFireType::EFT_Shotgun:
                FireShotgun();          // 霰弹枪
                break;
            default:
                break;
            }
        }
        // 启动射击间隔定时器
        StartFireTimer(); 
    }
}

// 发射抛射物武器
void UCombatComponent::FireProjectileWeapon()
{
    if (EquippedWeapon && BlasterCharacter)
    {
        // 如果武器配置了弹着点散射，则重新计算目标点
        HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
        // 如果是客户端，先在本地模拟开火表现（无延迟感）
        if (!BlasterCharacter->HasAuthority()) LocalFire(HitTarget);
        // 向服务器发送 RPC 请求开火，并带上射速延迟数据用于校验
        ServerFire(HitTarget, EquippedWeapon->FireDelay); 
    }
}

// 发射射线检测武器
void UCombatComponent::FireHitScanWeapon()
{
    if (EquippedWeapon && BlasterCharacter)
    {
        HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
        if (!BlasterCharacter->HasAuthority()) LocalFire(HitTarget);
        ServerFire(HitTarget, EquippedWeapon->FireDelay);
    }
}

// 发射霰弹枪
void UCombatComponent::FireShotgun()
{
    AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
    if (Shotgun && BlasterCharacter)
    {
        // 霰弹枪产生多个弹丸目标点
        TArray<FVector_NetQuantize> HitTargets;
        Shotgun->ShotgunTraceEndWithScatter(HitTarget, HitTargets);
        
        if (!BlasterCharacter->HasAuthority()) ShotgunLocalFire(HitTargets); 
        // 向服务器发送多个目标点的开火请求
        ServerShotgunFire(HitTargets, EquippedWeapon->FireDelay);
    }
}

// 本地表现先行：播放动画、特效、音效（普通武器）
void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget)
{
    if (EquippedWeapon == nullptr) return;
    
    // 只有在空闲状态下才播放开火蒙太奇（防止打断换弹等动作）
    if (BlasterCharacter && CombatState == ECombatState::ECS_Unoccupied)
    {
        // 播放人物开火动画（带机瞄区分）
        BlasterCharacter->PlayFireMontage(bAiming);
        // 调用武器自身的开火逻辑（生成子弹/特效）
        EquippedWeapon->Fire(TraceHitTarget);
    }
}

// 本地表现先行：霰弹枪专用
void UCombatComponent::ShotgunLocalFire(const TArray<FVector_NetQuantize>& HitTargets)
{
    AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
    if (Shotgun == nullptr || BlasterCharacter == nullptr) return;
    
    // 霰弹枪特例：允许在装填过程中开火（打断装填）
    if (CombatState == ECombatState::ECS_Reloading || CombatState == ECombatState::ECS_Unoccupied)
    {
        bLocallyReloading = false; // 取消本地换弹标记
        BlasterCharacter->PlayFireMontage(bAiming);
        Shotgun->FireShotgun(HitTargets); 
        // 恢复空闲状态
        CombatState = ECombatState::ECS_Unoccupied;
    }
}

// 服务器端执行开火 RPC 的具体实现
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget, float FireDelay)
{
    // 服务器收到请求后，直接调用多播，通知所有客户端（包括自身）播放开火表现
    MulticastFire(TraceHitTarget);
}

// 服务器防作弊校验函数：在 Implementation 之前自动调用
bool UCombatComponent::ServerFire_Validate(const FVector_NetQuantize& TraceHitTarget, float FireDelay)
{
    // 检查客户端传来的开火间隔，是否与服务器上配置的武器真实间隔一致
    if (EquippedWeapon)
    {
        // 误差允许在 0.001 秒内。如果客户端恶意修改内存加快射速，此处将返回 false 并断开客户端连接
        bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
        return bNearlyEqual;
    }
    return true;
}

// 多播开火表现的具体实现（所有客户端及服务器都会执行）
void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
    // 如果是本地控制的客户端，它之前已经通过 LocalFire 提前表现过了，这里直接返回，避免重放两次
    if (BlasterCharacter && BlasterCharacter->IsLocallyControlled() && !BlasterCharacter->HasAuthority()) return;
    // 其他玩家（以及服务器上的模拟代理）执行开火表现
    LocalFire(TraceHitTarget); 
}

// 霰弹枪服务器开火实现
void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize>& HitTargets, float FireDelay)
{
    MulticastShotgunFire(HitTargets);
}

// 霰弹枪服务器校验
bool UCombatComponent::ServerShotgunFire_Validate(const TArray<FVector_NetQuantize>& HitTargets, float FireDelay)
{
    if (EquippedWeapon)
    {
        bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
        return bNearlyEqual;
    }
    return true;
}

// 霰弹枪多播实现
void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize>& HitTargets)
{
    if (BlasterCharacter && BlasterCharacter->IsLocallyControlled() && !BlasterCharacter->HasAuthority()) return;
    ShotgunLocalFire(HitTargets);
}

// 开启射速限制定时器
void UCombatComponent::StartFireTimer()
{
    if (EquippedWeapon == nullptr || BlasterCharacter == nullptr) return;
    // 根据武器配置的 FireDelay 时间，设定定时器
    BlasterCharacter->GetWorldTimerManager().SetTimer(
        FireTimer,
        this,
        &UCombatComponent::FireTimerFinished,
        EquippedWeapon->FireDelay
    );
}

// 定时器结束，解除开火限制
void UCombatComponent::FireTimerFinished()
{
    if (EquippedWeapon == nullptr) return;
    bCanFire = true; // 允许再次开火
    
    // 如果玩家一直按着左键，且是全自动武器，立刻再次开火
    if (bFireButtonPressed && EquippedWeapon->bAutomatic)
    {
        Fire();
    }
    // 如果打光了子弹，尝试自动换弹
    ReloadEmptyWeapon(); 
}

// 核心判断：当前是否允许开火
bool UCombatComponent::CanFire() const
{
    if (EquippedWeapon == nullptr) return false;
    
    // 如果本地正在播放换弹动画
    if (bLocallyReloading)
    {
        // 允许霰弹枪在弹匣有子弹的情况下打断换弹并开火
        if (!EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun)
        {
            return true;
        }
        return false;
    }
    // 常规判断：弹匣不为空 && 不在开火CD中 && 角色状态空闲
    return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

// ==============================================================================
// 4. 瞄准、准星与视野 (Aiming, Crosshairs & FOV)
// ==============================================================================

// 设置瞄准状态
void UCombatComponent::SetAiming(bool bIsAiming)
{
    if (BlasterCharacter == nullptr || EquippedWeapon == nullptr) return;
    bAiming = bIsAiming; // 客户端本地立刻赋值，消除延迟感
    ServerSetAiming(bIsAiming); // 通知服务器
    
    // 本地修改行走速度
    if (BlasterCharacter)
    {
        BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
    }
    // 如果是狙击枪且是本地玩家，打开狙击镜 UI
    if (BlasterCharacter->IsLocallyControlled() && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle)
    {
        BlasterCharacter->ShowSniperScopeWidget(bIsAiming);
    }
    // 记录玩家的操作意图
    if (BlasterCharacter->IsLocallyControlled()) bAimButtonPressed = bIsAiming;
}

// 服务器执行瞄准状态修改
void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
    bAiming = bIsAiming;
    // 服务器修改速度，保证网络移动同步计算准确
    if (BlasterCharacter)
    {
        BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = bAiming ? AimWalkSpeed : BaseWalkSpeed;
    }
}

// 瞄准状态的网络回调 (其他客户端收到某人瞄准时)
void UCombatComponent::OnRep_Aiming()
{
    // 如果是本地玩家，确保以玩家当前的按键状态为准，防止网络延迟导致的抖动
    if (BlasterCharacter && BlasterCharacter->IsLocallyControlled())
    {
        bAiming = bAimButtonPressed;
    }
}

// 核心 UI 函数：动态计算准星的形状并传递给 HUD
void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
    if (BlasterCharacter == nullptr || BlasterCharacter->Controller == nullptr) return;
    
    // 缓存获取控制器和 HUD
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(BlasterCharacter->Controller) : BlasterPlayerController;
    if(BlasterPlayerController)
    {
        HUD = HUD == nullptr ? Cast<ABlasterHUD>(BlasterPlayerController->GetHUD()) : HUD;
        if (HUD)
        {
            // 打包当前武器的准星纹理
            if (EquippedWeapon)
            {
                HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
                HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
                HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
                HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
                HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
            }
            else 
            {
                // 空手没准星
                HUDPackage.CrosshairsCenter = nullptr;
                HUDPackage.CrosshairsLeft = nullptr;
                HUDPackage.CrosshairsRight = nullptr;
                HUDPackage.CrosshairsBottom = nullptr;
                HUDPackage.CrosshairsTop = nullptr;
            }
          
            // 计算移动速度对准星的扩散影响 (映射到 0~1 的范围)
            FVector2D WalkSpeedRange(0.f, BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed);
            FVector2D SpreadRange(0.f, 1.f);
            FVector Velocity = BlasterCharacter->GetVelocity();
            Velocity.Z = 0.f; // 忽略垂直速度（如下落）
            CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, SpreadRange, Velocity.Size());
          
            // 跳跃/空中的扩散惩罚
            if (BlasterCharacter->GetCharacterMovement()->IsFalling())
            {
                CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f); 
            }
            else
            {
                // 落地后快速恢复
                CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);  
            }
          
            // 机瞄收缩奖励
            if (bAiming)
            {
                CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.58f, DeltaTime, 30.f);
            }
            else         
            {
                CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
            }
          
            // 开火后坐力快速衰减
            CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);
          
            // 综合计算最终准星扩散值：基础值 + 移动惩罚 + 空中惩罚 - 机瞄奖励 + 后坐力惩罚
            HUDPackage.CrosshairSpread = 0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor + CrosshairShootingFactor;
            HUD->SetHUDPackage(HUDPackage);
        }
    }
}

// 准星射线检测：从屏幕中心向世界发出射线，判断是否对准了目标
void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
    FVector2D ViewportSize;
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize); 
    }
    
    // 获取屏幕中心坐标
    FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
    FVector CrosshairWorldPosition;
    FVector CrosshairWorldDirection;
    
    // 将屏幕坐标转换为 3D 世界空间的位置和方向
    bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
        UGameplayStatics::GetPlayerController(this, 0), 
        CrosshairLocation, 
        CrosshairWorldPosition, 
        CrosshairWorldDirection
    );

    if (bScreenToWorld)
    {
        FVector Start = CrosshairWorldPosition;
        // 微调起点：将射线起点向后推移一点（越过角色自身），防止射线打到玩家自己的后脑勺
        if (BlasterCharacter)
        {
            float DistanceToCharacter = (BlasterCharacter->GetActorLocation() - Start).Size();
            Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f);
        }
        // 计算终点
        FVector End = Start + (CrosshairWorldDirection * TRACE_LENGTH); 
        
        // 发射射线
        bool bHit = GetWorld()->LineTraceSingleByChannel(TraceHitResult, Start, End, ECC_Visibility);
        
        if (bHit)
        {
            // 如果命中了实现了互交接口的 Actor（比如敌人），准星变红
            if (TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>())
            {
                HUDPackage.CrosshairColor = FLinearColor::Red;
            }
            else
            {
                HUDPackage.CrosshairColor = FLinearColor::White;
            }
        }
        else
        {
            // Bug修复：如果朝天开枪什么都没打中，HitResult.ImpactPoint 会是零向量。
            // 必须强制将其赋值为射线的最远端点，否则子弹会默认向世界原点 (0,0,0) 也就是脚下飞去。
            TraceHitResult.ImpactPoint = FVector_NetQuantize(End);
            HUDPackage.CrosshairColor = FLinearColor::White;
        }
    }
}

// 平滑过渡视野 (FOV)，实现开镜缩放效果
void UCombatComponent::InterpFOV(float DeltaTime)
{
    if (EquippedWeapon == nullptr) return;

    if (bAiming)
    {
        CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
    }
    else
    {
        CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
    }
    
    if (BlasterCharacter && BlasterCharacter->GetCameraComponent())
    {
        BlasterCharacter->GetCameraComponent()->SetFieldOfView(CurrentFOV);
    }
}

// ==============================================================================
// 5. 弹药与换弹逻辑 (Ammo & Reloading)
// ==============================================================================

// 初始化角色的默认备弹（仅服务器）
void UCombatComponent::InitializeCarriedAmmo()
{
    CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
    CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
    CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
    CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
    CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
    CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
    CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}

// 拾取弹药包逻辑
void UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount)
{
    if (CarriedAmmoMap.Contains(WeaponType))
    {
        // Bug修复：原版代码这里可能写成了 Clamp(..., 0, CarriedAmmoMap[WeaponType])，导致备弹永远无法增加。
        // 现在改为 MaxCarriedAmmo 保证能正常拾取，且不超过上限。
        CarriedAmmoMap[WeaponType] = FMath::Clamp(CarriedAmmoMap[WeaponType] + AmmoAmount, 0, MaxCarriedAmmo);
        UpdateCarriedAmmo(); 
    }
    
    // 如果手里的枪正好是空弹匣，且类型对应，自动触发换弹
    if (EquippedWeapon && EquippedWeapon->IsEmpty() && EquippedWeapon->GetWeaponType() == WeaponType)
    {
        Reload();
    }
}

// 刷新当前手持武器的备弹 UI 显示
void UCombatComponent::UpdateCarriedAmmo()
{
    if (EquippedWeapon == nullptr) return;
    
    // 从字典中获取对应武器的备弹量
    if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
    {
        CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
    }
    
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(BlasterCharacter->Controller) : BlasterPlayerController;
    if (BlasterPlayerController)
    {
        BlasterPlayerController->SetHUDCarriedAmmo(CarriedAmmo);
    }
}

// 备弹量网络同步回调（客户端刷新 UI）
void UCombatComponent::OnRep_CarriedAmmo()
{
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(BlasterCharacter->Controller) : BlasterPlayerController;
    if (BlasterPlayerController)
    {
        BlasterPlayerController->SetHUDCarriedAmmo(CarriedAmmo);
    }
    
    // 霰弹枪特例：如果备弹已经用光，强制停止换弹循环动画
    bool bJumpToShotgunEnd = CombatState == ECombatState::ECS_Reloading &&
        EquippedWeapon && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun && CarriedAmmo == 0;
    if (bJumpToShotgunEnd)
    {
        JumpToShotgunEnd();
    }
}

// 触发换弹
void UCombatComponent::Reload()
{
    // 条件：有备弹、当前空闲、有武器、弹匣没满、本地没有在换弹
    if (CarriedAmmo > 0 && CombatState == ECombatState::ECS_Unoccupied && EquippedWeapon && !EquippedWeapon->IsFull() && !bLocallyReloading)
    {
        ServerReload();      // 呼叫服务器锁定状态
        HandleReload();      // 本地先行播放动画
        bLocallyReloading = true; // 标记本地换弹状态
    }
}

// 服务器端锁定换弹状态
void UCombatComponent::ServerReload_Implementation()
{
    if (BlasterCharacter == nullptr || EquippedWeapon == nullptr) return;
    
    CombatState = ECombatState::ECS_Reloading; // 服务器切换状态，这会同步给其他客户端
    // 如果服务器同时也是玩家（Listen Server），它自己已经先行 HandleReload 了，不需要再播一遍
    if (!BlasterCharacter->IsLocallyControlled()) HandleReload();
}

// 播放换弹动画
void UCombatComponent::HandleReload() const
{
    if (BlasterCharacter)
    { 
        BlasterCharacter->PlayReloadMontage();
    }
}

// 计算本次换弹需要从备弹中扣除多少子弹
int32 UCombatComponent::AmountToReload()
{
    if (EquippedWeapon == nullptr) return 0;
    
    // 计算弹匣还能装多少
    int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();

    if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
    {
        int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()]; 
        // 取 备弹量 和 弹匣空余量 的最小值
        int32 Least = FMath::Min(AmountCarried, RoomInMag); 
        return FMath::Clamp(RoomInMag, 0, Least); 
    }
    return 0;
}

// 换弹动作结束后（动画通知调用），执行实际的数值更新（服务器端执行）
void UCombatComponent::UpdateAmmoValues()
{
    if (BlasterCharacter == nullptr || EquippedWeapon == nullptr) return;
    
    int32 ReloadAmount = AmountToReload(); 
    
    // 扣除备弹
    if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
    {
        CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
        CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
    }
    
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(BlasterCharacter->Controller) : BlasterPlayerController;
    if (BlasterPlayerController)
    {
        BlasterPlayerController->SetHUDCarriedAmmo(CarriedAmmo);
    }
    
    // 增加弹匣内弹药
    EquippedWeapon->AddAmmo(ReloadAmount);
}

// 霰弹枪专属的单发弹药更新（每次装入一发触发一次）
void UCombatComponent::UpdateShotgunAmmoValues()
{
    if (BlasterCharacter == nullptr || EquippedWeapon == nullptr) return;
    
    if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
    {
        CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= 1;
        CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
    }
    
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(BlasterCharacter->Controller) : BlasterPlayerController;
    if (BlasterPlayerController)
    {
        BlasterPlayerController->SetHUDCarriedAmmo(CarriedAmmo);
    }
    
    EquippedWeapon->AddAmmo(1); 
    bCanFire = true; 
    
    // 如果弹匣满了，或者备弹空了，跳出装填循环
    if (EquippedWeapon->IsFull() || CarriedAmmo == 0)
    {
        JumpToShotgunEnd();
    }
}

// 蓝图动画通知调用：装入一发霰弹
void UCombatComponent::ShotgunShellReload()
{
    if (BlasterCharacter && BlasterCharacter->HasAuthority())
    {
        UpdateShotgunAmmoValues();
    }
}

// 霰弹枪结束装填，跳转到动画蒙太奇的收尾部分
void UCombatComponent::JumpToShotgunEnd() const
{
    UAnimInstance* AnimInstance = BlasterCharacter->GetMesh()->GetAnimInstance();
    if (AnimInstance && BlasterCharacter->GetReloadMontage())
    {
        AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
    }
}

// 蓝图动画通知调用：整个换弹过程彻底结束
void UCombatComponent::FinishReloading()
{
    if (BlasterCharacter == nullptr) return;
    bLocallyReloading = false; 
    
    // 服务器解除锁定状态并更新子弹
    if (BlasterCharacter->HasAuthority())
    {
        CombatState = ECombatState::ECS_Unoccupied; 
        UpdateAmmoValues(); 
    }
    
    // 优化手感：如果换弹时一直按着左键，换弹一结束立刻开火
    if (bFireButtonPressed)
    {
        Fire();
    }
}

// 弹匣空时自动触发换弹
void UCombatComponent::ReloadEmptyWeapon()
{
    if (EquippedWeapon && EquippedWeapon->IsEmpty())
    {
        Reload();
    }
}

// ==============================================================================
// 6. 手雷系统 (Grenades)
// ==============================================================================

// 触发扔手雷
void UCombatComponent::ThrowGrenade()
{
    if (Grenades <= 0) return; 
    if (CombatState != ECombatState::ECS_Unoccupied || EquippedWeapon == nullptr) return; 
    
    CombatState = ECombatState::ECS_ThrowingGrenade; 
    
    // 本地先行播放动画，并将武器移到左手（腾出右手扔雷），显示手上的假手雷
    if (BlasterCharacter && BlasterCharacter->IsLocallyControlled())
    {
        BlasterCharacter->PlayThrowGrenadeMontage();
        AttachActorToLeftHand(EquippedWeapon); 
        ShowAttachedGrenade(true); 
    }
    
    // 客户端通知服务器
    if (BlasterCharacter && !BlasterCharacter->HasAuthority())
    {
        ServerThrowGrenade();
    }
    
    // 服务器扣除手雷数量
    if (BlasterCharacter && BlasterCharacter->HasAuthority())
    {
        Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
        UpdateHUDGrenades();
    }
}

// 服务器扔雷实现
void UCombatComponent::ServerThrowGrenade_Implementation()
{
    if (Grenades <= 0) return;
    CombatState = ECombatState::ECS_ThrowingGrenade;
    
    if (BlasterCharacter)
    {
        BlasterCharacter->PlayThrowGrenadeMontage();
        AttachActorToLeftHand(EquippedWeapon);
        ShowAttachedGrenade(true);
    }
    
    Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
    UpdateHUDGrenades();
}

// 动画通知：手雷扔完，恢复持枪状态
void UCombatComponent::ThrowGrenadeFinished()
{
    CombatState = ECombatState::ECS_Unoccupied;
    AttachActorToRightHand(EquippedWeapon);
}

// 动画通知：手雷脱手瞬间，隐藏假手雷，生成真实的物理手雷
void UCombatComponent::LaunchGrenade()
{
    ShowAttachedGrenade(false);
    if (BlasterCharacter && BlasterCharacter->IsLocallyControlled())
    {
        ServerLaunchGrenade(HitTarget); 
    }
}

// 服务器生成真实手雷抛射物
void UCombatComponent::ServerLaunchGrenade_Implementation(const FVector_NetQuantize& Target)
{
    if (GrenadeClass && BlasterCharacter->GetAttachedGrenade())
    {
        // 从假手雷模型的位置开始生成
        const FVector StartLocation = BlasterCharacter->GetAttachedGrenade()->GetComponentLocation(); 
        // 计算抛射方向
        FVector ToTarget = Target - StartLocation; 
        
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = BlasterCharacter;
        SpawnParams.Instigator = BlasterCharacter;
        
        if (UWorld* World = GetWorld())
        {
            // 生成抛射物实体（Projectile 内部自带物理弹道）
            AProjectile* SpawnedGrenade = World->SpawnActor<AProjectile>(
                GrenadeClass, 
                StartLocation, 
                ToTarget.Rotation(), 
                SpawnParams
            );
        }
    }
}

// 控制假手雷网格体的显示隐藏
void UCombatComponent::ShowAttachedGrenade(bool bShowGrenade) const
{
    if (BlasterCharacter && BlasterCharacter->GetAttachedGrenade())
    {
        BlasterCharacter->GetAttachedGrenade()->SetVisibility(bShowGrenade);
    }
}

// 刷新 UI
void UCombatComponent::UpdateHUDGrenades()
{
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(BlasterCharacter->Controller) : BlasterPlayerController;
    if (BlasterPlayerController)
    {
        BlasterPlayerController->SetHUDGrenades(Grenades);
    }
}

// 同步手雷数量给客户端 UI
void UCombatComponent::OnRep_Grenades()
{
    UpdateHUDGrenades();
}

// ==============================================================================
// 7. 辅助功能与状态同步 (Helpers & State Sync)
// ==============================================================================

// 其他客户端接收到服务器状态改变时的表现同步
void UCombatComponent::OnRep_CombatState()
{
    switch (CombatState)
    {
    case ECombatState::ECS_Reloading:
        // 如果服务器状态变成换弹中，且我是模拟代理（其他玩家），播动画
        if (BlasterCharacter && !BlasterCharacter->IsLocallyControlled()) HandleReload(); 
        break;
    case ECombatState::ECS_Unoccupied:
        // 恢复空闲时，如果一直按着左键，自动开火
        if (bFireButtonPressed) 
        {
            Fire();
        }
        break;
    case ECombatState::ECS_ThrowingGrenade:
        if (BlasterCharacter && !BlasterCharacter->IsLocallyControlled())
        {
            BlasterCharacter->PlayThrowGrenadeMontage(); 
            AttachActorToLeftHand(EquippedWeapon);
            ShowAttachedGrenade(true);
        }
        break;
    case ECombatState::ECS_SwappingWeapon: // 对应切枪
        if (BlasterCharacter && !BlasterCharacter->IsLocallyControlled())
        {
            BlasterCharacter->PlaySwapMontage();
        }
        break;
    default:
        break;
    }
}

// 夺旗模式：同步其他玩家拿旗的表现（比如蹲下）
void UCombatComponent::OnRep_HoldingTheFlag() const
{
    if (bHoldingTheFlag && BlasterCharacter && BlasterCharacter->IsLocallyControlled())
    {
        BlasterCharacter->Crouch();
    }
}

// 夺旗模式：同步旗帜挂载
void UCombatComponent::OnRep_Flag()
{
    if (TheFlag)
    {
        TheFlag->SetWeaponState(EWeaponState::EWS_Equipped);
        AttachFlagToLeftHand(TheFlag);
    }
}

// 挂载辅助函数：右手
void UCombatComponent::AttachActorToRightHand(class AActor* ActorToAttach) const
{
    if (BlasterCharacter == nullptr || BlasterCharacter->GetMesh() == nullptr || ActorToAttach == nullptr) return;
    
    if (const USkeletalMeshSocket* HandSocket = BlasterCharacter->GetMesh()->GetSocketByName("RightHandSocket"))
    {
        HandSocket->AttachActor(ActorToAttach, BlasterCharacter->GetMesh());
    }
}

// 挂载辅助函数：左手（根据武器类型选择不同姿势的插槽）
void UCombatComponent::AttachActorToLeftHand(class AActor* ActorToAttach) const
{
    if (BlasterCharacter == nullptr || BlasterCharacter->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr) return;
    
    // 手枪和冲锋枪单手持握，挂载点可能不同
    bool bUsePistolSocket = EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol || EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SubmachineGun;
    
    const FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LeftHandSocket");

    if (const USkeletalMeshSocket* HandSocket = BlasterCharacter->GetMesh()->GetSocketByName(SocketName))
    {
        HandSocket->AttachActor(ActorToAttach, BlasterCharacter->GetMesh());
    }
}

// 夺旗专用挂载
void UCombatComponent::AttachFlagToLeftHand(class AActor* Flag)
{
    if (BlasterCharacter == nullptr || BlasterCharacter->GetMesh() == nullptr || Flag == nullptr) return;
    
    if (const USkeletalMeshSocket* HandSocket = BlasterCharacter->GetMesh()->GetSocketByName("FlagSocket"))
    {
        HandSocket->AttachActor(Flag, BlasterCharacter->GetMesh());
    }
}

// 挂载辅助函数：背包（背后）
void UCombatComponent::AttachActorToBackpack(class AActor* ActorToAttach) const
{
    if (BlasterCharacter == nullptr || BlasterCharacter->GetMesh() == nullptr || ActorToAttach == nullptr) return;
    
    if (const USkeletalMeshSocket* BackpackSocket = BlasterCharacter->GetMesh()->GetSocketByName("BackpackSocket"))
    {
        BackpackSocket->AttachActor(ActorToAttach, BlasterCharacter->GetMesh());
    }
}

// 播放装备音效
void UCombatComponent::PlayEquipWeaponSound(const ABlasterWeapon* WeaponToEquip) const
{
    if (BlasterCharacter && WeaponToEquip && WeaponToEquip->EquipSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, WeaponToEquip->EquipSound, BlasterCharacter->GetActorLocation());
    }
}