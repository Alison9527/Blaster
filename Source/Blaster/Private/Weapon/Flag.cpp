#include "Weapon/Flag.h"
#include "Character/BlasterCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"

// AFlag 构造函数
AFlag::AFlag()
{
    // 创建旗帜的静态网格组件，作为旗帜的表现
    FlagMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlagMesh"));
    SetRootComponent(FlagMesh); // 将 FlagMesh 设置为根组件

    // 设置 AreaSphere 和 PickupWidget 附加到 FlagMesh 上
    GetAreaSphere()->SetupAttachment(FlagMesh);
    GetPickupWidget()->SetupAttachment(FlagMesh);

    // 设置旗帜网格的碰撞响应为忽略所有碰撞
    FlagMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    // 设置旗帜网格的碰撞为无碰撞
    FlagMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// 游戏开始时初始化
void AFlag::BeginPlay()
{
    Super::BeginPlay();

    // 记录旗帜的初始位置
    InitialTransform = GetActorTransform();
    
    // 启用网络同步
    bReplicates = true;
    SetReplicateMovement(true);
}

// 丢弃旗帜时的逻辑
void AFlag::Dropped()
{
    // 设置武器状态为“已丢弃”
    SetWeaponState(EWeaponState::EWS_Dropped);

    // 设置分离规则，并从当前组件中分离旗帜
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    FlagMesh->DetachFromComponent(DetachRules);

    // 清除拥有者信息
    SetOwner(nullptr);
    BlasterOwnerCharacter = nullptr;
    BlasterOwnerController = nullptr;
}

// 重置旗帜到初始位置
void AFlag::ResetFlag()
{
    // 如果当前旗帜持有者存在，则更新状态
    if (ABlasterCharacter* FlagBearer = Cast<ABlasterCharacter>(GetOwner()))
    {
        FlagBearer->SetHoldingTheFlag(false); // 设置玩家不再持有旗帜
        FlagBearer->SetOverlappingWeapon(nullptr); // 清除重叠的武器
        FlagBearer->UnCrouch(); // 取消玩家的蹲伏状态
    }

    // 仅在服务器端执行此操作
    if (!HasAuthority()) return;

    // 将旗帜重置到初始位置
    SetActorTransform(InitialTransform);

    // 设置旗帜分离规则并从当前组件中分离旗帜
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    FlagMesh->DetachFromComponent(DetachRules);

    // 设置武器状态为“初始”
    SetWeaponState(EWeaponState::EWS_Initial);

    // 启用区域碰撞检测
    GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetAreaSphere()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

    // 清除旗帜的拥有者信息
    SetOwner(nullptr);
    BlasterOwnerCharacter = nullptr;
    BlasterOwnerController = nullptr;
}

// 装备旗帜时的逻辑
void AFlag::OnEquipped()
{
    // 隐藏拾取小部件
    ShowPickupWidget(false);

    // 禁用区域碰撞
    GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 禁用旗帜物理模拟和重力
    FlagMesh->SetSimulatePhysics(false);
    FlagMesh->SetEnableGravity(false);
    
    // 设置旗帜的碰撞为“仅查询”
    FlagMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // 设置旗帜与动态物体的碰撞响应
    FlagMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);

    // 禁用自定义深度
    EnableCustomDepth(false);
}

// 丢弃旗帜时的逻辑
void AFlag::OnDropped()
{
    // 仅在服务器端执行此操作
    if (HasAuthority())
    {
        GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 启用区域碰撞查询
    }

    // 启用旗帜的物理模拟和重力
    FlagMesh->SetSimulatePhysics(true);
    FlagMesh->SetEnableGravity(true);
    
    // 设置旗帜的碰撞为“查询与物理”
    FlagMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    // 设置旗帜与所有通道的碰撞响应为阻挡
    FlagMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    // 设置旗帜与玩家碰撞时的响应为忽略
    FlagMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
    // 设置旗帜与相机的碰撞响应为忽略
    FlagMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

    // 设置自定义深度的标识（例如显示为蓝色）
    FlagMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
    FlagMesh->MarkRenderStateDirty(); // 更新渲染状态
    EnableCustomDepth(true); // 启用自定义深度
}