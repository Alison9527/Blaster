#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon/WeaponTypes.h"
#include "BlasterTypes/Team.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "BlasterWeapon.generated.h"

// Weapon State 枚举，用于表示武器的不同状态
UENUM(BlueprintType)
enum class EWeaponState : uint8
{
    EWS_Initial UMETA(DisplayName = "Initial State"),       // 武器的初始状态
    EWS_Equipped UMETA(DisplayName = "Equipped"),           // 武器被角色装备时的状态
    EWS_EquippedSecondary UMETA(DisplayName = "Equipped Secondary"), // 副武器装备状态
    EWS_Dropped UMETA(DisplayName = "Dropped"),             // 武器被丢弃的状态
    EWS_MAX UMETA(DisplayName = "DefaultMAX")               // 最大值占位符，用于限制枚举值
};

// Fire Type 枚举，用于表示不同武器的开火类型
UENUM(BlueprintType)
enum class EFireType : uint8
{
    EFT_HitScan UMETA(DisplayName = "HitScan Weapon"),      // 命中扫描武器类型（瞬时命中）
    EFT_Projectile UMETA(DisplayName = "Projectile Weapon"),// 投射物武器类型（有飞行时间的弹药）
    EFT_Shotgun UMETA(DisplayName = "Shotgun Weapon"),      // 霰弹枪类型武器
    EFT_MAX UMETA(DisplayName = "DefaultMAX")               // 最大值占位符
};

// ABlasterWeapon 类，表示游戏中的武器
UCLASS()
class BLASTER_API ABlasterWeapon : public AActor
{
    GENERATED_BODY()

public:
    // 构造函数
    ABlasterWeapon();

    // 每帧更新，处理武器状态
    virtual void Tick(float DeltaTime) override;

    // 网络同步函数，确保武器属性在网络中保持一致
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

    // 处理武器拥有者的同步
    virtual void OnRep_Owner() override;

    // 更新 HUD 上的弹药显示
    void SetHUDAmmo();

    // 显示/隐藏武器拾取小部件
    void ShowPickupWidget(bool bShowWidget) const;

    // 执行武器开火
    virtual void Fire(const FVector& HitTarget);

    // 处理武器丢弃事件
    virtual void Dropped();

    // 增加弹药
    void AddAmmo(int32 AmmoToAdd);

    // 计算带有散射效果的射击目标位置（适用于霰弹枪等）
    FVector TraceEndWithScatter(const FVector& HitTarget) const;

    // 网络多播函数，更新所有客户端的弹药数量
    UFUNCTION(NetMulticast, Reliable)
    void MulticastAmmo(int32 UpdateAmmo);

    // 准星纹理设置
    UPROPERTY(EditAnywhere, Category = "Crosshairs")
    UTexture2D* CrosshairsCenter;

    UPROPERTY(EditAnywhere, Category = "Crosshairs")
    UTexture2D* CrosshairsLeft;

    UPROPERTY(EditAnywhere, Category = "Crosshairs")
    UTexture2D* CrosshairsRight;

    UPROPERTY(EditAnywhere, Category = "Crosshairs")
    UTexture2D* CrosshairsTop;

    UPROPERTY(EditAnywhere, Category = "Crosshairs")
    UTexture2D* CrosshairsBottom;

    // 瞄准设置
    UPROPERTY(EditAnywhere)
    float ZoomedFOV = 30.f; // 瞄准时的视野范围

    UPROPERTY(EditAnywhere)
    float ZoomInterpSpeed = 20.f; // 瞄准时视野变化的插值速度

    // 战斗设置
    UPROPERTY(EditAnywhere, Category = "Combat")
    float FireDelay = 0.15f; // 开火延迟

    UPROPERTY(EditAnywhere, Category = "Combat")
    bool bAutomatic = true; // 是否为自动武器

    UPROPERTY(EditAnywhere, Category = "Combat")
    USoundCue* EquipSound; // 装备武器时播放的声音

    // 启用/禁用自定义深度渲染（高亮显示）
    void EnableCustomDepth(bool bEnable) const;

    // 销毁武器标志
    bool bDestroyWeapon = false;

    // 武器的开火类型（命中扫描、投射物、霰弹枪等）
    UPROPERTY(EditAnywhere)
    EFireType FireType;

    // 散射设置（适用于霰弹枪等）
    UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
    bool bUseScatter = false;

protected:
    // 游戏开始时初始化
    virtual void BeginPlay() override;

    // 设置武器状态（例如装备、丢弃等）
    virtual void OnWeaponStateSet();

    // 装备武器事件
    virtual void OnEquipped();

    // 丢弃武器事件
    virtual void OnDropped();

    // 装备副武器事件
    virtual void OnEquippedSecondary();

    // 碰撞重叠函数：检测武器与其他物体的重叠
    UFUNCTION()
    virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 碰撞结束重叠函数
    UFUNCTION()
    virtual void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // 散射设置（霰弹枪等）
    UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
    float DistanceToSphere = 800.f; // 最大散射距离

    UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
    float SphereRadius = 75.f; // 散射半径

    // 武器伤害设置
    UPROPERTY(EditAnywhere)
    float Damage = 20.f; // 基础伤害

    UPROPERTY(EditAnywhere)
    float HeadShotDamage = 40.f; // 爆头伤害

    // 是否使用服务器端回滚（用于高延迟补偿）
    UPROPERTY(Replicated, EditAnywhere)
    bool bUseServerSideRewind = false;

    // 武器拥有者（角色和控制器）引用
    UPROPERTY()
    class ABlasterCharacter* BlasterOwnerCharacter;

    UPROPERTY()
    class ABlasterPlayerController* BlasterOwnerController;

    // 处理高延迟时的 ping 事件
    UFUNCTION()
    void OnPingTooHigh(bool bPingTooHigh);

public:
    // 设置武器状态（例如装备、丢弃等）
    void SetWeaponState(EWeaponState State);

    // 获取武器的组件和属性
    FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
    FORCEINLINE USphereComponent* GetAreaSphere() const { return AreaSphere; }
    FORCEINLINE UWidgetComponent* GetPickupWidget() const { return PickupWidget; }
    FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; }
    FORCEINLINE int32 GetAmmo() const { return Ammo; }
    FORCEINLINE int32 GetMagCapacity() const { return MagCapacity; }
    FORCEINLINE float GetDamage() const { return Damage; }
    FORCEINLINE float GetHeadShotDamage() const { return HeadShotDamage; }
    FORCEINLINE ETeam GetTeam() const { return Team; }
    FORCEINLINE float GetZoomedFOV() const { return ZoomedFOV; }
    FORCEINLINE float GetZoomInterpSpeed() const { return ZoomInterpSpeed; }

    // 重置序列值为 0
    FORCEINLINE void SetSequence() { Sequence = 0; }

    // 检查武器是否为空（弹药耗尽）
    bool IsEmpty() const;

    // 检查武器是否已满（弹匣已满）
    bool IsFull() const;

private:
    // 武器网格组件（用于渲染武器模型）
    UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
    USkeletalMeshComponent* WeaponMesh;

    // 武器区域碰撞组件（用于碰撞检测和交互）
    UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
    USphereComponent* AreaSphere;

    // 武器状态（通过网络同步）
    UPROPERTY(ReplicatedUsing = OnRep_WeaponState, VisibleAnywhere, Category = "Weapon Properties")
    EWeaponState WeaponState;

    // 状态变化的回调函数
    UFUNCTION()
    void OnRep_WeaponState();

    // 武器拾取小部件（用于显示武器信息）
    UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
    UWidgetComponent* PickupWidget;

    // 武器开火动画
    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    UAnimationAsset* FireAnimation;

    // 弹壳（子弹壳）类
    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    TSubclassOf<class ACasing> CasingClass;

    // 弹药相关
    // 弹药数量（默认30发）
    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    int32 Ammo = 30;

    // 更新客户端的弹药数量（从服务器同步）
    UFUNCTION(Client, Reliable)
    void ClientUpdateAmmo(int32 ServerAmmo);

    // 客户端增加弹药数量（用于同步）
    UFUNCTION(Client, Reliable)
    void ClientAddAmmo(int32 AmmoToAdd);

    // 花费一发弹药（每次开火时调用）
    void SpendRound();

    // 弹匣容量（最大弹药数）
    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    int32 MagCapacity = 30;

    // 用于追踪武器的发射顺序（例如开火时的子弹编号）
    int32 Sequence = 0;

    // 武器类型（例如步枪、手枪等）
    UPROPERTY(EditAnywhere)
    EWeaponType WeaponType;

    // 武器所属阵营（例如红队或蓝队）
    UPROPERTY(EditAnywhere)
    ETeam Team;
};