// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/HitScanWeapon.h"
#include "BlasterComponents/LagCompensationComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Sound/SoundCue.h"

void AHitScanWeapon::Fire(const FVector& HitTarget)
{
    Super::Fire(HitTarget);

    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn == nullptr) return; 
    
    AController* InstigatorController = OwnerPawn->GetController();

    if (const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash"))
    {
       FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
       FVector Start = SocketTransform.GetLocation();

       FHitResult FireHit;
       WeaponTraceHit(Start, HitTarget, FireHit);

       ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
       if (BlasterCharacter && InstigatorController)
       {
          // ✅ 重构点：严格区分服务端施加伤害的条件和客户端发起倒带请求的条件
          // 服务端逻辑：如果不使用回滚，或者是服务器主机（LocallyControlled），则直接造成伤害
          if (HasAuthority() && (!bUseServerSideRewind || OwnerPawn->IsLocallyControlled()))
          {
             const float DamageToCause = FireHit.BoneName.ToString() == FString("head") ? HeadShotDamage : Damage;
             UGameplayStatics::ApplyDamage(
                BlasterCharacter,
                DamageToCause,
                InstigatorController,
                this,
                UDamageType::StaticClass()
             );
          }
          
          // 客户端逻辑：如果启用了回滚，并且是本地玩家，向服务器发 RPC 请求
          if (!HasAuthority() && bUseServerSideRewind && OwnerPawn->IsLocallyControlled())
          {
             BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
             BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
             
             if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensationComponent())
             {
                BlasterOwnerCharacter->GetLagCompensationComponent()->ServerScoreRequest(
                   BlasterCharacter,
                   Start,
                   HitTarget,
                   BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime
                );
             }
          }
       }
       
       if (ImpactParticles)
       {
          UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, FireHit.ImpactPoint, FireHit.ImpactNormal.Rotation());
       }
       if (HitSound)
       {
          UGameplayStatics::PlaySoundAtLocation(this, HitSound, FireHit.ImpactPoint);
       }
       if (MuzzleFlash)
       {
          UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
       }
       if (FireSound)
       {
          UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
       }
    }
}

void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit) const
{
   if (UWorld* World = GetWorld())
   {
      FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;
       
      World->LineTraceSingleByChannel(
         OutHit,
         TraceStart,
         End,
         ECollisionChannel::ECC_Visibility
      );
       
      FVector BeamEnd = End;
      if (OutHit.bBlockingHit)
      {
         BeamEnd = OutHit.ImpactPoint;
      }
      else
      {
         OutHit.ImpactPoint = End;
      }

      if (BeamParticles)
      {
         UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(World, BeamParticles, TraceStart, FRotator::ZeroRotator, true);
         if (Beam)
         {
            Beam->SetVectorParameter(FName("Target"), BeamEnd);
         }
      }
   }
}