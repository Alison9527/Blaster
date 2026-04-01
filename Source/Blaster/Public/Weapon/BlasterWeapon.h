#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon/WeaponTypes.h"
#include "BlasterTypes/Team.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "BlasterWeapon.generated.h"

UENUM(BlueprintType)
enum class EWeaponState : uint8
{
    EWS_Initial UMETA(DisplayName = "Initial State"),
    EWS_Equipped UMETA(DisplayName = "Equipped"),
    EWS_EquippedSecondary UMETA(DisplayName = "Equipped Secondary"),
    EWS_Dropped UMETA(DisplayName = "Dropped"),
    EWS_MAX UMETA(DisplayName = "DefaultMAX")
};

UENUM(BlueprintType)
enum class EFireType : uint8
{
    EFT_HitScan UMETA(DisplayName = "HitScan Weapon"),
    EFT_Projectile UMETA(DisplayName = "Projectile Weapon"),
    EFT_Shotgun UMETA(DisplayName = "Shotgun Weapon"),
    EFT_MAX UMETA(DisplayName = "DefaultMAX")
};

UCLASS()
class BLASTER_API ABlasterWeapon : public AActor
{
    GENERATED_BODY()

public:
    ABlasterWeapon();
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnRep_Owner() override;
    void SetHUDAmmo();
    void ShowPickupWidget(bool bShowWidget) const;
    virtual void Fire(const FVector& HitTarget);
    virtual void Dropped();
    void AddAmmo(int32 AmmoToAdd);
    FVector TraceEndWithScatter(const FVector& HitTarget) const;

    UFUNCTION(NetMulticast, Reliable)
    void MulticastAmmo(int32 UpdateAmmo);

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

    UPROPERTY(EditAnywhere)
    float ZoomedFOV = 30.f;

    UPROPERTY(EditAnywhere)
    float ZoomInterpSpeed = 20.f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    float FireDelay = 0.15f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    bool bAutomatic = true;

    UPROPERTY(EditAnywhere, Category = "Combat")
    USoundCue* EquipSound;

    void EnableCustomDepth(bool bEnable) const;
    bool bDestroyWeapon = false;

    UPROPERTY(EditAnywhere)
    EFireType FireType;

    UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
    bool bUseScatter = false;

protected:
    virtual void BeginPlay() override;
    virtual void OnWeaponStateSet();
    virtual void OnEquipped();
    virtual void OnDropped();
    virtual void OnEquippedSecondary();

    UFUNCTION()
    virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
    float DistanceToSphere = 800.f;

    UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
    float SphereRadius = 75.f;

    UPROPERTY(EditAnywhere)
    float Damage = 20.f;

    UPROPERTY(EditAnywhere)
    float HeadShotDamage = 40.f;

    UPROPERTY(Replicated, EditAnywhere)
    bool bUseServerSideRewind = false;

    UPROPERTY()
    class ABlasterCharacter* BlasterOwnerCharacter;

    UPROPERTY()
    class ABlasterPlayerController* BlasterOwnerController;

    UFUNCTION()
    void OnPingTooHigh(bool bPingTooHigh);

    UFUNCTION(Client, Reliable)
    void ClientAddAmmo(int32 AmmoToAdd);
    // 声明为虚函数，允许子类（如霰弹枪）重写客户端加弹逻辑
    virtual void ClientAddAmmo_Implementation(int32 AmmoToAdd);

public:
    void SetWeaponState(EWeaponState State);
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
    FORCEINLINE void SetSequence() { Sequence = 0; }
    bool IsEmpty() const;
    bool IsFull() const;

private:
    UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
    USkeletalMeshComponent* WeaponMesh;

    UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
    USphereComponent* AreaSphere;

    UPROPERTY(ReplicatedUsing = OnRep_WeaponState, VisibleAnywhere, Category = "Weapon Properties")
    EWeaponState WeaponState;

    UFUNCTION()
    void OnRep_WeaponState();

    UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
    UWidgetComponent* PickupWidget;

    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    UAnimationAsset* FireAnimation;

    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    TSubclassOf<class ACasing> CasingClass;

    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    int32 Ammo = 30;

    UFUNCTION(Client, Reliable)
    void ClientUpdateAmmo(int32 ServerAmmo);

    void SpendRound();

    UPROPERTY(EditAnywhere, Category = "Weapon Properties")
    int32 MagCapacity = 30;

    int32 Sequence = 0;

    UPROPERTY(EditAnywhere)
    EWeaponType WeaponType;

    UPROPERTY(EditAnywhere)
    ETeam Team;
};