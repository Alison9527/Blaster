// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/Shotgun.h"
#include "BlasterComponents/LagCompensationComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Particles/ParticleSystemComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Sound/SoundCue.h"

// 霰弹枪开火函数。传入参数 HitTargets 是一个经过网络量化压缩的向量数组，包含了所有霰弹弹丸的落点
void AShotgun::FireShotgun(const TArray<FVector_NetQuantize>& HitTargets)
{
    // 调用父类的开火逻辑（传入一个空向量，因为父类可能只负责播放开火动画、扣除弹药等基础表现）
    ABlasterWeapon::Fire(FVector());
    
    // 获取这把武器的拥有者（通常是拿着这把枪的玩家角色 Pawn）
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn == nullptr) return; // 安全检查：如果没有拥有者则直接返回
    
    // 获取拥有者的控制器，这在后续申请伤害时必须用到（用于判定击杀者是谁）
    AController* InstigatorController = OwnerPawn->GetController();

    // 尝试获取枪械骨骼网格体上的枪口插槽 ("MuzzleFlash")
    if (const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash"))
    {
       // 获取枪口插槽在世界空间中的 Transform（位置、旋转、缩放）
       const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
       // 将枪口位置作为子弹射线的起点
       const FVector Start = SocketTransform.GetLocation();
       
       // 【性能优化核心】使用哈希表（TMap）来记录每个角色被击中了多少发弹丸，避免重复调用 ApplyDamage
       TMap<ABlasterCharacter*, uint32> HitMap;           // 记录击中身体的次数：<被击中角色, 命中次数>
       TMap<ABlasterCharacter*, uint32> HeadShotHitMap;   // 记录爆头的次数：<被击中角色, 爆头次数>
       
       // 遍历传入的所有弹丸落点（霰弹枪一枪可能打出十几发弹丸）
       for (FVector_NetQuantize HitTarget : HitTargets)
       {
          FHitResult FireHit; // 用于存储射线检测的结果
          // 从枪口到当前遍历的落点发射一条射线，检测是否打中物体
          WeaponTraceHit(Start, HitTarget, FireHit);

          // 尝试将射线打中的目标转换为玩家角色（ABlasterCharacter）
          if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor()))
          {
             // 检查打中的物理资产骨骼名称是否是头部 ("head")
             // （注：原作者这里用 const float 接布尔值略显奇怪，但C++中 true 会隐式转换为 1.0f）
             if (const float bHeadShot = FireHit.BoneName.ToString() == FString("head"))
             {
                // 如果是爆头，且 Map 中已有该角色，命中次数+1；否则添加到 Map 中并设为 1
                if (HeadShotHitMap.Contains(BlasterCharacter)) HeadShotHitMap[BlasterCharacter]++;
                else HeadShotHitMap.Emplace(BlasterCharacter, 1);
             }
             else
             {
                // 如果打中的是身体其他部位，操作同上，存入普通 HitMap
                if (HitMap.Contains(BlasterCharacter)) HitMap[BlasterCharacter]++;
                else HitMap.Emplace(BlasterCharacter, 1);
             }
             
             // 如果配置了击中粒子特效，在当前弹丸的击中点生成特效
             if (ImpactParticles)
             {
                UGameplayStatics::SpawnEmitterAtLocation(
                   GetWorld(),
                   ImpactParticles,
                   FireHit.ImpactPoint,
                   FireHit.ImpactNormal.Rotation() // 粒子朝向击中表面的法线方向
                );
             }
             // 如果配置了击中音效，在击中点播放声音
             if (HitSound)
             {
                UGameplayStatics::PlaySoundAtLocation(
                   this,
                   HitSound,
                   FireHit.ImpactPoint,
                   0.5f, // 霰弹枪弹丸多，音量减半防止爆音
                   FMath::FRandRange(-0.5f, 0.5f) // 随机化音高，避免多个相同音效同时播放产生刺耳的相位问题
                );
             }
          }
       }
       
       // 准备汇总伤害数据
       TArray<ABlasterCharacter*> HitCharacters;  // 记录所有被打中的角色列表
       TMap<ABlasterCharacter*, float> DamageMap; // 记录每个角色最终应受到的总伤害
       
       // 遍历普通命中的 Map，计算打身体的总伤害
       for (auto HitPair : HitMap)
       {
          if (HitPair.Key)
          {
             // 该角色的身体总伤害 = 身体命中次数 * 单发基础伤害
             DamageMap.Emplace(HitPair.Key, HitPair.Value * Damage);
             // 将该角色加入受击者列表（AddUnique 防止重复添加）
             HitCharacters.AddUnique(HitPair.Key);
          }
       }
       
       // 遍历爆头命中的 Map，计算并叠加爆头伤害
       for (auto HeadShotHitPair : HeadShotHitMap)
       {
          if (HeadShotHitPair.Key)
          {
             // 如果该角色已经被打中过身体，则在原有总伤害上累加：爆头次数 * 单发爆头伤害
             if (DamageMap.Contains(HeadShotHitPair.Key)) DamageMap[HeadShotHitPair.Key] += HeadShotHitPair.Value * HeadShotDamage;
             // 如果该角色只被打中了头，直接存入：爆头次数 * 单发爆头伤害
             else DamageMap.Emplace(HeadShotHitPair.Key, HeadShotHitPair.Value * HeadShotDamage);

             // 同样加入受击者列表
             HitCharacters.AddUnique(HeadShotHitPair.Key);
          }
       }
       
       // 正式应用（结算）总伤害
       for (auto DamagePair : DamageMap)
       {
          if (DamagePair.Key && InstigatorController)
          {
             // 决定是否由当前端直接造成权威伤害：
             // 条件：不使用延迟补偿机制，或者是本地玩家（比如服务端的主机玩家自己开的枪）
             bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
             
             // 只有服务端 (HasAuthority) 且满足上述条件时，才真正扣减目标血量
             if (HasAuthority() && bCauseAuthDamage)
             {
                UGameplayStatics::ApplyDamage(
                   DamagePair.Key,         // 受击角色
                   DamagePair.Value,       // 计算好的合并总伤害
                   InstigatorController,   // 伤害来源的控制器
                   this,                   // 造成伤害的武器 (AShotgun)
                   UDamageType::StaticClass() // 伤害类型
                );
             }
          }
       }

       // 【延迟补偿核心】如果是客户端开枪，且开启了服务器端倒带验证（Server-Side Rewind）
       if (!HasAuthority() && bUseServerSideRewind)
       {
          // 缓存角色和控制器指针，避免重复 Cast 带来性能开销
          BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
          BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
          
          // 如果当前是本地控制的客户端
          if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensationComponent() && BlasterOwnerCharacter->IsLocallyControlled())
          {
             // 向服务器发送 RPC 请求，附带当前的霰弹枪射击数据让服务器进行倒带验证
             BlasterOwnerCharacter->GetLagCompensationComponent()->ShotgunServerScoreRequest(
                HitCharacters, // 客户端判定打中了哪些人
                Start,         // 开火起点（枪口）
                HitTargets,    // 所有弹丸的落点
                // 开火时的客户端对应的服务器时间（当前服务器时间 - 到服务器的单程网络延迟）
                BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime 
             );
          }
       }
    }
}

// 根据准星瞄准的中心点，计算出带有随机散射偏移的所有弹丸落点
void AShotgun::ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets) const
{
   // 获取枪口插槽
   const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
   if (MuzzleFlashSocket == nullptr) return;

   // 获取枪口在世界空间中的位置，作为所有弹丸射线的起点
   const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
   const FVector TraceStart = SocketTransform.GetLocation();

   // 计算从枪口指向玩家准星中心目标点 (HitTarget) 的标准单位方向向量
   const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
    
   // 【算法核心：虚拟球体】在沿着瞄准方向的一定距离（DistanceToSphere）处，假想一个虚拟球体的中心点
   const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
    
   // 循环生成设定数量（NumberOfPellets，例如 12 发）的弹丸
   for (uint32 i = 0; i < NumberOfPellets; i++)
   {
      // UKismetMathLibrary::RandomUnitVector() 生成一个随机的三维单位向量
      // 乘以一个 0 到球体半径 (SphereRadius) 之间的随机数
      // 这样就在虚拟球体的内部随机生成了一个偏移向量 (RandVec)
      const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
       
      // 将偏移向量加到球体中心点上，得到当前这发弹丸在虚拟球内的目标穿透点
      const FVector EndLoc = SphereCenter + RandVec; 
       
      // 计算从枪口指向这个随机目标点的方向向量
      FVector ToEndLoc = EndLoc - TraceStart;
       
      // 将这个方向向量拉长到武器的最大射程限制 (TRACE_LENGTH)
      // 公式：起点 + (方向向量 * (最大射程 / 当前向量长度))
      ToEndLoc = TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size();

      // 将计算好的这发弹丸的最终终点添加到数组中
      HitTargets.Add(ToEndLoc);
   }
}