// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterComponents/LagCompensationComponent.h"
#include "Blaster/Blaster.h"
#include "Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon/BlasterWeapon.h"

// 构造函数
ULagCompensationComponent::ULagCompensationComponent()
{
	// 允许该组件每帧调用 TickComponent。延迟补偿的核心依赖于每帧记录数据
	PrimaryComponentTick.bCanEverTick = true;
}

// 游戏开始时调用
void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 每帧更新函数
void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// 在服务器端，每一帧都会记录当前玩家的命中盒位置（即录制“历史帧”）
	SaveFramePackageServer();
}

// 调试用的辅助函数：在世界中画出某一历史帧的命中盒包
void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color) const
{
	// 遍历包裹中的每一个命中盒信息
	for (auto& BoxInfo : Package.HitBoxInfo)
	{
		// 绘制调试盒子（参数：世界上下文、位置、半尺寸、旋转、颜色、是否持久显示、显示时长）
		DrawDebugBox(
			GetWorld(),
			BoxInfo.Value.Location,
			BoxInfo.Value.BoxExtent,
			FQuat(BoxInfo.Value.Rotation),
			Color,
			false,
			4.f
		);
	}
}

// ==========================================
// 即中武器（HitScan，如步枪）的回滚与伤害判定
// ==========================================

// 服务器执行：根据客户端提供的开枪时间和轨迹，执行倒带验证
FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime)
{
	// 获取目标角色在 HitTime（客户端开枪时刻）那一瞬间的历史帧数据（包含插值）
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	// 将目标拉回到历史位置，发射射线验证，并返回验证结果（是否命中、是否爆头）
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

// 客户端调用的 RPC：请求服务器判定一次射击
void ULagCompensationComponent::ServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime)
{
	// 1. 服务器端进行倒带验证
	FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);
	
	// 2. 如果验证通过（确认命中），且角色和武器均有效
	if (BlasterCharacter && HitCharacter && BlasterCharacter->GetEquippedWeapon() && Confirm.bHitConfirmed)
	{
		// 根据是否爆头，从当前装备的武器中获取对应的伤害值
		const float Damage = Confirm.bHeadShot ? BlasterCharacter->GetEquippedWeapon()->GetHeadShotDamage() : BlasterCharacter->GetEquippedWeapon()->GetDamage();

		// 应用伤害给目标角色
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			Damage,
			BlasterCharacter->Controller,
			BlasterCharacter->GetEquippedWeapon(),
			UDamageType::StaticClass()
		);
	}
}

// ==========================================
// 投射物武器（Projectile，如火箭弹/榴弹）的回滚与伤害判定
// ==========================================

FServerSideRewindResult ULagCompensationComponent::ProjectileServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	// 获取目标角色在 HitTime 时的历史数据
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	// 投射物验证方法：模拟抛物线轨迹而不是直线射线
	return ProjectileConfirmHit(FrameToCheck, HitCharacter, TraceStart, InitialVelocity, HitTime);
}

void ULagCompensationComponent::ProjectileServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	FServerSideRewindResult Confirm = ProjectileServerSideRewind(HitCharacter, TraceStart, InitialVelocity, HitTime);

	if (BlasterCharacter && HitCharacter && Confirm.bHitConfirmed && BlasterCharacter->GetEquippedWeapon())
	{
		const float Damage = Confirm.bHeadShot ? BlasterCharacter->GetEquippedWeapon()->GetHeadShotDamage() : BlasterCharacter->GetEquippedWeapon()->GetDamage();

		UGameplayStatics::ApplyDamage(
			HitCharacter,
			Damage,
			BlasterCharacter->Controller,
			BlasterCharacter->GetEquippedWeapon(),
			UDamageType::StaticClass()
		);
	}
}

// ==========================================
// 霰弹枪（Shotgun）的回滚与伤害判定
// ==========================================

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunServerSideRewind(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	TArray<FFramePackage> FrameToCheck;
	// 遍历所有被打中的角色，收集他们每个人在 HitTime 时的历史帧数据
	for (ABlasterCharacter* HitCharacter : HitCharacters)
	{
		FrameToCheck.Add(GetFrameToCheck(HitCharacter, HitTime));
	}
	// 批量执行验证
	return ShotgunConfirmHit(FrameToCheck, TraceStart, HitLocations);
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	FShotgunServerSideRewindResult Confirm = ShotgunServerSideRewind(HitCharacters, TraceStart, HitLocations, HitTime);

	// 遍历每一个被击中的角色，分别计算累加伤害
	for (auto& HitCharacter : HitCharacters)
	{
		if (HitCharacter == nullptr || BlasterCharacter == nullptr || BlasterCharacter->GetEquippedWeapon() == nullptr) continue;
		
		float TotalDamage = 0.f;
		// 累加爆头伤害（爆头次数 * 武器单发爆头伤害）
		if (Confirm.HeadShots.Contains(HitCharacter))
		{
			float HeadShotDamage = Confirm.HeadShots[HitCharacter] * BlasterCharacter->GetEquippedWeapon()->GetHeadShotDamage();
			TotalDamage += HeadShotDamage;
		}
		// 累加打身体伤害（身体命中次数 * 武器单发基础伤害）
		if (Confirm.BodyShots.Contains(HitCharacter))
		{
			float BodyShotDamage = Confirm.BodyShots[HitCharacter] * BlasterCharacter->GetEquippedWeapon()->GetDamage();
			TotalDamage += BodyShotDamage;
		}
		
		// 结算该角色所受的总伤害
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			TotalDamage,
			BlasterCharacter->Controller,
			BlasterCharacter->GetEquippedWeapon(),
			UDamageType::StaticClass()
		);
	}
}

// ==========================================
// 核心历史查找与插值算法
// ==========================================

// 获取指定时间点的历史帧数据（通过双向链表查找）
FFramePackage ULagCompensationComponent::GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime)
{
	// 容错检查：确保目标角色、其组件以及历史记录链表有效
	bool bReturn =
		HitCharacter == nullptr || HitCharacter->GetLagCompensationComponent() == nullptr || HitCharacter->GetLagCompensationComponent()->FrameHistory.GetHead() == nullptr ||HitCharacter->GetLagCompensationComponent()->FrameHistory.GetTail() == nullptr;
	
	if (bReturn) return FFramePackage();
	
	FFramePackage FrameToCheck;
	bool bShouldInterpolate = true; // 标记是否需要插值（如果时间正好吻合就不需要）
	
	// 获取目标的整个历史记录链表（Head是最新的记录，Tail是最旧的记录）
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensationComponent()->FrameHistory;
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;
	
	// 情况1：开枪时间比我们保存的最旧的记录还要老（网络太卡了，超过了最大记录时间）
	if (OldestHistoryTime > HitTime) return FFramePackage(); // 无法回退，直接失败
	
	// 情况2：正好等于最旧记录的时间
	if (OldestHistoryTime == HitTime)
	{
		FrameToCheck = FrameHistory.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	
	// 情况3：开枪时间比最新的记录还要新（说明服务器的时钟可能刚好晚于推算时间）
	if (NewestHistoryTime <= HitTime)
	{
		FrameToCheck = FrameHistory.GetHead()->GetValue(); // ✅ 修正：直接取最新的一帧
		bShouldInterpolate = false;
	}
	
	// 开始在链表中搜寻夹住 HitTime 的两帧（Older 和 Younger）
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = Younger;
	
	while (Older->GetValue().Time > HitTime) // 只要当前帧的时间还比开枪时间新，就继续往回找
	{
		if (Older->GetNextNode() == nullptr) break;
		Older = Older->GetNextNode(); // 往过去推演
		if (Older->GetValue().Time > HitTime)
		{
			Younger = Older; // Younger始终紧跟在Older前面（时间更新）
		}
	}
	
	// 情况4：运气极好，某一帧的时间刚好等于开枪时间
	if (Older->GetValue().Time == HitTime)
	{
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	
	// 情况5：开枪时间处于 Older 和 Younger 之间，执行插值推算
	if (bShouldInterpolate)
	{
		FrameToCheck = InterpolateBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}
	
	// 标记这个帧包属于哪个角色
	FrameToCheck.BlasterCharacter = HitCharacter;
	return FrameToCheck; 
}

// 两个历史帧之间的线性插值算法
FFramePackage ULagCompensationComponent::InterpolateBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime)
{
	// 计算两帧之间的时间差
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	// 计算开枪时间在两帧时间差中所占的比例（0.0 ~ 1.0 之间）
	const float InterpolationFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);
	
	FFramePackage InterpolatedFrame;
	InterpolatedFrame.Time = HitTime;
	
	// 遍历目标身上的每一个命中盒（头、胸、四肢等）
	for (auto& YoungerHitBox : YoungerFrame.HitBoxInfo)
	{
		const FName& HitBoxName = YoungerHitBox.Key;
		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[HitBoxName];
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[HitBoxName];
		
		FBoxInformation InterpBoxInfo;
		// 对盒子的位置进行线性插值
		InterpBoxInfo.Location = FMath::VInterpTo(OlderBox.Location, YoungerBox.Location, InterpolationFraction, InterpolationFraction);
		// 对盒子的旋转进行球面线性插值
		InterpBoxInfo.Rotation = FMath::RInterpTo(OlderBox.Rotation, YoungerBox.Rotation, InterpolationFraction, InterpolationFraction);
		// 盒子的大小不会随时间改变，直接取原值
		InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;
		
		InterpolatedFrame.HitBoxInfo.Add(HitBoxName, InterpBoxInfo);
	}
	return InterpolatedFrame;
}

// ==========================================
// 命中盒(HitBox) 物理状态管理
// ==========================================

// 缓存角色当前真正的命中盒状态（为了之后能还原它）
void ULagCompensationComponent::CacheBoxPositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage)
{
	if (HitCharacter == nullptr) return;
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

// “时间旅行”：强行将角色当前的命中盒移动到历史帧包规定的位置
void ULagCompensationComponent::MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& FramePackage)
{
	if (HitCharacter == nullptr) return;
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

// “回归现实”：将角色命中盒恢复到刚刚缓存的当前位置，并关闭命中盒碰撞（平时是不开启的）
void ULagCompensationComponent::ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& FramePackage)
{
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			if (const FBoxInformation* BoxValue = FramePackage.HitBoxInfo.Find(HitBoxPair.Key))
			{
				HitBoxPair.Value->SetWorldLocation(BoxValue->Location);
				HitBoxPair.Value->SetWorldRotation(BoxValue->Rotation);
				HitBoxPair.Value->SetBoxExtent(BoxValue->BoxExtent);
				// 恢复后关闭它的碰撞
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}

// 开启/关闭角色身体外观网格体（胶囊体或网格体）的碰撞
void ULagCompensationComponent::EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled)
{
	if (HitCharacter && HitCharacter->GetMesh())
	{
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}

// ==========================================
// 录制系统的底层实现
// ==========================================

// 服务器每帧调用：生成新的帧包，存入链表，清理过期帧
void ULagCompensationComponent::SaveFramePackageServer()
{
	// 只有服务器有权录制
	if (BlasterCharacter == nullptr || !BlasterCharacter->HasAuthority()) return;
	
	// 如果记录不足两条，直接添加
	if (FrameHistory.Num() <= 1)
	{
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame); // 加到链表头部（最新）
	}
	else
	{
		// 计算当前录制的总时长 (最新帧 - 最旧帧)
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		
		// 如果超出了最大记录时长（比如4秒），则剔除尾部的最旧帧
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			if (FrameHistory.GetTail()) // 安全检查
			{
				HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
			}
			else break;
		}
		
		// 剔除完毕后，插入当前最新帧
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);
	}
}

// 将角色当前的碰撞盒数据打包封装成 FFramePackage
void ULagCompensationComponent::SaveFramePackage(FFramePackage& FramePackage)
{
	BlasterCharacter = BlasterCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterCharacter;
	if (BlasterCharacter)
	{
		FramePackage.Time = GetWorld()->GetTimeSeconds(); // 记录当前服务器世界时间
		FramePackage.BlasterCharacter = BlasterCharacter;
		for (const auto& HitBox : BlasterCharacter->HitCollisionBoxes)
		{
			FBoxInformation BoxInformation;
			BoxInformation.Location = HitBox.Value->GetComponentLocation();
			BoxInformation.Rotation = HitBox.Value->GetComponentRotation();
			BoxInformation.BoxExtent = HitBox.Value->GetScaledBoxExtent();
			FramePackage.HitBoxInfo.Add(HitBox.Key, BoxInformation);
		}
	}
}

// ==========================================
// 倒带确认逻辑：碰撞检测核心
// ==========================================

// HitScan 倒带检测
FServerSideRewindResult ULagCompensationComponent::ConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	// 1. 记录现在的位置
	CacheBoxPositions(HitCharacter, CurrentFrame);
	// 2. 退回过去的位置
	MoveBoxes(HitCharacter, FramePackage);
	// 3. 关闭目标的外皮胶囊碰撞，防止射线被挡住
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);
	
	// --- 第一阶段：优先判定爆头 ---
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block); // 设置通道响应

	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f; // 稍微拉长射线防止精度问题
	if (UWorld* World = GetWorld())
	{
		FHitResult ConfirmHitResult;
		// ❌ 修复 1：发射专门探测 ECC_HitBox 通道的射线
		World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
		
		if (ConfirmHitResult.bBlockingHit) // 爆头成功
		{
			ResetHitBoxes(HitCharacter, CurrentFrame); // 还原位置
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics); // 还原外皮碰撞
			return FServerSideRewindResult{ true, true }; // 返回命中，且是爆头
		}
		else // 没打到头
		{
			// --- 第二阶段：开启全身体碰撞盒判定打身体 ---
			for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
			{
				if (HitBoxPair.Value != nullptr)
				{
					HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
				}
			}
			// ❌ 同理修复：用 HitBox 通道再射一次
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
			if (ConfirmHitResult.bBlockingHit) // 身体命中成功
			{
				ResetHitBoxes(HitCharacter, CurrentFrame);
				EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
				return FServerSideRewindResult{ true, false }; // 返回命中，不是爆头
			}
		}
	}
	
	// 彻底空枪
	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

// 投射物（抛物线）倒带检测
FServerSideRewindResult ULagCompensationComponent::ProjectileConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, FramePackage);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);
	
	// 开启头部碰撞盒
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
	
	// 设置引擎内置的弹道预测参数（模拟子弹下坠等）
	FPredictProjectilePathParams PathParams;
	PathParams.bTraceWithCollision = true;
	PathParams.MaxSimTime = MaxRecordTime;
	PathParams.LaunchVelocity = InitialVelocity;
	PathParams.StartLocation = TraceStart;
	PathParams.SimFrequency = 15.f; // 模拟精度
	PathParams.ProjectileRadius = 5.f; // 子弹判定半径
	PathParams.TraceChannel = ECC_HitBox; // 针对 HitBox 碰撞通道
	PathParams.ActorsToIgnore.Add(GetOwner());

	FPredictProjectilePathResult PathResult;
	// 执行弹道预测（第一阶段查头）
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
	
	if (PathResult.HitResult.bBlockingHit)
	{
		ResetHitBoxes(HitCharacter, CurrentFrame);
		EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
		// ❌ 修复 2：命中了头部盒子，理应是爆头！(true, true)
		return FServerSideRewindResult{true, true};
	}
	
	// 开启全身碰撞盒
	for (const auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
		}
	}
	
	// 执行弹道预测（第二阶段查身体）
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

// 霰弹枪（多弹片）倒带检测
FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunConfirmHit( const TArray<FFramePackage>& FramePackages, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations)
{
	for (const auto& Frame : FramePackages)
	{
		if (Frame.BlasterCharacter == nullptr) return FShotgunServerSideRewindResult();
	}
	
	FShotgunServerSideRewindResult ShotgunResult;
	TArray<FFramePackage> CurrentFrames;
	
	// 批量将所有被打中的角色退回历史位置
	for (auto& Frame : FramePackages)
	{
		FFramePackage CurrentFrame;
		CurrentFrame.BlasterCharacter = Frame.BlasterCharacter;
		CacheBoxPositions(Frame.BlasterCharacter, CurrentFrame);
		MoveBoxes(Frame.BlasterCharacter, Frame);
		EnableCharacterMeshCollision(Frame.BlasterCharacter, ECollisionEnabled::NoCollision);
		CurrentFrames.Add(CurrentFrame);
	}
	
	// ======================================
	// 第一阶段：集体查头部判定
	// ======================================
	// 仅开启所有人的头部碰撞盒
	for (const auto& Frame : FramePackages)
	{
		UBoxComponent* HeadBox = Frame.BlasterCharacter->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
	}
	
	UWorld* World = GetWorld();
	// 对每一发霰弹弹丸发射射线
	for (const auto& HitLocation : HitLocations)
	{
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			FHitResult ConfirmHitResult;
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
			if (ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor()))
			{
				// ❌ 修复 3：仅开启头部时命中，应累计爆头数量 (HeadShots)
				if (ShotgunResult.HeadShots.Contains(HitCharacter))
				{
					ShotgunResult.HeadShots[HitCharacter]++;
				}
				else
				{
					ShotgunResult.HeadShots.Emplace(HitCharacter, 1);
				}
			}
		}
	}
	
	// ======================================
	// 第二阶段：集体查身体判定
	// ======================================
	for (auto& Frame : FramePackages)
	{
		// 开启全身碰撞
		for (auto& HitBoxPair : Frame.BlasterCharacter->HitCollisionBoxes)
		{
			if (HitBoxPair.Value != nullptr)
			{
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			}
		}
		// 关闭头部碰撞（因为第一阶段已经算过头部的伤害了，避免二次叠加）
		UBoxComponent* HeadBox = Frame.BlasterCharacter->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	// 再对所有弹丸射一遍线
	for (auto& HitLocation : HitLocations)
	{
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			FHitResult ConfirmHitResult;
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECC_HitBox);
			if (ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor()))
			{
				// 累计打身体的数量
				if (ShotgunResult.BodyShots.Contains(HitCharacter))
				{
					ShotgunResult.BodyShots[HitCharacter]++;
				}
				else
				{
					ShotgunResult.BodyShots.Emplace(HitCharacter, 1);
				}
			}
		}
	}
	
	// ======================================
	// 收尾还原
	// ======================================
	// 将所有人还原到现实位置
	for (auto& Frame : CurrentFrames)
	{
		ResetHitBoxes(Frame.BlasterCharacter, Frame);
		EnableCharacterMeshCollision(Frame.BlasterCharacter, ECollisionEnabled::QueryAndPhysics);
	}
	
	return ShotgunResult;
}