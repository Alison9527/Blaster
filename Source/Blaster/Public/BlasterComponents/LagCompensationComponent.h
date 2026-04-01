// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/List.h"
#include "LagCompensationComponent.generated.h"

// ⚙️ 核心执行流程解析
// 整个延迟补偿的生命周期可以分为 5 个阶段：
//
// 阶段 1：历史帧录制 (TickComponent -> SaveFramePackageServer)
// 动作：在服务器上，每个玩家的 LagCompensationComponent 每一帧（Tick）都在静默工作。
//
// 逻辑：它会把玩家身上所有的命中盒（HitBoxes，如头、胸、四肢）当前的世界坐标、旋转和大小记录下来，打包成一个 FFramePackage，并打上当前的服务器时间戳。
//
// 存储：这些包被存入一个双向链表（FrameHistory）中。为了节省内存，链表只保留最近几秒（MaxRecordTime = 4.f）的数据，太旧的会被剔除。
//
// 阶段 2：客户端开火与上报 (Client Fire -> ScoreRequest)
// 动作：客户端玩家按下左键开火。客户端的武器代码检测到准星碰到了敌人。
//
// 逻辑：因为网络延迟，客户端看到的世界是过去的世界。客户端会将以下信息打包通过 RPC（如 ServerScoreRequest）发送给服务器：
//
// 打中了谁（HitCharacter）
//
// 开枪起点和落点（TraceStart, HitLocation）
//
// 命中时间（HitTime）：这是最关键的！公式是 当前服务器时间 - 客户端单程延迟(Ping/2)。它代表着“我开枪时，在我眼里的世界是几秒”。
//
// 阶段 3：服务器查找与插值 (GetFrameToCheck -> InterpolateBetweenFrames)
// 动作：服务器收到客户端的请求，开始查阅历史记录。
//
// 逻辑：
//
// 服务器拿到请求中的 HitTime，去被击中玩家的 FrameHistory 链表里翻找。
//
// 它通常找不到时间绝对吻合的那一帧，而是会找到刚好夹住 HitTime 的前一帧（Older）和后一帧（Younger）。
//
// 接着，执行线性插值（VInterpTo/RInterpTo），精准推算出在 HitTime 这一极其精确的微秒时刻，目标玩家各个命中盒的确切位置。
//
// 阶段 4：服务器“时间旅行”与验证 (ConfirmHit)
// 动作：这是最魔幻的一步，服务器让目标的命中盒回到过去。
//
// 逻辑：
//
// 缓存现状：先把目标玩家现在的真实位置（CurrentFrame）缓存下来。
//
// 回到过去：把目标玩家的命中盒强制移动到第 3 步算出来的历史插值位置（MoveBoxes）。
//
// 关闭外壳：关闭玩家表面的大胶囊体网格碰撞，防止干扰。
//
// 射线检测：服务器沿着客户端上报的开枪轨迹（从起点到落点）发射一条射线。
//
// 先只开启头部碰撞盒，如果打中，返回 {true, true}（爆头）。
//
// 如果头没中，开启全身碰撞盒再打一次，如果打中，返回 {true, false}（打身体）。
//
// 回到现在：无论结果如何，瞬间把目标玩家的命中盒恢复到第 1 步缓存的现状（ResetHitBoxes），并恢复胶囊体碰撞。整个过程在 1 帧内完成，其他玩家肉眼根本看不见这个“瞬移”。
//
// 阶段 5：结算伤害
// 动作：根据第 4 步的返回结果，服务器扣除玩家血量，并广播击杀信息。
//
// 🎬 结合具体场景的详细例子
// 假设我们在玩一场游戏，服务器运行在 60  टिक（Tick）。
//
// 玩家 A（狙击手）：网络有点卡，Ping 值为 100ms（单程延迟 SingleTripTime = 50ms 或 0.05秒）。
//
// 玩家 B（目标）：正在从左向右全速奔跑。
//
// ⏱️ 时刻 1：服务器时间 = 10.000 秒
// 此时在服务器上，玩家 B 跑到了坐标 X=500 的位置。
//
// 但在玩家 A 的屏幕上：因为有 50ms 的下行延迟，玩家 A 看到的其实是服务器在 9.950 秒时发来的画面。在 A 的眼里，玩家 B 才跑到坐标 X=450 的位置。
//
// 💥 时刻 2：玩家 A 开火
// 玩家 A 的准星瞄准了屏幕上坐标 X=450 处玩家 B 的头部，按下左键。
//
// 客户端代码立刻计算命中时间：HitTime = 10.000s (当前时间) - 0.050s (单程延迟) = 9.950s。
//
// 玩家 A 的电脑向服务器发送 RPC 数据包：“报告老大，我在 HitTime = 9.950s 的时候，朝着坐标 X=450 的位置开了一枪。”
//
// ⏳ 时刻 3：服务器时间 = 10.050 秒（服务器收到数据包）
// 由于玩家 A 有 50ms 的上行延迟，服务器在 10.050 秒才收到这个开火包。
//
// 此时服务器上的现状：玩家 B 一直在跑，现在已经跑到了坐标 X=550。
//
// 如果不做延迟补偿（没有这段代码）：服务器会直接在现在的世界（X=550）里，沿着 A 报告的轨迹（打向 X=450）发射射线。结果显然是打空了。玩家 A 会愤怒地砸键盘：“我明明爆头了，怎么不掉血？！”
//
// ⏪ 时刻 4：服务器执行倒带（代码介入）
// 服务器收到请求，看到 HitTime = 9.950s。
//
// 服务器翻开玩家 B 的 FrameHistory 账本。找到了两条记录：
//
// 第 9.940 秒：B 在 X=440
//
// 第 9.960 秒：B 在 X=460
//
// 执行插值（InterpolateBetweenFrames）：计算出 9.950 秒时，B 的头部精确位置应该在 X=450。
//
// 服务器悄悄把 B 的头部碰撞盒瞬移到 X=450（MoveBoxes）。
//
// 服务器沿着 A 的射击轨迹打出射线（ConfirmHit）。
//
// 命中！ 射线完美击中了被拉回 X=450 的 B 的头部碰撞盒。
//
// 服务器立刻把 B 的碰撞盒放回 X=550 的真实位置（ResetHitBoxes）。
//
// 💀 时刻 5：结果
// 玩家 A 听到了清脆的爆头音效，觉得游戏判定非常丝滑精准。
//
// 玩家 B 在 10.050s 时已经跑进了掩体（X=550 处有一堵墙），但他突然暴毙了。在他的视角里，他是**“躲进墙后才被打死的”**。
//
// 💡 总结与启发
// 你代码中的这套逻辑完美地展示了射击游戏网络同步的核心妥协：Favor the Shooter（优先照顾开火者）。
//
// 这套代码非常棒地处理了三种不同的武器类型：
//
// HitScan（即中武器，如步枪/狙击）：直线拉回，直接射击判定。
//
// Projectile（飞行物，如榴弹/火箭弹）：由于子弹有飞行时间，回滚判定时不能用简单的直线，而是使用 PredictProjectilePath 模拟出一条抛物线去碰撞历史盒子。
//
// Shotgun（霰弹枪）：一次开火有十几发弹片，为了性能，代码极其聪明地把十几发弹片打包在一次历史回退中处理。而不是让服务器退回过去十几次。它退回一次，一口气算完所有弹片的射线，累加伤害，再把历史拨回正轨。
//
// 这套重构后的代码逻辑闭环严密，只要保证碰撞通道（ECC_HitBox）配置正确，就能提供非常现代且优秀的射击手感。

USTRUCT(BlueprintType)
struct FBoxInformation
{
	GENERATED_BODY()
    
	UPROPERTY()
	FVector Location;
    
	UPROPERTY()
	FRotator Rotation;
    
	UPROPERTY()
	FVector BoxExtent;
};

USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()
	
	UPROPERTY()
	float Time;
	
	UPROPERTY()
	TMap<FName, FBoxInformation> HitBoxInfo;
	
	UPROPERTY()
	class ABlasterCharacter* BlasterCharacter;
};

USTRUCT(BlueprintType)
struct FServerSideRewindResult
{
	GENERATED_BODY()
	
	UPROPERTY()
	bool bHitConfirmed;
	
	UPROPERTY()
	bool bHeadShot;
};

USTRUCT(BlueprintType)
struct FShotgunServerSideRewindResult
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<ABlasterCharacter*, uint32> HeadShots;

	UPROPERTY()
	TMap<ABlasterCharacter*, uint32> BodyShots;
};  

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULagCompensationComponent();
	friend class ABlasterCharacter;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ShowFramePackage(const FFramePackage& Package, const FColor& Color) const;
	
	// === HitScan 武器回滚 ===
	FServerSideRewindResult ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime);
	
	UFUNCTION(Server, Reliable)
	void ServerScoreRequest(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime);
	
	// === 投射物 武器回滚 ===
	FServerSideRewindResult ProjectileServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime);
	
	UFUNCTION(Server, Reliable)
	void ProjectileServerScoreRequest(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime);
	
	// === 霰弹枪 武器回滚 ===
	FShotgunServerSideRewindResult ShotgunServerSideRewind(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime);
	
	UFUNCTION(Server, Reliable)
	void ShotgunServerScoreRequest(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime);

protected:
	virtual void BeginPlay() override;
	void SaveFramePackage(FFramePackage& FramePackage);
	FFramePackage InterpolateBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime);
	void CacheBoxPositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage);
	void MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& FramePackage);
	void ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& FramePackage);
	void EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled);
	void SaveFramePackageServer();
	FFramePackage GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime);
	
	// 核心判定逻辑
	FServerSideRewindResult ConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation);
	FServerSideRewindResult ProjectileConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime);
	FShotgunServerSideRewindResult ShotgunConfirmHit(const TArray<FFramePackage>& FramePackages, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations);

private:
	UPROPERTY()
	ABlasterCharacter* BlasterCharacter;
	
	UPROPERTY()
	class ABlasterPlayerController* BlasterPlayerController;
	
	TDoubleLinkedList<FFramePackage> FrameHistory;
	
	UPROPERTY(EditAnywhere)
	float MaxRecordTime = 4.f;
};