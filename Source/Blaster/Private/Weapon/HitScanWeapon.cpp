// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/HitScanWeapon.h"
#include "BlasterComponents/LagCompensationComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Sound/SoundCue.h"

// 即中武器（HitScan）的开火逻辑。HitTarget 是玩家准星瞄准的最终目标点
void AHitScanWeapon::Fire(const FVector& HitTarget)
{
    // 调用父类开火逻辑（处理通用的后坐力、子弹扣除、开火动画等）
    Super::Fire(HitTarget);

    // 获取当前持有这把武器的 Pawn（通常是玩家角色）
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn == nullptr) return; // 如果没有持有者，直接返回防崩溃
    
    // 获取开枪者的控制器（后续造成伤害时需要追溯伤害来源）
    AController* InstigatorController = OwnerPawn->GetController();

    // 尝试获取枪口火焰的骨骼插槽 ("MuzzleFlash")
    if (const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash"))
    {
       // 获取枪口插槽的世界坐标和旋转信息
       FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
       // 将枪口位置作为射线的起点
       FVector Start = SocketTransform.GetLocation();

       // 声明一个命中结果结构体，用于存储射线检测的返回信息
       FHitResult FireHit;
       // 调用下方自定义的射线检测函数，执行 HitScan 判定
       WeaponTraceHit(Start, HitTarget, FireHit);

       // 尝试将射线击中的 Actor 转换为游戏中的玩家角色 (ABlasterCharacter)
       ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
       // 如果确实打中了一个玩家，并且开枪者的控制器有效
       if (BlasterCharacter && InstigatorController)
       {
          // 决定当前端是否应该直接产生伤害（或发送伤害请求）：
          // 条件：不使用延迟补偿 (!bUseServerSideRewind) 或者 当前是本地玩家开枪
          const bool bCauseAuthorityDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
          
          // 【服务端逻辑】如果是服务器，并且满足上述条件，则直接扣除目标血量
          if (HasAuthority() && bCauseAuthorityDamage)
          {
             // 判断击中的骨骼名称是否为头部 ("head")，是则应用爆头伤害，否则应用普通伤害
             const float DamageToCause = FireHit.BoneName.ToString() == FString("head") ? HeadShotDamage : Damage;
             // 调用引擎原生的应用伤害接口
             UGameplayStatics::ApplyDamage(
                BlasterCharacter,     // 受击者
                DamageToCause,        // 伤害值
                InstigatorController, // 肇事者（开枪玩家）的控制器
                this,                 // 伤害源（这把武器）
                UDamageType::StaticClass()
             );
          }
          
          // 【客户端延迟补偿逻辑】如果是客户端开枪，且满足造成伤害的条件（通常意味着启用了延迟补偿且是本地玩家）
          if (!HasAuthority() && bCauseAuthorityDamage)
          {
             // 缓存本地玩家角色和控制器，避免重复Cast消耗性能
             BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
             BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
             
             // 如果获取成功，且拥有延迟补偿组件，且是本地控制的玩家
             if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensationComponent() && BlasterOwnerCharacter->IsLocallyControlled())
             {
                // 客户端调用延迟补偿组件的 RPC，向服务器发送带有时间戳的“命中请求”
                BlasterOwnerCharacter->GetLagCompensationComponent()->ServerScoreRequest(
                   BlasterCharacter, // 客户端判定打中了谁
                   Start,            // 开枪起点（枪口）
                   HitTarget,        // 射线终点
                   // 核心：计算开枪时的服务器时间 = 当前服务器时间 - 到服务器的单程网络延迟(Ping/2)
                   BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime
                );
             }
          }
       }
       
       // ==== 以下为视觉与听觉表现部分（双端都会执行，用于表现射击反馈） ====
       
       // 如果配置了击中粒子特效（例如血迹、火花），在命中点生成
       if (ImpactParticles)
       {
          UGameplayStatics::SpawnEmitterAtLocation(
             GetWorld(),
             ImpactParticles,
             FireHit.ImpactPoint,            // 命中点位置
             FireHit.ImpactNormal.Rotation() // 让特效顺着击中表面的法线方向喷射
          );
       }
       // 如果配置了击中音效，在命中点播放
       if (HitSound)
       {
          UGameplayStatics::PlaySoundAtLocation(
             this,
             HitSound,
             FireHit.ImpactPoint
          );
       }
       // 如果配置了枪口火焰特效，在枪口插槽处生成
       if (MuzzleFlash)
       {
          UGameplayStatics::SpawnEmitterAtLocation(
             GetWorld(),
             MuzzleFlash,
             SocketTransform // 包含枪口的位置和旋转
          );
       }
       // 如果配置了开火枪声，在武器当前位置播放
       if (FireSound)
       {
          UGameplayStatics::PlaySoundAtLocation(
             this,
             FireSound,
             GetActorLocation()
          );
       }
    }
}

// 执行实际物理射线检测的函数
void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit) const
{
   // 获取当前游戏世界上下文
   if (UWorld* World = GetWorld())
   {
      // 计算射线的终点：方向是 HitTarget 减去起点，乘以 1.25f 是为了把射线延长 25%。
      // 这是一个常见技巧：防止由于网络同步或浮点数精度误差，导致 HitTarget 刚好差一点点没够到目标表面。
      FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;
       
      // 发射一条单次碰撞的可见性通道射线（ECC_Visibility），遇到第一个阻挡物体就停下
      World->LineTraceSingleByChannel(
         OutHit,     // 结果存放的引用
         TraceStart, // 射线起点
         End,        // 射线终点
         ECollisionChannel::ECC_Visibility // 碰撞通道（打在可见物体上）
      );
       
      // 初始化光束特效的终点为射线的预期终点
      FVector BeamEnd = End;
      // 如果射线确实打到了东西（被阻挡）
      if (OutHit.bBlockingHit)
      {
         // 将光束的终点更新为实际击中的物理表面点
         BeamEnd = OutHit.ImpactPoint;
      }
      else
      {
         // 如果什么都没打中（向天空开枪），强制把 OutHit 的命中点设为射线终点
         // 这是为了防止后续逻辑（比如生成粒子）拿不到有效的 ImpactPoint 而报错或在原点生成
         OutHit.ImpactPoint = End;
      }

      // 调试用的代码被注释掉了：在击中点画一个橙色的球来方便开发者核对落点
      //DrawDebugSphere(GetWorld(), BeamEnd, 16.f, 12, FColor::Orange, true);

      // 绘制子弹的弹道光束特效（例如烟雾弹痕、激光束）
      if (BeamParticles)
      {
         // 在枪口（射线起点）生成光束粒子
         UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
            World,
            BeamParticles,
            TraceStart,
            FRotator::ZeroRotator,
            true // 自动销毁
         );
         // 如果生成成功
         if (Beam)
         {
            // 将光束材质/特效里的 "Target"（目标点参数）设置为之前计算出的击中点 (BeamEnd)
            // 这样光束就会在空间中被拉长，完美连接枪口和命中点
            Beam->SetVectorParameter(FName("Target"), BeamEnd);
         }
      }
   }
}