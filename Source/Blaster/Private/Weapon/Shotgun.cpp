// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/Shotgun.h"
#include "BlasterComponents/LagCompensationComponent.h"
#include "BlasterComponents/CombatComponent.h" // 新增头文件以支持装弹动画调用
#include "Engine/SkeletalMeshSocket.h"
#include "Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Particles/ParticleSystemComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Sound/SoundCue.h"

void AShotgun::FireShotgun(const TArray<FVector_NetQuantize>& HitTargets)
{
    ABlasterWeapon::Fire(FVector());
    
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn == nullptr) return; 
    
    AController* InstigatorController = OwnerPawn->GetController();

    if (const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash"))
    {
       const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
       const FVector Start = SocketTransform.GetLocation();
       
       TMap<ABlasterCharacter*, uint32> HitMap;           
       TMap<ABlasterCharacter*, uint32> HeadShotHitMap;   
       
       for (FVector_NetQuantize HitTarget : HitTargets)
       {
          FHitResult FireHit; 
          WeaponTraceHit(Start, HitTarget, FireHit);

          // ✅ 重构点：将粒子和音效移出 Cast<ABlasterCharacter> 判断外
          // 这样即使霰弹枪打中的是墙壁，也会播放火花和音效
          if (FireHit.bBlockingHit)
          {
              if (ImpactParticles)
              {
                  UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, FireHit.ImpactPoint, FireHit.ImpactNormal.Rotation());
              }
              if (HitSound)
              {
                  UGameplayStatics::PlaySoundAtLocation(this, HitSound, FireHit.ImpactPoint, 0.5f, FMath::FRandRange(-0.5f, 0.5f));
              }
          }

          if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor()))
          {
             if (const bool bHeadShot = FireHit.BoneName.ToString() == FString("head"))
             {
                if (HeadShotHitMap.Contains(BlasterCharacter)) HeadShotHitMap[BlasterCharacter]++;
                else HeadShotHitMap.Emplace(BlasterCharacter, 1);
             }
             else
             {
                if (HitMap.Contains(BlasterCharacter)) HitMap[BlasterCharacter]++;
                else HitMap.Emplace(BlasterCharacter, 1);
             }
          }
       }
       
       TArray<ABlasterCharacter*> HitCharacters;  
       TMap<ABlasterCharacter*, float> DamageMap; 
       
       for (auto HitPair : HitMap)
       {
          if (HitPair.Key)
          {
             DamageMap.Emplace(HitPair.Key, HitPair.Value * Damage);
             HitCharacters.AddUnique(HitPair.Key);
          }
       }
       
       for (auto HeadShotHitPair : HeadShotHitMap)
       {
          if (HeadShotHitPair.Key)
          {
             if (DamageMap.Contains(HeadShotHitPair.Key)) DamageMap[HeadShotHitPair.Key] += HeadShotHitPair.Value * HeadShotDamage;
             else DamageMap.Emplace(HeadShotHitPair.Key, HeadShotHitPair.Value * HeadShotDamage);
             HitCharacters.AddUnique(HeadShotHitPair.Key);
          }
       }
       
       for (auto DamagePair : DamageMap)
       {
          if (DamagePair.Key && InstigatorController)
          {
             bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
             if (HasAuthority() && bCauseAuthDamage)
             {
                UGameplayStatics::ApplyDamage(
                   DamagePair.Key,         
                   DamagePair.Value,       
                   InstigatorController,   
                   this,                   
                   UDamageType::StaticClass() 
                );
             }
          }
       }

       if (!HasAuthority() && bUseServerSideRewind)
       {
          BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
          BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
          
          if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensationComponent() && BlasterOwnerCharacter->IsLocallyControlled())
          {
             BlasterOwnerCharacter->GetLagCompensationComponent()->ShotgunServerScoreRequest(
                HitCharacters, 
                Start,         
                HitTargets,    
                BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime 
             );
          }
       }
    }
}

void AShotgun::ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets) const
{
   const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
   if (MuzzleFlashSocket == nullptr) return;

   const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
   const FVector TraceStart = SocketTransform.GetLocation();
   const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
   const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
    
   for (uint32 i = 0; i < NumberOfPellets; i++)
   {
      const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
      const FVector EndLoc = SphereCenter + RandVec; 
      FVector ToEndLoc = EndLoc - TraceStart;
      ToEndLoc = TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size();
      HitTargets.Add(ToEndLoc);
   }
}

// ✅ 重构点：将原先在父类中的霰弹枪专属逻辑移到这里，实现真正的多态解耦
void AShotgun::ClientAddAmmo_Implementation(int32 AmmoToAdd)
{
    // 调用父类基础加弹药并刷新 HUD 的逻辑
    Super::ClientAddAmmo_Implementation(AmmoToAdd);

    BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
    
    // 如果弹匣已满，让 CombatComponent 跳转到结束装填动作
    if (BlasterOwnerCharacter && BlasterOwnerCharacter->GetCombatComponent() && IsFull())
    {
        BlasterOwnerCharacter->GetCombatComponent()->JumpToShotgunEnd();
    }
}