// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterComponents/LagCompensationComponent.h"
#include "Blaster/Blaster.h"
#include "Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon/BlasterWeapon.h"

ULagCompensationComponent::ULagCompensationComponent()
{
	// 允许组件每帧调用 TickComponent，因为录制历史数据必须每帧进行
	PrimaryComponentTick.bCanEverTick = true;
}

void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 在服务器端，每一帧都会记录当前玩家的命中盒位置存入链表
	SaveFramePackageServer();
}

void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color) const
{
	for (auto& BoxInfo : Package.HitBoxInfo)
	{
		DrawDebugBox(GetWorld(), BoxInfo.Value.Location, BoxInfo.Value.BoxExtent, FQuat(BoxInfo.Value.Rotation), Color, false, 4.f);
	}
}

// ==========================================
// 核心：寻找历史帧与插值算法
// ==========================================

FFramePackage ULagCompensationComponent::GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime)
{
	// 容错检查：确保目标角色及其录像链表有效
	bool bReturn = HitCharacter == nullptr || HitCharacter->GetLagCompensationComponent() == nullptr || HitCharacter->GetLagCompensationComponent()->FrameHistory.GetHead() == nullptr ||HitCharacter->GetLagCompensationComponent()->FrameHistory.GetTail() == nullptr;
	if (bReturn) return FFramePackage();
	
	FFramePackage FrameToCheck;
	bool bShouldInterpolate = true; 
	
	// 获取录像链表，Head 是最新记录，Tail 是最旧记录
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensationComponent()->FrameHistory;
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;
	
	// 极端情况1：开枪时间比我们保存的最旧记录还要老（延迟过高），无法补偿
	if (OldestHistoryTime > HitTime) return FFramePackage(); 
	
	// 极端情况2：开枪时间刚好等于最旧记录
	if (OldestHistoryTime == HitTime)
	{
		FrameToCheck = FrameHistory.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	
	// 极端情况3：开枪时间比最新的记录还要新（时钟误差），直接取最新帧
	if (NewestHistoryTime <= HitTime)
	{
		FrameToCheck = FrameHistory.GetHead()->GetValue(); 
		bShouldInterpolate = false;
	}
	
	// 定义游标从最新的记录开始，往过去的时间倒退寻找
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = Younger;
	
	while (Older->GetValue().Time > HitTime) 
	{
		if (Older->GetNextNode() == nullptr) break;
		Older = Older->GetNextNode(); // 游标继续向过去移动
		if (Older->GetValue().Time > HitTime)
		{
			Younger = Older; // 如果 Older 还没跨过 HitTime，Younger 就跟上
		}
	}
	
	// 运气极好：某一帧记录的时间刚好等于开枪时间
	if (Older->GetValue().Time == HitTime)
	{
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	
	// 常规情况：开枪时间处于 Older 和 Younger 两帧之间，执行平滑插值计算出那一瞬间的确切位置
	if (bShouldInterpolate)
	{
		FrameToCheck = InterpolateBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}
	
	FrameToCheck.BlasterCharacter = HitCharacter;
	return FrameToCheck; 
}

FFramePackage ULagCompensationComponent::InterpolateBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime)
{
	// 计算两帧之间的时间差距离
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	// 计算开枪时间在两帧之间占据的百分比（0.0 ~ 1.0）
	const float InterpolationFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);
	
	FFramePackage InterpolatedFrame;
	InterpolatedFrame.Time = HitTime;
	
	// 遍历身上每一个部位（头、手、脚等）
	for (auto& YoungerHitBox : YoungerFrame.HitBoxInfo)
	{
		const FName& HitBoxName = YoungerHitBox.Key;
		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[HitBoxName];
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[HitBoxName];
		
		FBoxInformation InterpBoxInfo;
		// 位置：线性插值 (VInterpTo)
		InterpBoxInfo.Location = FMath::VInterpTo(OlderBox.Location, YoungerBox.Location, InterpolationFraction, InterpolationFraction);
		// 旋转：球面线性插值 (RInterpTo)
		InterpBoxInfo.Rotation = FMath::RInterpTo(OlderBox.Rotation, YoungerBox.Rotation, InterpolationFraction, InterpolationFraction);
		// 尺寸保持不变
		InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;
		
		InterpolatedFrame.HitBoxInfo.Add(HitBoxName, InterpBoxInfo);
	}
	return InterpolatedFrame;
}

// ==========================================
// 命中盒(HitBox) 物理状态管理 (“时间旅行”控制器)
// ==========================================

void ULagCompensationComponent::CacheBoxPositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage)
{
	if (HitCharacter == nullptr) return;
	// 将目前角色身上的真实命中盒坐标保存下来
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			FBoxInformation BoxInfo;
			BoxInfo.Location = HitBoxPair.Value->GetComponentLocation();
			BoxInfo.Rotation = HitBoxPair.Value->GetComponentRotation();
			BoxInfo.BoxExtent = HitBoxPair.Value->GetScaledBoxExtent();
			OutFramePackage.HitBoxInfo.Add(HitBoxPair.Key, BoxInfo);
		}
	}
}

void ULagCompensationComponent::MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& FramePackage)
{
	if (HitCharacter == nullptr) return;
	// 强行把角色当前的碰撞盒瞬移到历史包 (FramePackage) 记录的位置
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			if (const FBoxInformation* BoxValue = FramePackage.HitBoxInfo.Find(HitBoxPair.Key))
			{
				HitBoxPair.Value->SetWorldLocation(BoxValue->Location);
				HitBoxPair.Value->SetWorldRotation(BoxValue->Rotation);
				HitBoxPair.Value->SetBoxExtent(BoxValue->BoxExtent);
			}
		}
	}
}

void ULagCompensationComponent::ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& FramePackage)
{
	if (HitCharacter == nullptr) return;
	// 验证完毕，把盒子送回当前真实的物理位置，并关闭碰撞
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			if (const FBoxInformation* BoxValue = FramePackage.HitBoxInfo.Find(HitBoxPair.Key))
			{
				HitBoxPair.Value->SetWorldLocation(BoxValue->Location);
				HitBoxPair.Value->SetWorldRotation(BoxValue->Rotation);
				HitBoxPair.Value->SetBoxExtent(BoxValue->BoxExtent);
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 恢复关闭状态
			}
		}
	}
}

void ULagCompensationComponent::EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled)
{
	// 开启或关闭角色的外观模型碰撞
	if (HitCharacter && HitCharacter->GetMesh())
	{
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}

// ==========================================
// 录制系统的底层实现
// ==========================================

void ULagCompensationComponent::SaveFramePackageServer()
{
	// 只有服务器有权限和必要录制
	if (BlasterCharacter == nullptr || !BlasterCharacter->HasAuthority()) return;
	
	if (FrameHistory.Num() <= 1)
	{
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame); // 加到链表头部
	}
	else
	{
		// 计算记录总跨度时长 (最新时间 - 最老时间)
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		
		// 如果超出了最大限制，循环剔除最旧的尾部数据
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			if (FrameHistory.GetTail()) 
			{
				HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
			}
			else break;
		}
		
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);
	}
}

// 定义在 ULagCompensationComponent 类中的函数，用于保存一帧的数据包
// 传入的参数 FramePackage 是一个引用，函数内部会直接修改并填充这个包的数据
void ULagCompensationComponent::SaveFramePackage(FFramePackage& FramePackage)
{
	// 检查缓存的 BlasterCharacter 成员变量是否为空。
	// 如果为空，则获取当前组件的拥有者 (GetOwner)，并将其强制转换 (Cast) 为 ABlasterCharacter 类型进行赋值；
	// 如果不为空，则保持原值。这是一种懒加载/安全检查的写法，避免每次都调用 Cast 消耗性能。
	BlasterCharacter = BlasterCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterCharacter;

	// 确保我们成功获取到了有效的 BlasterCharacter 对象
	if (BlasterCharacter)
	{
		// 获取当前游戏世界的时间（通常是服务器的当前时间），并将其作为时间戳保存到这一帧的数据包中
		FramePackage.Time = GetWorld()->GetTimeSeconds(); // 盖上当前服务器时间戳
       
		// 将这一帧所对应的角色指针也保存到数据包中，方便后续调用时知道是哪个角色的数据
		FramePackage.BlasterCharacter = BlasterCharacter;

		// 遍历该角色身上所有的命中碰撞盒 (HitCollisionBoxes)。
		// 假设这是一个 TMap 或者 TArray 包含着类似 <FName, UBoxComponent*> 键值对的数据结构
		for (const auto& HitBox : BlasterCharacter->HitCollisionBoxes)
		{
			// 声明一个局部的结构体变量 BoxInformation，用于存储当前这个碰撞盒的变换信息
			FBoxInformation BoxInformation;

			// 获取当前碰撞盒组件在世界空间中的绝对位置，并存入结构体
			BoxInformation.Location = HitBox.Value->GetComponentLocation();

			// 获取当前碰撞盒组件在世界空间中的绝对旋转角度，并存入结构体
			BoxInformation.Rotation = HitBox.Value->GetComponentRotation();

			// 获取当前碰撞盒组件经过缩放后的包围盒范围（即长宽高的一半），并存入结构体
			BoxInformation.BoxExtent = HitBox.Value->GetScaledBoxExtent();

			// 将收集好信息的 BoxInformation 添加到 FramePackage 的 HitBoxInfo 集合（TMap）中。
			// HitBox.Key 通常是该碰撞盒对应的骨骼名称或标识符（比如 "head", "pelvis" 等）
			FramePackage.HitBoxInfo.Add(HitBox.Key, BoxInformation);
		} 
	}
}

// ==========================================
// 核心验证逻辑 (HitScan 步枪/手枪)
// ==========================================

FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime)
{
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

void ULagCompensationComponent::ServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime)
{
	FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);
	
	if (BlasterCharacter && HitCharacter && BlasterCharacter->GetEquippedWeapon() && Confirm.bHitConfirmed)
	{
		// 如果打中了，根据是否爆头提取相应的伤害数值
		const float Damage = Confirm.bHeadShot ? BlasterCharacter->GetEquippedWeapon()->GetHeadShotDamage() : BlasterCharacter->GetEquippedWeapon()->GetDamage();

		// 应用扣血
		UGameplayStatics::ApplyDamage(HitCharacter, Damage, BlasterCharacter->Controller, BlasterCharacter->GetEquippedWeapon(), UDamageType::StaticClass());
	}
}

FServerSideRewindResult ULagCompensationComponent::ConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	// 1. 记录现在真实位置
	CacheBoxPositions(HitCharacter, CurrentFrame);
	// 2. 强行把目标退回过去
	MoveBoxes(HitCharacter, FramePackage);
	// 3. 扒掉外皮碰撞，防止阻挡射线
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);
	
	// --- 第一阶段：优先验证爆头 ---
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block); // 设置射线响应

	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f; // 射线拉长1.25倍防浮点误差
	if (UWorld* World = GetWorld())
	{
		FHitResult ConfirmHitResult;
		// 沿着开火轨迹发射射线，只探测我们设置好的 ECC_HitBox 通道
		World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
		
		if (ConfirmHitResult.bBlockingHit) // 挡住了！说明爆头成功！
		{
			ResetHitBoxes(HitCharacter, CurrentFrame); // 擦屁股还原
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, true }; // 返回：命中=真，爆头=真
		}
		// --- 第二阶段：如果没爆头，开启全身体碰撞盒验证是否打中身体 ---
		for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
		{
			if (HitBoxPair.Value != nullptr)
			{
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			}
		}
		// 用大号的全身靶子再射一次
		World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
		if (ConfirmHitResult.bBlockingHit) 
		{
			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, false }; // 返回：命中=真，爆头=假
		}
	}
	
	// 射线没扫到任何盒子，判定为空枪
	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

// ==========================================
// 投射物验证逻辑 (火箭筒/榴弹)
// ==========================================

FServerSideRewindResult ULagCompensationComponent::ProjectileServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ProjectileConfirmHit(FrameToCheck, HitCharacter, TraceStart, InitialVelocity, HitTime);
}

void ULagCompensationComponent::ProjectileServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	FServerSideRewindResult Confirm = ProjectileServerSideRewind(HitCharacter, TraceStart, InitialVelocity, HitTime);
	if (BlasterCharacter && HitCharacter && Confirm.bHitConfirmed && BlasterCharacter->GetEquippedWeapon())
	{
		const float Damage = Confirm.bHeadShot ? BlasterCharacter->GetEquippedWeapon()->GetHeadShotDamage() : BlasterCharacter->GetEquippedWeapon()->GetDamage();
		UGameplayStatics::ApplyDamage(HitCharacter, Damage, BlasterCharacter->Controller, BlasterCharacter->GetEquippedWeapon(), UDamageType::StaticClass());
	}
}

FServerSideRewindResult ULagCompensationComponent::ProjectileConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, FramePackage);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);
	
	// 开启头部
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
	
	// 设置引擎内置抛物线弹道预测参数
	FPredictProjectilePathParams PathParams;
	PathParams.bTraceWithCollision = true;
	PathParams.MaxSimTime = MaxRecordTime;
	PathParams.LaunchVelocity = InitialVelocity;
	PathParams.StartLocation = TraceStart;
	PathParams.SimFrequency = 15.f; // 弹道模拟精度
	PathParams.ProjectileRadius = 5.f; // 投射物体积
	PathParams.TraceChannel = ECC_HitBox; 
	PathParams.ActorsToIgnore.Add(GetOwner());

	FPredictProjectilePathResult PathResult;
	// 模拟投射物飞行轨迹，看是否撞到了拉回来的头部
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
	
	if (PathResult.HitResult.bBlockingHit)
	{
		ResetHitBoxes(HitCharacter, CurrentFrame);
		EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
		return FServerSideRewindResult{true, true};
	}
	
	// 没打到头，开启全身
	for (const auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
		}
	}
	
	// 模拟轨迹撞击全身
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
	if (PathResult.HitResult.bBlockingHit)
	{
		ResetHitBoxes(HitCharacter, CurrentFrame);
		EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
		return FServerSideRewindResult{true, false};
	}
	
	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{false, false};
}

// ==========================================
// 霰弹枪验证逻辑 (多弹片批处理)
// ==========================================

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunServerSideRewind(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	for (const auto& Frame : HitCharacters)
	{
		if (Frame == nullptr) return FShotgunServerSideRewindResult();
	}
	
	TArray<FFramePackage> FrameToCheck;
	for (ABlasterCharacter* HitCharacter : HitCharacters)
	{
		FrameToCheck.Add(GetFrameToCheck(HitCharacter, HitTime));
	}
	return ShotgunConfirmHit(FrameToCheck, TraceStart, HitLocations);
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	FShotgunServerSideRewindResult Confirm = ShotgunServerSideRewind(HitCharacters, TraceStart, HitLocations, HitTime);

	// 遍历结算累加伤害
	for (auto& HitCharacter : HitCharacters)
	{
		if (HitCharacter == nullptr || BlasterCharacter == nullptr || BlasterCharacter->GetEquippedWeapon() == nullptr) continue;
		
		float TotalDamage = 0.f;
		if (Confirm.HeadShots.Contains(HitCharacter)) TotalDamage += Confirm.HeadShots[HitCharacter] * BlasterCharacter->GetEquippedWeapon()->GetHeadShotDamage();
		if (Confirm.BodyShots.Contains(HitCharacter)) TotalDamage += Confirm.BodyShots[HitCharacter] * BlasterCharacter->GetEquippedWeapon()->GetDamage();
		
		UGameplayStatics::ApplyDamage(HitCharacter, TotalDamage, BlasterCharacter->Controller, BlasterCharacter->GetEquippedWeapon(), UDamageType::StaticClass());
	}
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunConfirmHit( const TArray<FFramePackage>& FramePackages, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations)
{
	for (const auto& Frame : FramePackages)
	{
		if (Frame.BlasterCharacter == nullptr) return FShotgunServerSideRewindResult();
	}
	
	FShotgunServerSideRewindResult ShotgunResult;
	TArray<FFramePackage> CurrentFrames;
	
	// 批量将所有被打中的人倒带
	for (auto& Frame : FramePackages)
	{
		FFramePackage CurrentFrame;
		CurrentFrame.BlasterCharacter = Frame.BlasterCharacter;
		CacheBoxPositions(Frame.BlasterCharacter, CurrentFrame);
		MoveBoxes(Frame.BlasterCharacter, Frame);
		EnableCharacterMeshCollision(Frame.BlasterCharacter, ECollisionEnabled::NoCollision);
		CurrentFrames.Add(CurrentFrame);
	}
	
	// 阶段一：集体开头部碰撞
	for (const auto& Frame : FramePackages)
	{
		UBoxComponent* HeadBox = Frame.BlasterCharacter->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
	}
	
	UWorld* World = GetWorld();
	// 对每一发霰弹弹丸进行测试
	for (const auto& HitLocation : HitLocations)
	{
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			FHitResult ConfirmHitResult;
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
			if (ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor()))
			{
				// 累计爆头数量
				if (ShotgunResult.HeadShots.Contains(HitCharacter)) ShotgunResult.HeadShots[HitCharacter]++;
				else ShotgunResult.HeadShots.Emplace(HitCharacter, 1);
			}
		}
	}
	
	// 阶段二：集体关头，开身体碰撞
	for (auto& Frame : FramePackages)
	{
		for (auto& HitBoxPair : Frame.BlasterCharacter->HitCollisionBoxes)
		{
			if (HitBoxPair.Value != nullptr)
			{
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			}
		}
		UBoxComponent* HeadBox = Frame.BlasterCharacter->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 已经算过头了，关闭避免二次叠加
	}
	
	// 再次对所有弹丸进行测试
	for (auto& HitLocation : HitLocations)
	{
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			FHitResult ConfirmHitResult;
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
			if (ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor()))
			{
				// 累计身体命中数量
				if (ShotgunResult.BodyShots.Contains(HitCharacter)) ShotgunResult.BodyShots[HitCharacter]++;
				else ShotgunResult.BodyShots.Emplace(HitCharacter, 1);
			}
		}
	}
	
	// 批量恢复所有人到现实位置
	for (auto& Frame : CurrentFrames)
	{
		ResetHitBoxes(Frame.BlasterCharacter, Frame);
		EnableCharacterMeshCollision(Frame.BlasterCharacter, ECollisionEnabled::QueryAndPhysics);
	}
	
	return ShotgunResult;
}