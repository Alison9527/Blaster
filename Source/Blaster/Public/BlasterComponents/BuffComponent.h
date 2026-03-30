// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UBuffComponent : public UActorComponent
{
    GENERATED_BODY()

public: 
    // 友元类：允许 ABlasterCharacter 直接访问本组件的 private 成员，方便逻辑高度耦合的类交互
    friend class ABlasterCharacter;
    
    UBuffComponent();
    
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- 公开的 Buff 接口 ---
    void Heal(float HealAmount, float HealingTime); // 触发治疗 Buff
    void ReplenishShield(float ReplenishAmount, float ReplenishTime); // 触发护盾回复 Buff
    void BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime); // 触发加速 Buff
    void SetInitialSpeeds(float BaseSpeed, float CrouchSpeed); // 记录角色初始速度（Buff 结束时还原）
    void SetInitialJumpVelocity(float JumpZVelocity); // 记录初始跳跃高度
    void BuffJump(float BuffJumpZVelocity, float BuffTime); // 触发跳跃增强 Buff

protected:
    virtual void BeginPlay() override;
    
    // 内部逻辑：处理治疗随时间递增的过程（在 Tick 中调用）
    void HealRampUp(float DeltaTime);
    // 内部逻辑：处理护盾随时间递增的过程（在 Tick 中调用）
    void ShieldRampUp(float DeltaTime);
    
private:
    // 加上 UPROPERTY() 以便垃圾回收，存储所属的角色引用
    UPROPERTY()
    class ABlasterCharacter* BlasterCharacter;
    
    /*
     * 治疗 Buff 相关变量
     */
    bool bHealing = false;       // 是否正在治疗中
    float HealingRate = 0;       // 治疗速率（每秒回多少血）
    float AmountToHeal = 0.f;    // 本次 Buff 剩余需要治疗的总量
    
    /*
     * 护盾 Buff 相关变量
     */
    bool bReplenishingShield = false;  // 是否正在回复护盾中
    float ShieldReplenishRate = 0.f;   // 护盾回复速率
    float ShieldReplenishAmount = 0.f; // 剩余需要回复的护盾总量
    
    /*
     * 速度 Buff 相关变量
     */
    FTimerHandle SpeedBuffTimer; // 定时器句柄，用于 Buff 到期后自动重置速度
    void ResetSpeeds();          // Buff 结束时恢复正常速度的回调函数
    float InitialBaseSpeed = 0.f;   // 存储原始行走速度
    float InitialCrouchSpeed = 0.f; // 存储原始蹲下速度
    
    // 网络多播函数（Reliable）：在服务器执行，会自动同步到所有客户端，用于表现角色加速
    UFUNCTION(NetMulticast, Reliable)
    void MulticastSpeedBuff(float BuffBaseSpeed, float BuffCrouchSpeed);
    
    /*
     * 跳跃 Buff 相关变量
     */
    FTimerHandle JumpBuffTimer;   // 跳跃 Buff 的定时器句柄
    void ResetJump();             // Buff 结束时恢复正常跳跃的回调函数
    float InitialJumpZVelocity = 0.f; // 存储原始跳跃高度
    
    // 网络多播函数：同步所有客户端上的角色跳跃高度增强
    UFUNCTION(NetMulticast, Reliable)
    void MulticastJumpBuff(float BuffJumpZVelocity);
};
