// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BlasterAnimInstance.h"
#include "Character/BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner()); 
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds); // 调用父类实现，确保基础更新逻辑执行

	if (BlasterCharacter == nullptr) // 如果还没有缓存角色指针
	{
		BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner()); // 尝试从拥有者中获取并转换为 AB­lasterCharacter
	}

	if (BlasterCharacter == nullptr) return; // 仍然为空则直接返回，避免后续空指针访问

	FVector Velocity = BlasterCharacter->GetVelocity(); // 获取角色当前速度向量
	Velocity.Z = 0.f; // 忽略垂直分量，只关心平面速度
	Speed = Velocity.Size(); // 根据平面速度计算移动速度大小，供动画使用

	bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling(); // 判断是否在空中（跳跃或掉落）
	bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f; // 判断是否正在加速（是否有输入导致加速）
	bWeaponEquipped = BlasterCharacter->IsWeaponEquipped(); // 判断是否装备武器
	bIsCrouched = BlasterCharacter->IsCrouched(); // 判断是否处于下蹲状态
	bAiming = BlasterCharacter->IsAiming(); // 判断是否处于瞄准状态

	// Offset Yaw for Strafing
	FRotator AimRotation = BlasterCharacter->GetBaseAimRotation(); // 获取角色当前的瞄准朝向（通常由控制器决定）
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity()); // 根据速度向量生成一个朝向（移动方向）
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation); // 计算移动方向与瞄准方向之间的标准化角差
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 6.f); // 平滑插值当前 DeltaRotation toward 计算得到的 DeltaRot，插值速度为 6
	YawOffset = DeltaRotation.Yaw;

	CharacterRotationLastFrame = CharacterRotation; // 保存上一帧的角色旋转，用于计算差值
	CharacterRotation = BlasterCharacter->GetActorRotation(); // 获取当前角色世界旋转
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame); // 计算本帧与上一帧旋转差
	const float Target = Delta.Yaw / DeltaSeconds; // 将旋转差转换为旋转速度（度/秒），作为目标倾斜值
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaSeconds, 6.f); // 平滑插值当前 Lean 值 toward 目标，插值速度为 6
	Lean = FMath::Clamp(Interp, -90.f, 90); // 限制 Lean 值范围，避免极端数值（用于角色左右倾斜动画）

	AO_Yaw = BlasterCharacter->GetAO_Yaw();
	AO_Pitch = BlasterCharacter->GetAO_Pitch(); // 获取角色的 AO_Yaw 和 AO_Pitch，用于动画蓝图中的上半身旋转调整
}
