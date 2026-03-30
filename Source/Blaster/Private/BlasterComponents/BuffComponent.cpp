// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterComponents/BuffComponent.h"
#include "Character/BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

// 构造函数：设置组件基础属性
UBuffComponent::UBuffComponent()
{
	// 开启 Tick，因为我们要处理平滑回血/回盾（RampUp）
	PrimaryComponentTick.bCanEverTick = true;
}

// 游戏开始
void UBuffComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 每帧执行
void UBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 每帧尝试进行生命值和护盾的平滑回复
	HealRampUp(DeltaTime);
	ShieldRampUp(DeltaTime);
}

// 启动治疗：计算每秒回血量并累加总量
void UBuffComponent::Heal(float HealAmount, float HealingTime)
{
	bHealing = true;
	HealingRate = HealAmount / HealingTime; // 计算每秒应该回复多少
	AmountToHeal += HealAmount;             // 叠加总治疗量（防止连续吃药水被覆盖）
}

// 治疗的每帧具体实现
void UBuffComponent::HealRampUp(float DeltaTime)
{
	// 安全检查：如果没在治疗、角色不存在或角色已阵亡，则跳过
	if (!bHealing || BlasterCharacter == nullptr || BlasterCharacter->IsEliminated()) return;

	// 计算本帧应回血量 = 速率 * 帧时间
	const float HealThisFrame = HealingRate * DeltaTime;
	// 设置角色新血量，并限制在 0 到最大血量之间
	BlasterCharacter->SetHealth(FMath::Clamp(BlasterCharacter->GetHealth() + HealThisFrame, 0.f, BlasterCharacter->GetMaxHealth()));
	// 更新 UI 上的血条
	BlasterCharacter->UpdateHUDHealth();
	// 从待回血池中减去本帧已回的部分
	AmountToHeal -= HealThisFrame;

	// 如果血回满了，或者池子空了，停止治疗
	if (AmountToHeal <= 0.f || BlasterCharacter->GetHealth() >= BlasterCharacter->GetMaxHealth())
	{
		bHealing = false;
		AmountToHeal = 0.f;
	}
}

// 启动护盾回复：逻辑与 Heal 几乎完全一致
void UBuffComponent::ReplenishShield(float ShieldAmount, float ReplenishTime)
{
	bReplenishingShield = true;
	ShieldReplenishRate = ShieldAmount / ReplenishTime;
	ShieldReplenishAmount += ShieldAmount;
}

// 护盾回复的每帧实现
void UBuffComponent::ShieldRampUp(float DeltaTime)
{
	if (!bReplenishingShield || BlasterCharacter == nullptr || BlasterCharacter->IsEliminated()) return;

	const float ReplenishThisFrame = ShieldReplenishRate * DeltaTime;
	BlasterCharacter->SetShield(FMath::Clamp(BlasterCharacter->GetShield() + ReplenishThisFrame, 0.f, BlasterCharacter->GetMaxShield()));
	BlasterCharacter->UpdateHUDShield();
	ShieldReplenishAmount -= ReplenishThisFrame;

	if (ShieldReplenishAmount <= 0.f || BlasterCharacter->GetShield() >= BlasterCharacter->GetMaxShield())
	{
		bReplenishingShield = false;
		ShieldReplenishAmount = 0.f;
	}
}

// 启动速度 Buff
void UBuffComponent::BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime)
{
	if (BlasterCharacter == nullptr) return;

	// 设置定时器，BuffTime 结束后自动调用 ResetSpeeds
	BlasterCharacter->GetWorldTimerManager().SetTimer(
	   SpeedBuffTimer,
	   this,
	   &UBuffComponent::ResetSpeeds,
	   BuffTime
	);

	// 修改本地移动组件数值
	if (BlasterCharacter->GetCharacterMovement())
	{
		BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = BuffBaseSpeed;
		BlasterCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched = BuffCrouchSpeed;
	}
	// 同步给其他客户端
	MulticastSpeedBuff(BuffBaseSpeed, BuffCrouchSpeed);
}

// 恢复初始速度
void UBuffComponent::ResetSpeeds()
{
	if (BlasterCharacter == nullptr || BlasterCharacter->GetCharacterMovement() == nullptr) return;

	// 还原回最初记录的速度
	BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = InitialBaseSpeed;
	BlasterCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched = InitialCrouchSpeed;
	// 再次多播同步还原
	MulticastSpeedBuff(InitialBaseSpeed, InitialCrouchSpeed);
}

// 多播实现：确保全网所有玩家看到的该角色速度都一致
void UBuffComponent::MulticastSpeedBuff_Implementation(float BaseSpeed, float CrouchSpeed)
{
	if (BlasterCharacter && BlasterCharacter->GetCharacterMovement())
	{
		BlasterCharacter->GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
		BlasterCharacter->GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	}
}

// 启动跳跃 Buff
void UBuffComponent::BuffJump(float BuffJumpVelocity, float BuffTime)
{
	if (BlasterCharacter == nullptr) return;

	// 定时器：时间到了调用 ResetJump
	BlasterCharacter->GetWorldTimerManager().SetTimer(
	   JumpBuffTimer,
	   this,
	   &UBuffComponent::ResetJump,
	   BuffTime
	);

	if (BlasterCharacter->GetCharacterMovement())
	{
		BlasterCharacter->GetCharacterMovement()->JumpZVelocity = BuffJumpVelocity;
	}
	MulticastJumpBuff(BuffJumpVelocity);
}

// 恢复初始跳跃力
void UBuffComponent::ResetJump()
{
	if (BlasterCharacter->GetCharacterMovement())
	{
		BlasterCharacter->GetCharacterMovement()->JumpZVelocity = InitialJumpZVelocity;
	}
	MulticastJumpBuff(InitialJumpZVelocity);
}

// 跳跃高度的多播同步
void UBuffComponent::MulticastJumpBuff_Implementation(float JumpVelocity)
{
	if (BlasterCharacter && BlasterCharacter->GetCharacterMovement())
	{
		BlasterCharacter->GetCharacterMovement()->JumpZVelocity = JumpVelocity;
	}
}

// 记录初始行走速度
void UBuffComponent::SetInitialSpeeds(float BaseSpeed, float CrouchSpeed)
{
	InitialBaseSpeed = BaseSpeed;
	InitialCrouchSpeed = CrouchSpeed;
}

// 记录初始跳跃力
void UBuffComponent::SetInitialJumpVelocity(float Velocity)
{
	InitialJumpZVelocity = Velocity;
}
