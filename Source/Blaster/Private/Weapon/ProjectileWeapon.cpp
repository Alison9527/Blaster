// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/ProjectileWeapon.h"
#include "Weapon/Projectile.h"
#include "Engine/SkeletalMeshSocket.h"

// 武器开火的具体实现，传入参数 HitTarget 为枪管瞄准的最终目标/落点
void AProjectileWeapon::Fire(const FVector& HitTarget)
{
    // 调用父类的开火逻辑（通常在这里处理通用的：播放开火动画、枪声、扣除弹药等）
    Super::Fire(HitTarget);
    
    // 获取当前武器的拥有者，并将其转换为 Pawn（通常是持有该武器的角色）
    APawn* InstigatorPawn = Cast<APawn>(GetOwner());
    // 获取武器骨骼网格体上名为 "MuzzleFlash"（枪口火焰）的插槽，用于确定子弹从哪里射出
    const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
    // 获取当前的游戏世界上下文
    UWorld* World = GetWorld();

    // 确保插槽存在且当前世界有效，防止引发空指针崩溃
    if (MuzzleFlashSocket && World)
    {
       // 获取枪口插槽在世界空间中的 Transform（位置、旋转、缩放）
       FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
       // 计算从枪口位置指向目标落点（HitTarget）的方向向量
       FVector ToTarget = HitTarget - SocketTransform.GetLocation();
       // 将方向向量转换为旋转体（Rotator），这是子弹生成时的初始朝向
       FRotator TargetRotation = ToTarget.Rotation();

       // 设置生成 Actor（子弹）时的参数
       FActorSpawnParameters SpawnParams;
       SpawnParams.Owner = GetOwner();          // 设置子弹的所有者为武器的所有者
       SpawnParams.Instigator = InstigatorPawn; // 设置子弹的肇事者/发起者为持枪的 Pawn

       // 声明一个空的子弹指针，准备接收接下来生成的子弹
       AProjectile* SpawnedProjectile = nullptr;

       // ========== 核心逻辑：是否启用服务器端倒带（延迟补偿）==========
       if (bUseServerSideRewind)
       {
          // 如果开枪的 Pawn 拥有网络权限（通常意味着这是服务器/主机端的 Pawn）
          if (InstigatorPawn->HasAuthority())
          {
             // 如果这个 Pawn 是本地控制的（即：这是服务器主机玩家自己开的枪）
             if (InstigatorPawn->IsLocallyControlled())
             {
                // 主机玩家自己开枪不需要延迟补偿，直接生成普通的子弹类 (ProjectileClass)
                SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
                SpawnedProjectile->bUseServerSideRewind = false; // 关闭倒带
                SpawnedProjectile->Damage = Damage;              // 赋予常规伤害
                SpawnedProjectile->HeadShotDamage = HeadShotDamage; // 赋予爆头伤害
             }
             else
             {
                // 主机拥有权限，但不是本地控制（即：这是服务器在处理客户端玩家开的枪）
                // 此时需要生成带有延迟补偿逻辑的子弹类 (ServerSideRewindProjectileClass)
                SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
                SpawnedProjectile->bUseServerSideRewind = true;  // 开启倒带以验证客户端的命中
             }
          }
          else
          {
             // 如果开枪的 Pawn 没有网络权限（即这是一个纯客户端控制的 Pawn）
             if (InstigatorPawn->IsLocallyControlled())
             {
                // 客户端本地玩家开枪：生成用于客户端本地预测的假子弹（或者是请求服务器做倒带的子弹）
                SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
                SpawnedProjectile->bUseServerSideRewind = true;
                SpawnedProjectile->TraceStart = SocketTransform.GetLocation(); // 记录射击起点用于射线检测
                SpawnedProjectile->InitialVelocity = SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed; // 设置初始速度
             }
             else
             {
                // 客户端机器上看到的其他玩家开枪（模拟代理）：只需生成做视觉表现的子弹即可
                SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
                SpawnedProjectile->bUseServerSideRewind = false; // 模拟代理不需要执行实际的倒带伤害判定
             }
          }
       }
       else
       {
          // ========== 未启用服务器端倒带（传统的权威服务器射击逻辑）==========
          if (InstigatorPawn->HasAuthority())
          {
             // 只有服务器有权生成带有伤害的真实子弹
             SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
             SpawnedProjectile->bUseServerSideRewind = false;
             SpawnedProjectile->Damage = Damage;
             SpawnedProjectile->HeadShotDamage = HeadShotDamage;
          }
       }
    }
}