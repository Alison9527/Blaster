#include "Weapon/BlasterWeapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Character/BlasterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Weapon/Casing.h"
#include "BlasterComponents/CombatComponent.h"
#include "Kismet/KismetMathLibrary.h"

// ABlasterWeapon 构造函数
ABlasterWeapon::ABlasterWeapon()
{
    // 禁用 Tick，每帧更新
    PrimaryActorTick.bCanEverTick = false;
    
    // 启用网络同步
    bReplicates = true;
    AActor::SetReplicateMovement(true);

    // 创建武器网格组件
    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    SetRootComponent(WeaponMesh);

    // 设置武器网格的碰撞响应（忽略玩家碰撞，阻挡其他物体）
    WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 启用自定义深度（例如高亮显示）
    WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);
    WeaponMesh->MarkRenderStateDirty();
    EnableCustomDepth(true);

    // 创建碰撞区域球体（用于武器拾取检测）
    AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
    AreaSphere->SetupAttachment(RootComponent);
    AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

    // 创建拾取小部件（用于显示武器信息）
    PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
    PickupWidget->SetupAttachment(RootComponent);
}

// 游戏开始时初始化
void ABlasterWeapon::BeginPlay()
{
    Super::BeginPlay();

    if (PickupWidget)
    {
        PickupWidget->SetVisibility(false); // 隐藏拾取小部件
    }

    // 启用区域碰撞检测
    AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
    AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &ABlasterWeapon::OnSphereOverlap);
    AreaSphere->OnComponentEndOverlap.AddDynamic(this, &ABlasterWeapon::OnSphereEndOverlap);
}

// 每帧更新（目前未使用）
void ABlasterWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// 设置网络同步的属性
void ABlasterWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 网络同步武器状态和服务器端回滚设置
    DOREPLIFETIME(ABlasterWeapon, WeaponState);
    DOREPLIFETIME_CONDITION(ABlasterWeapon, bUseServerSideRewind, COND_OwnerOnly);
}

// 处理武器与其他角色重叠时的逻辑
void ABlasterWeapon::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor))
    {
        // 如果是旗帜且是队友，则不可拾取
        if (WeaponType == EWeaponType::EWT_Flag && BlasterCharacter->GetTeam() == Team) return;
        // 如果角色正在持有旗帜，则不可拾取
        if (BlasterCharacter->IsHoldingTheFlag()) return;

        // 设置当前重叠的武器
        BlasterCharacter->SetOverlappingWeapon(this);
    }
}

// 处理武器与其他角色结束重叠时的逻辑
void ABlasterWeapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor))
    {
        // 如果是旗帜且是队友，则不可拾取
        if (WeaponType == EWeaponType::EWT_Flag && BlasterCharacter->GetTeam() == Team) return;
        // 如果角色正在持有旗帜，则不可拾取
        if (BlasterCharacter->IsHoldingTheFlag()) return;

        // 清除当前重叠的武器
        BlasterCharacter->SetOverlappingWeapon(nullptr);
    }
}

// 处理武器拥有者的同步
void ABlasterWeapon::OnRep_Owner()
{
    Super::OnRep_Owner();
    if (Owner == nullptr)
    {
        BlasterOwnerCharacter = nullptr;
        BlasterOwnerController = nullptr;
    }
    else
    {
        BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(Owner) : BlasterOwnerCharacter;
        if (BlasterOwnerCharacter && BlasterOwnerCharacter->GetEquippedWeapon() == this)
        {
            SetHUDAmmo(); // 更新 HUD 显示的弹药数量
        }
    }
}

// 更新 HUD 上的弹药显示
void ABlasterWeapon::SetHUDAmmo()
{
    BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
    if (BlasterOwnerCharacter)
    {
        BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(BlasterOwnerCharacter->Controller) : BlasterOwnerController;
        if (BlasterOwnerController)
        {
            BlasterOwnerController->SetHUDWeaponAmmo(Ammo); // 设置 HUD 上显示的弹药数量
        }
    }
}

// 执行开火操作
void ABlasterWeapon::Fire(const FVector& HitTarget)
{
    if (WeaponMesh && FireAnimation)
    {
        WeaponMesh->PlayAnimation(FireAnimation, false); // 播放开火动画
    }

    // 弹壳生成
    if (CasingClass)
    {
        if (const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject")))
        {
            FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);
            GetWorld()->SpawnActor<ACasing>(CasingClass, SocketTransform.GetLocation(), SocketTransform.GetRotation().Rotator()); // 生成弹壳
        }
    }

    SpendRound(); // 消耗一发弹药
}

// 消耗一发弹药
void ABlasterWeapon::SpendRound()
{
    Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity); // 确保弹药不会小于 0
    SetHUDAmmo(); // 更新 HUD 显示
    if (HasAuthority()) // 如果是服务器端，更新客户端弹药数量
    {
        ClientUpdateAmmo(Ammo);
    }
    else
    {
        ++Sequence; // 增加序列号
    }
}

// 客户端同步弹药数量
void ABlasterWeapon::ClientUpdateAmmo_Implementation(int32 ServerAmmo)
{
    if (HasAuthority()) return;
    Ammo = ServerAmmo;
    --Sequence;
    Ammo -= Sequence; // 减去序列号
    SetHUDAmmo(); // 更新 HUD 显示
}

// 增加弹药
void ABlasterWeapon::AddAmmo(int32 AmmoToAdd)
{
    Ammo = FMath::Clamp(Ammo + AmmoToAdd, 0, MagCapacity); // 增加弹药并确保不超过弹匣容量
    SetHUDAmmo(); // 更新 HUD 显示
    ClientAddAmmo(AmmoToAdd); // 向客户端同步
}

// 客户端同步增加弹药
void ABlasterWeapon::ClientAddAmmo_Implementation(int32 AmmoToAdd)
{
    if (HasAuthority()) return;
    Ammo = FMath::Clamp(Ammo + AmmoToAdd, 0, MagCapacity); // 增加弹药并确保不超过弹匣容量
    BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;

    // 散弹枪特殊逻辑：如果散弹枪已经满了，跳转到散弹枪结束动画
    bool bIsShotgunFull = BlasterOwnerCharacter 
        && BlasterOwnerCharacter->GetCombatComponent() 
        && WeaponType == EWeaponType::EWT_Shotgun 
        && IsFull();

    if (bIsShotgunFull)
    {
        BlasterOwnerCharacter->GetCombatComponent()->JumpToShotgunEnd(); // 跳转到散弹枪结束动画
    }
    SetHUDAmmo(); // 更新 HUD 显示
}

// 通过网络更新弹药数量
void ABlasterWeapon::MulticastAmmo_Implementation(int32 UpdateAmmo)
{
    Ammo = UpdateAmmo;
}

// 设置武器状态
void ABlasterWeapon::OnWeaponStateSet()
{
    switch (WeaponState)
    {
    case EWeaponState::EWS_Equipped:
        OnEquipped();
        break;
    case EWeaponState::EWS_EquippedSecondary:
        OnEquippedSecondary();
        break;
    case EWeaponState::EWS_Dropped:
        OnDropped();
        break;
    default:
        break;
    }
}

// 处理武器状态同步
void ABlasterWeapon::OnRep_WeaponState()
{
    OnWeaponStateSet();
}

// 处理高延迟时的操作
void ABlasterWeapon::OnPingTooHigh(bool bPingTooHigh)
{
    bUseServerSideRewind = !bPingTooHigh; // 如果高延迟，启用服务器端回滚
}

// 装备武器时的处理
void ABlasterWeapon::OnEquipped()
{
    ShowPickupWidget(false); // 隐藏拾取小部件
    AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 禁用区域碰撞

    // 禁用物理模拟（武器固定在角色身上）
    WeaponMesh->SetSimulatePhysics(false);
    WeaponMesh->SetEnableGravity(false);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 如果是冲锋枪，启用物理模拟
    if (WeaponType == EWeaponType::EWT_SubmachineGun)
    {
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        WeaponMesh->SetEnableGravity(true);
        WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    }

    EnableCustomDepth(false); // 禁用自定义深度

    // 处理高延迟补偿
    BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
    if (BlasterOwnerCharacter && bUseServerSideRewind)
    {
        BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(BlasterOwnerCharacter->Controller) : BlasterOwnerController;
        if (BlasterOwnerController && HasAuthority() && !BlasterOwnerController->HighPingDelegate.IsBound())
        {
            BlasterOwnerController->HighPingDelegate.AddDynamic(this, &ABlasterWeapon::OnPingTooHigh); // 绑定高延迟委托
        }
    }
}

// 丢弃武器时的处理
void ABlasterWeapon::OnDropped()
{
    if (HasAuthority())
    {
        AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 启用区域碰撞查询
    }

    // 启用物理模拟（武器掉落后可以受物理影响）
    WeaponMesh->SetSimulatePhysics(true);
    WeaponMesh->SetEnableGravity(true);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

    WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE); // 更改深度值显示武器
    WeaponMesh->MarkRenderStateDirty();
    EnableCustomDepth(true); // 启用自定义深度

    // 移除高延迟委托
    if (BlasterOwnerController && HasAuthority() && BlasterOwnerController->HighPingDelegate.IsBound())
    {
        BlasterOwnerController->HighPingDelegate.RemoveDynamic(this, &ABlasterWeapon::OnPingTooHigh);
    }
}

// 装备副武器时的处理
void ABlasterWeapon::OnEquippedSecondary()
{
    ShowPickupWidget(false); // 隐藏拾取小部件
    AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 禁用区域碰撞

    // 禁用物理模拟（副武器固定在角色身上）
    WeaponMesh->SetSimulatePhysics(false);
    WeaponMesh->SetEnableGravity(false);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (WeaponMesh)
    {
        WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_TAN); // 设置副武器的深度值
        WeaponMesh->MarkRenderStateDirty();
    }

    EnableCustomDepth(true); // 启用自定义深度

    // 移除高延迟委托
    if (BlasterOwnerController && HasAuthority() && BlasterOwnerController->HighPingDelegate.IsBound())
    {
        BlasterOwnerController->HighPingDelegate.RemoveDynamic(this, &ABlasterWeapon::OnPingTooHigh);
    }
}

// 丢弃武器（客户端调用）
void ABlasterWeapon::Dropped()
{
    SetWeaponState(EWeaponState::EWS_Dropped);
    FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
    WeaponMesh->DetachFromComponent(DetachmentTransformRules); // 断开武器与角色的连接
    SetOwner(nullptr); // 清除拥有者
    BlasterOwnerCharacter = nullptr;
    BlasterOwnerController = nullptr;
}

// 获取带有散射效果的射击终点位置
FVector ABlasterWeapon::TraceEndWithScatter(const FVector& HitTarget) const
{
    const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
    if (MuzzleFlashSocket == nullptr) return FVector();

    const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
    const FVector TraceStart = SocketTransform.GetLocation();
    
    const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
    const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
    const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
    const FVector EndLoc = SphereCenter + RandVec;
    const FVector ToEndLoc = EndLoc - TraceStart;

    return FVector(TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size()); // 返回带散射效果的终点位置
}

// 显示或隐藏武器拾取小部件
void ABlasterWeapon::ShowPickupWidget(bool bShowWidget) const
{
    if (PickupWidget)
    {
        PickupWidget->SetVisibility(bShowWidget); // 设置可见性
    }
}

// 启用或禁用自定义深度（例如高亮显示）
void ABlasterWeapon::EnableCustomDepth(bool bEnable) const
{
    if (WeaponMesh)
    {
        WeaponMesh->SetRenderCustomDepth(bEnable); // 启用/禁用自定义深度
    }
}

// 设置武器状态
void ABlasterWeapon::SetWeaponState(EWeaponState State)
{
    WeaponState = State;
    OnWeaponStateSet(); // 调用状态设置函数
}

// 检查武器是否为空（弹药为0）
bool ABlasterWeapon::IsEmpty() const
{
    return Ammo <= 0;
}

// 检查武器是否已满（弹药达到弹匣容量）
bool ABlasterWeapon::IsFull() const
{
    return Ammo >= MagCapacity;
}