// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/List.h"
#include "LagCompensationComponent.generated.h"

// 声明UE反射结构体，BlueprintType表示该结构体可在蓝图中使用
USTRUCT(BlueprintType)
// 定义结构体：存储单个碰撞盒/命中盒的核心信息
struct FBoxInformation
{
	// UE结构体必需宏，生成反射、序列化、蓝图绑定等底层代码
	GENERATED_BODY()
    
	// UE属性宏，标记该变量为UE可识别的属性（无参数=默认属性）
	UPROPERTY()
	// 盒子的世界空间位置
	FVector Location;
    
	// UE属性宏
	UPROPERTY()
	// 盒子的世界空间旋转
	FRotator Rotation;
    
	// UE属性宏
	UPROPERTY()
	// 盒子的半尺寸（碰撞盒的扩展大小，决定盒子范围）
	FVector BoxExtent;
};

// 定义结构体：单帧数据包裹体，存储一帧内的所有命中盒数据
USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()
	
	// 该帧对应的时间戳（用于服务器回退判定）
	UPROPERTY()
	float Time;
	
	// 映射表：Key=命中盒名称(FName)，Value=对应盒子的信息(FBoxInformation)
	UPROPERTY()
	TMap<FName, FBoxInformation> HitBoxInfo;
	
	UPROPERTY()
	ABlasterCharacter* BlasterCharacter;
};

// 定义结构体：基础的服务器端回滚判定结果
USTRUCT(BlueprintType)
struct FServerSideRewindResult
{
	GENERATED_BODY()
	
	// 是否确认命中（服务器判定的最终结果）
	UPROPERTY()
	bool bHitConfirmed;
	
	// 是否为爆头命中
	UPROPERTY()
	bool bHeadShot;
};

// 声明UE反射结构体，支持蓝图使用
USTRUCT(BlueprintType)
// 定义结构体：霰弹枪专属的服务器端回滚结果（霰弹多弹片，需统计多次命中）
struct FShotgunServerSideRewindResult
{
	// UE结构体必需宏
	GENERATED_BODY()

	// UE属性宏
	UPROPERTY()
	// 爆头映射表：Key=被命中的角色对象，Value=爆头命中的次数
	TMap<ABlasterCharacter*, uint32> HeadShots;

	// UE属性宏
	UPROPERTY()
	// 身体命中映射表：Key=被命中的角色对象，Value=身体命中的次数
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
	
	/*
	 * ServerScoreRequest
	 */
	FServerSideRewindResult ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime);
	
	UFUNCTION(Server, Reliable)
	void ServerScoreRequest(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime);
	
	
	/*
	 * ProjectileServerScoreRequest
	 */
	FServerSideRewindResult ProjectileServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime);
	
	UFUNCTION(Server, Reliable)
	void ProjectileServerScoreRequest(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime);
	
	
	/*
	 * ShotgunServerScoreRequest
	 */
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
	
	/*
	 * ConfirmHit
	 */
	FServerSideRewindResult ConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation);
	
	/*
	* ProjectileConfirmHit
	*/
	FServerSideRewindResult ProjectileConfirmHit(const FFramePackage& FramePackage, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime);
	
	/* 
	 * ShotgunConfirmHit
	 * */	
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
