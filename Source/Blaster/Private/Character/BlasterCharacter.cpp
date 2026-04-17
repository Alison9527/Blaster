// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BlasterCharacter.h"
#include "Blaster/Blaster.h"
#include "BlasterComponents/BuffComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/BlasterWeapon.h"
#include "BlasterComponents/CombatComponent.h"
#include "BlasterComponents/LagCompensationComponent.h"
#include "BlasterTypes/Team.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterTypes/TurningInPlace.h"
#include "Components/BoxComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "GameMode/BlasterGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "PlayerState/BlasterPlayerState.h"
#include "GameState/BlasterGameState.h"
#include "PlayerStart/TeamPlayerStart.h"

ABlasterCharacter::ABlasterCharacter()
{
    // 开启 Tick
    PrimaryActorTick.bCanEverTick = true;
    // 碰撞处理：如果位置冲突则调整，但总是生成
    SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 创建弹簧臂组件
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
    SpringArmComponent->SetupAttachment(GetMesh());
    SpringArmComponent->TargetArmLength = 600.f;
    SpringArmComponent->bUsePawnControlRotation = true;
    
    // 创建摄像机组件
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
    CameraComponent->bUsePawnControlRotation = false;

    // 初始控制器旋转 Yaw 为 false，配合移动时转向运动
    bUseControllerRotationYaw = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;

    // 创建战斗组件
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    CombatComponent->SetIsReplicated(true);
    
    // 创建增益组件
    BuffComponent = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
    BuffComponent->SetIsReplicated(true);
    
    // 创建延迟补偿组件
    LagCompensationComponent = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensationComponent"));

    // 允许蹲下
    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
    // 胶囊体忽略相机通道
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
    // 骨骼网格体设为骨骼物体类型，忽略相机，阻挡可见性
    GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
    GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
    // 移动旋转速率
    GetCharacterMovement()->RotationRate = FRotator(0, 0, 850.f);
    
    // 初始转身状态
    TurningInPlace = ETurningInPlace::ETIP_NotTurning;
    // 网络更新频率
    SetNetUpdateFrequency(66.0f);
    SetMinNetUpdateFrequency(33.0f);

    // 溶解时间轴组件
    DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));
    
    // 手雷模型附着到手雷插槽
    AttachedGrenade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttachedGrenade"));
    AttachedGrenade->SetupAttachment(GetMesh(), FName("GrenadeSocket"));
    AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 保证动画 Tick 始终进行（用于偏移等）
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    
    /*
     * 服务器回滚用的碰撞盒创建
     */
    Head = CreateDefaultSubobject<UBoxComponent>(TEXT("Head"));
    Head->SetupAttachment(GetMesh(), FName("head"));
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("head"), Head);
	
    Pelvis = CreateDefaultSubobject<UBoxComponent>(TEXT("Pelvis"));
    Pelvis->SetupAttachment(GetMesh(), FName("pelvis"));
    Pelvis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("pelvis"), Pelvis);
	
    Spine_02 = CreateDefaultSubobject<UBoxComponent>(TEXT("Spine_02"));
    Spine_02->SetupAttachment(GetMesh(), FName("spine_02"));
    Spine_02->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("spine_02"), Spine_02);
	
    Spine_03 = CreateDefaultSubobject<UBoxComponent>(TEXT("Spine_03"));
    Spine_03->SetupAttachment(GetMesh(), FName("spine_03"));
    Spine_03->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("spine_03"), Spine_03);
	
    UpperArm_L = CreateDefaultSubobject<UBoxComponent>(TEXT("UpperArm_L"));
    UpperArm_L->SetupAttachment(GetMesh(), FName("upperarm_l"));
    UpperArm_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("upperarm_l"), UpperArm_L);
	
    UpperArm_R = CreateDefaultSubobject<UBoxComponent>(TEXT("UpperArm_R"));
    UpperArm_R->SetupAttachment(GetMesh(), FName("upperarm_r"));
    UpperArm_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("upperarm_r"), UpperArm_R);
	
    LowerArm_L = CreateDefaultSubobject<UBoxComponent>(TEXT("LowerArm_L"));
    LowerArm_L->SetupAttachment(GetMesh(), FName("lowerarm_l"));
    LowerArm_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("lowerarm_l"), LowerArm_L);
	
    LowerArm_R = CreateDefaultSubobject<UBoxComponent>(TEXT("LowerArm_R"));
    LowerArm_R->SetupAttachment(GetMesh(), FName("lowerarm_r"));
    LowerArm_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("lowerarm_r"), LowerArm_R);
	
    Hand_L = CreateDefaultSubobject<UBoxComponent>(TEXT("Hand_L"));
    Hand_L->SetupAttachment(GetMesh(), FName("hand_l"));
    Hand_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("hand_l"), Hand_L);
	
    Hand_R = CreateDefaultSubobject<UBoxComponent>(TEXT("Hand_R"));
    Hand_R->SetupAttachment(GetMesh(), FName("hand_r"));
    Hand_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("hand_r"), Hand_R);
	
    Blanket = CreateDefaultSubobject<UBoxComponent>(TEXT("Blanket"));
    Blanket->SetupAttachment(GetMesh(), FName("blanket"));
    Blanket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("blanket"), Blanket);
	
    thigh_L = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_L"));
    thigh_L->SetupAttachment(GetMesh(), FName("thigh_l"));
    thigh_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("thigh_l"), thigh_L);
	
    thigh_R = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_R"));
    thigh_R->SetupAttachment(GetMesh(), FName("thigh_r"));
    thigh_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("thigh_r"), thigh_R);
	
    Calf_L = CreateDefaultSubobject<UBoxComponent>(TEXT("Calf_L"));
    Calf_L->SetupAttachment(GetMesh(), FName("calf_l"));
    Calf_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("calf_l"), Calf_L);
	
    Calf_R = CreateDefaultSubobject<UBoxComponent>(TEXT("Calf_R"));
    Calf_R->SetupAttachment(GetMesh(), FName("calf_r"));
    Calf_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("calf_r"), Calf_R);
	
    Foot_L = CreateDefaultSubobject<UBoxComponent>(TEXT("Foot_L"));
    Foot_L->SetupAttachment(GetMesh(), FName("foot_l"));
    Foot_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("foot_l"), Foot_L);
	
    Foot_R = CreateDefaultSubobject<UBoxComponent>(TEXT("Foot_R"));
    Foot_R->SetupAttachment(GetMesh(), FName("foot_r"));
    Foot_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitCollisionBoxes.Add(FName("foot_r"), Foot_R);
}

// 多播：获得领先时播放皇冠特效
void ABlasterCharacter::MulticastGainedTheLead_Implementation()
{
    if (CrownSystem == nullptr) return;
    if (CrownComponent == nullptr)
    {
        CrownComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            CrownSystem,
            GetMesh(),
            FName(),
            GetActorLocation() + FVector(0.f, 0.f, 110.f),
            GetActorRotation(),
            EAttachLocation::KeepWorldPosition,
            false
        );
    }
    if (CrownComponent)
    {
        CrownComponent->Activate();
    }
}

// 多播：失去领先时摧毁皇冠特效
void ABlasterCharacter::MulticastLostTheLead_Implementation()
{
    if (CrownComponent)
    {
        CrownComponent->DestroyComponent();
    }
}

// 根据队伍设置角色材质和溶解材质
void ABlasterCharacter::SetTeamColor(ETeam Team)
{
    if (GetMesh() == nullptr || OriginalMaterial == nullptr) return;
    switch (Team)
    {
    case ETeam::ET_RedTeam:
       GetMesh()->SetMaterial(0, RedMaterial);
       DissolveMaterialInstance = RedDissolveMathInst;
       break;
    case ETeam::ET_BlueTeam:
       GetMesh()->SetMaterial(0, BlueMaterial);
       DissolveMaterialInstance = BlueDissolveMathInst;
       break;
    case ETeam::ET_NoTeam:
       GetMesh()->SetMaterial(0, OriginalMaterial);
       DissolveMaterialInstance = BlueDissolveMathInst;
       break;
    default:
       break;
    }
}

void ABlasterCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // 生成默认武器
    SpawnDefaultWeapon();
    
    // 服务器绑定伤害接收
    if (HasAuthority())
    {
       OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::ReceiveDamage);
    }
    
    // 初始隐藏手雷模型（未投掷时）
    if (AttachedGrenade)
    {
       AttachedGrenade->SetVisibility(false);
    }
}

void ABlasterCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    RotateInPlace(DeltaTime);
    HideCameralIfCharacterClose();
    PollInit();
}

// 复制属性
void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
    DOREPLIFETIME(ABlasterCharacter, Health);
    DOREPLIFETIME(ABlasterCharacter, Shield);
    DOREPLIFETIME(ABlasterCharacter, bDisableGameplay);
}

// 当基于移动复制时，调用 SimProxiesTurn 更新代理转身
void ABlasterCharacter::OnRep_ReplicatedBasedMovement()
{
    Super::OnRep_ReplicatedBasedMovement();
    SimProxiesTurn();
    TimeSinceLastMovementReplication = 0.f;
}

// 淘汰主逻辑（服务器调用）
void ABlasterCharacter::Elim(bool bPlayerLeftGame)
{
    DropOrDestroyWeapons();
    MulticastElim(bPlayerLeftGame);
}

// 多播淘汰（所有客户端执行）
void ABlasterCharacter::MulticastElim_Implementation(bool bPlayerLeftGame)
{
    bLeftGame = bPlayerLeftGame;
    
    // 清空 HUD 武器弹药显示
    if (BlasterPlayerController)
    {
       BlasterPlayerController->SetHUDWeaponAmmo(0);
    }
    
    bEliminated = true;
    PlayElimMontage();

    // 设置溶解材质
    if (DissolveMaterialInstance)
    {
       DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
       GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
       DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
       DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
    }
    StartDissolve();
    
    // 禁用操作和移动
    bDisableGameplay = true;
    GetCharacterMovement()->DisableMovement();
    if (CombatComponent)
    {
       CombatComponent->FireButtonPressed(false);
    }
    
    // 关闭碰撞
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // 生成淘汰特效
    if (ElimBotComponent)
    {
       FVector ElimBotSpawnPont(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z + 200.f);
       ElimBotComponent = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ElimBotEffect, ElimBotSpawnPont, GetActorRotation());
    }
    
    if (ElimBotSound)
    {
       UGameplayStatics::SpawnSoundAtLocation(this, ElimBotSound, GetActorLocation());
    }

    // 本地控制且是狙击枪瞄准状态时，关闭狙击镜 UI
    bool bHideSniperScope = IsLocallyControlled() &&
          CombatComponent &&
          CombatComponent->GetAiming() &&
          CombatComponent->EquippedWeapon &&
          CombatComponent->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle;

    if (bHideSniperScope)
    {
       ShowSniperScopeWidget(false);
    }
    
    // 摧毁皇冠组件
    if (CrownComponent)
    {
        CrownComponent->DestroyComponent();
    }
    
    // 设置淘汰计时器，到时复活
    GetWorldTimerManager().SetTimer(
       ElimTimer,
       this,
       &ABlasterCharacter::ElimTimerFinished,
       ElimDelay
    );
}

// 输入绑定设置
void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlasterCharacter::Jump);
    PlayerInputComponent->BindAxis("MoveForward", this, &ABlasterCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &ABlasterCharacter::MoveRight);
    PlayerInputComponent->BindAxis("Turn", this, &ABlasterCharacter::Turn);
    PlayerInputComponent->BindAxis("LookUp", this, &ABlasterCharacter::LookUp);
    PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ABlasterCharacter::EquipButtonPressed);
    PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ABlasterCharacter::CrouchButtonPressed);
    PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ABlasterCharacter::AimButtonPressed);
    PlayerInputComponent->BindAction("Aim", IE_Released, this, &ABlasterCharacter::AimButtonReleased);
    PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABlasterCharacter::FireButtonPressed);
    PlayerInputComponent->BindAction("Fire", IE_Released, this, &ABlasterCharacter::FireButtonReleased);
    PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ABlasterCharacter::ReloadButtonPressed);
    PlayerInputComponent->BindAction("ThrowGrenade", IE_Pressed, this, &ABlasterCharacter::GrenadeButtonPressed);
}

// 初始化组件引用
void ABlasterCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (CombatComponent)
    {
       CombatComponent->BlasterCharacter = this;
    }
    if (BuffComponent)
    {
       BuffComponent->BlasterCharacter = this;
       BuffComponent->SetInitialSpeeds(GetCharacterMovement()->MaxWalkSpeed, GetCharacterMovement()->MaxWalkSpeedCrouched);
    }
    if (LagCompensationComponent)
    {
       LagCompensationComponent->BlasterCharacter = this;
       if (ABlasterPlayerController* PlayerController = Cast<ABlasterPlayerController>(Controller))
       {
          LagCompensationComponent->BlasterPlayerController = PlayerController;
       }
    }
}

// 播放射击蒙太奇
void ABlasterCharacter::PlayFireMontage(bool bAiming) const
{
    if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && FireWeaponMontage)
    {
       AnimInstance->Montage_Play(FireWeaponMontage);
       FName SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
       AnimInstance->Montage_JumpToSection(SectionName, FireWeaponMontage);
    }
}

// 播放换弹蒙太奇（根据武器类型选段）
void ABlasterCharacter::PlayReloadMontage() const
{
    if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && ReloadMontage)
    {
       AnimInstance->Montage_Play(ReloadMontage);
       FName SectionName;
       switch (CombatComponent->EquippedWeapon->GetWeaponType())
       {
       case EWeaponType::EWT_AssaultRifle:
          SectionName = FName("Rifle");
          break;
       case EWeaponType::EWT_RocketLauncher:
          SectionName = FName("RocketLauncher");
          break;
       case EWeaponType::EWT_Pistol:
       case EWeaponType::EWT_SubmachineGun:
          SectionName = FName("Pistol");
          break;
       case EWeaponType::EWT_Shotgun:
          SectionName = FName("Shotgun");
          break;
       case EWeaponType::EWT_SniperRifle:
          SectionName = FName("SniperRifle");
          break;
       case EWeaponType::EWT_GrenadeLauncher:
          SectionName = FName("GrenadeLauncher");
          break;
       default:
          SectionName = FName("Rifle");
          break;
       }
          
       AnimInstance->Montage_JumpToSection(SectionName, ReloadMontage);
    }
}

// 播放淘汰蒙太奇
void ABlasterCharacter::PlayElimMontage() const
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && ElimMontage)
    {
       AnimInstance->Montage_Play(ElimMontage);
    }
}

// 播放扔雷蒙太奇
void ABlasterCharacter::PlayThrowGrenadeMontage() const
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && ThrowGrenadeMontage)
    {
       AnimInstance->Montage_Play(ThrowGrenadeMontage);
    }
}

// 播放切枪蒙太奇
void ABlasterCharacter::PlaySwapMontage() const
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && SwapMontage)
    {
        AnimInstance->Montage_Play(SwapMontage);
    }
}

// 播放受击蒙太奇
void ABlasterCharacter::PlayHitReactMontage() const
{
    if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && HitReactMontage)
    {
       AnimInstance->Montage_Play(HitReactMontage);
       FName SectionName = FName("FromFront");
       AnimInstance->Montage_JumpToSection(SectionName, HitReactMontage);
    }
}

// 手雷按键处理
void ABlasterCharacter::GrenadeButtonPressed()
{
    if (CombatComponent)
    {
       if (CombatComponent->bHoldingTheFlag) return; // 扛旗时禁止扔雷
       CombatComponent->ThrowGrenade();
    }
}

// 伤害接收（绑定 OnTakeAnyDamage）
void ABlasterCharacter::ReceiveDamage(AActor* DamageActor, float Damage, const UDamageType* DamageType,
                                      class AController* InstigatorController, AActor* DamageCauser)
{
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	if (bEliminated || BlasterGameMode == nullptr) return;
	
	Damage = BlasterGameMode->CalculateDamage(InstigatorController, Controller, Damage);
	
    float DamageToHealth = Damage;
    // 护盾先吸收伤害
    if (Shield > 0.f)
    {
       if (DamageToHealth >= Shield)
       {
          DamageToHealth -= Shield;
          Shield = 0.f;
       }
       else
       {
          Shield -= DamageToHealth;
          DamageToHealth = 0.f;
       }
       UpdateHUDShield();
    }
    
    Health = FMath::Clamp(Health - DamageToHealth, 0.f, MaxHealth);
    UpdateHUDHealth();
    PlayHitReactMontage();

    if (Health == 0.f)
    {
       // 调用游戏模式处理淘汰
       if (ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>())
       {
          BlasterPlayerController = BlasterPlayerController == nullptr ? nullptr : BlasterPlayerController;
          ABlasterPlayerController* AttackController = Cast<ABlasterPlayerController>(InstigatorController);
          BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackController);
       }
    }     
}

void ABlasterCharacter::MoveForward(float Value)
{
    if (bDisableGameplay) return;
    if (Controller != nullptr && Value != 0.f)
    {
       const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
       const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
       AddMovementInput(Direction, Value);
    }
}

void ABlasterCharacter::MoveRight(float Value)
{
    if (bDisableGameplay) return;
    if (Controller != nullptr && Value != 0.f)
    {
       const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
       const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
       AddMovementInput(Direction, Value);
    }
}

void ABlasterCharacter::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

// 重叠武器复制回调
void ABlasterCharacter::OnRep_OverlappingWeapon(ABlasterWeapon* LastWeapon) const
{
    if (OverlappingWeapon)
    {
       OverlappingWeapon->ShowPickupWidget(true);
    }
    if (LastWeapon)
    {
       LastWeapon->ShowPickupWidget(false);
    }
}

// 装备按键处理
void ABlasterCharacter::EquipButtonPressed()
{
    if (bDisableGameplay) return;
    if (CombatComponent)
    {
        if (CombatComponent->bHoldingTheFlag) return; // 扛旗禁止装备
        if (CombatComponent->CombatState == ECombatState::ECS_Unoccupied) SeverEquipButtonPressed();
        
        // 本地预测：如果需要切枪且没有重叠武器，播放切枪蒙太奇
        bool bSwap = CombatComponent->ShouldSwapWeapons() &&
            !HasAuthority() &&
            CombatComponent->CombatState == ECombatState::ECS_Unoccupied &&
            OverlappingWeapon == nullptr;

        if (bSwap)
        {
            PlaySwapMontage();
            CombatComponent->CombatState = ECombatState::ECS_SwappingWeapon;
            bFinishedSwapping = false;
        }
    }
}

void ABlasterCharacter::CrouchButtonPressed()
{
    if (CombatComponent && CombatComponent->bHoldingTheFlag) return;
    if (bDisableGameplay) return;
    if (IsCrouched())
    {
       UnCrouch();
    }
    else
    {
       Crouch();
    }
}

void ABlasterCharacter::ReloadButtonPressed()
{
    if (CombatComponent && CombatComponent->bHoldingTheFlag) return;
    if (bDisableGameplay) return;
    if (CombatComponent)
    {
       CombatComponent->Reload();
    }
}

void ABlasterCharacter::AimButtonPressed()
{
    if (CombatComponent && CombatComponent->bHoldingTheFlag) return;
    if (bDisableGameplay) return;
    if (CombatComponent)
    {
       CombatComponent->SetAiming(true);
    }
}

void ABlasterCharacter::AimButtonReleased()
{
    if (CombatComponent && CombatComponent->bHoldingTheFlag) return;
    if (bDisableGameplay) return;
    if (CombatComponent)
    {
       CombatComponent->SetAiming(false);
    }
}

// 瞄准偏移计算（用于动画）
void ABlasterCharacter::AimOffset(float DeltaTime)
{
    if (CombatComponent && CombatComponent->GetEquippedWeapon() == nullptr) return;
    float Speed = CalculateSpeed();
    bool bIsInAir = GetCharacterMovement()->IsFalling();

    if (Speed == 0.f && !bIsInAir)
    {
       bRotateRootBone = true;
       FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
       FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
       AO_Yaw = DeltaRot.Yaw;
       if (TurningInPlace == ETurningInPlace::ETIP_NotTurning) 
       {
          InterpAO_Yaw = AO_Yaw;
       }
       bUseControllerRotationYaw = true;
       TurnInPlace(DeltaTime);
    }

    if (Speed > 0.f || bIsInAir)
    {
       bRotateRootBone = false;
       StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
       AO_Yaw = 0.f;
       bUseControllerRotationYaw = true;
       TurningInPlace = ETurningInPlace::ETIP_NotTurning;
    }
    CalculateAO_Pitch();
}

// 模拟代理的转身计算
void ABlasterCharacter::SimProxiesTurn()
{
    if (CombatComponent == nullptr || CombatComponent->GetEquippedWeapon() == nullptr) return;

    bRotateRootBone = false;
    float Speed = CalculateSpeed();
    if (Speed > 0.f)
    {
       TurningInPlace = ETurningInPlace::ETIP_NotTurning;
       return;
    }
    
    ProxyRotationLastFrame = ProxyRotation;
    ProxyRotation = GetActorRotation();
    ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

    if (FMath::Abs(ProxyYaw) > TurnThreshold)
    {
       if (ProxyYaw > TurnThreshold)
       {
          TurningInPlace = ETurningInPlace::ETIP_Right;
       }
       else if (ProxyYaw < -TurnThreshold)
       {
          TurningInPlace = ETurningInPlace::ETIP_Left;
       }
       else
       {
          TurningInPlace = ETurningInPlace::ETIP_NotTurning;
       }
       return;
    }
    TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::Jump()
{
    if (CombatComponent && CombatComponent->bHoldingTheFlag) return;
    if (bDisableGameplay) return;
    if (bIsCrouched)
    {
       UnCrouch();
    }
    else
    {
       Super::Jump();
    }
}

void ABlasterCharacter::FireButtonPressed()
{
    if (CombatComponent && CombatComponent->bHoldingTheFlag) return;
    if (bDisableGameplay) return;
    if (CombatComponent)
    {
       CombatComponent->FireButtonPressed(true);
    }
}

void ABlasterCharacter::FireButtonReleased()
{
    if (CombatComponent && CombatComponent->bHoldingTheFlag) return;
    if (bDisableGameplay) return;
    if (CombatComponent)
    {
       CombatComponent->FireButtonPressed(false);
    }
}

// 服务器装备武器RPC
void ABlasterCharacter::SeverEquipButtonPressed_Implementation()
{
    if (CombatComponent)
    {
       if (OverlappingWeapon)
       {
          CombatComponent->EquipWeapon(OverlappingWeapon);
       }
       else if (CombatComponent->ShouldSwapWeapons())
       {
          CombatComponent->SwapWeapons();
       }
    }
}

// 原地转身逻辑
void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
    if (AO_Yaw > 90.f)
    {
       TurningInPlace = ETurningInPlace::ETIP_Right;
    }
    else if (AO_Yaw < -90.f)
    {
       TurningInPlace = ETurningInPlace::ETIP_Left;
    }
    if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
    {
       InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
       AO_Yaw = InterpAO_Yaw;
       if (FMath::Abs(AO_Yaw) < 15.f)
       {
          TurningInPlace = ETurningInPlace::ETIP_NotTurning;
          StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
       }  
    }
}

// 摄像机过近时隐藏角色模型（防止穿模）
void ABlasterCharacter::HideCameralIfCharacterClose()
{
    if (!IsLocallyControlled()) return;
    if ((CameraComponent->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
    {
       GetMesh()->SetVisibility(false);
       if (CombatComponent && CombatComponent->GetEquippedWeapon() && CombatComponent->GetEquippedWeapon()->GetWeaponMesh())
       {
          CombatComponent->GetEquippedWeapon()->GetWeaponMesh()->bOwnerNoSee = true;
       }
       // 隐藏副武器
       if (CombatComponent && CombatComponent->SecondaryWeapon && CombatComponent->SecondaryWeapon->GetWeaponMesh())
       {
          CombatComponent->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = true;
       }
    }
    else
    {
       GetMesh()->SetVisibility(true);
       if (CombatComponent && CombatComponent->GetEquippedWeapon() && CombatComponent->GetEquippedWeapon()->GetWeaponMesh())
       {
          CombatComponent->GetEquippedWeapon()->GetWeaponMesh()->bOwnerNoSee = false;
       }
       if (CombatComponent && CombatComponent->SecondaryWeapon && CombatComponent->SecondaryWeapon->GetWeaponMesh())
       {
          CombatComponent->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = false;
       }
    }
}

float ABlasterCharacter::CalculateSpeed()
{
    FVector Velocity = GetVelocity();
    Velocity.Z = 0.f;
    return Velocity.Size();
}

void ABlasterCharacter::OnRep_Health(float LastHealth)
{
    UpdateHUDHealth();
    if (Health < LastHealth)
    {
       PlayHitReactMontage();
    }
}

void ABlasterCharacter::OnRep_Shield(float LastShield)
{
    UpdateHUDShield();
    if (Shield < LastShield)
    {
       PlayHitReactMontage();
    }
}

void ABlasterCharacter::UpdateHUDHealth()
{
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
    if (BlasterPlayerController)
    {
       BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
    }
}

void ABlasterCharacter::UpdateHUDShield()
{
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
    if (BlasterPlayerController)
    {
       BlasterPlayerController->SetHUDShield(Shield, MaxShield);
    }
}

void ABlasterCharacter::UpdateHUDAmmo()
{
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
    if (BlasterPlayerController && CombatComponent && CombatComponent->EquippedWeapon)
    {
       BlasterPlayerController->SetHUDWeaponAmmo(CombatComponent->EquippedWeapon->GetAmmo());
       BlasterPlayerController->SetHUDCarriedAmmo(CombatComponent->CarriedAmmo);
    }
}

void ABlasterCharacter::UpdateHUDGrenade()
{
    BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
    if (BlasterPlayerController && CombatComponent)
    {
        BlasterPlayerController->SetHUDGrenades(CombatComponent->GetGrenades());
    }
}

void ABlasterCharacter::SpawnDefaultWeapon() const
{
    ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
    UWorld* World = GetWorld();
    if (BlasterGameMode && World && !bEliminated && DefaultWeaponClass)
    {
       FActorSpawnParameters SpawnParams;
       SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
       ABlasterWeapon* DefaultWeapon = World->SpawnActor<ABlasterWeapon>(DefaultWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
       DefaultWeapon->bDestroyWeapon = true;
       if (CombatComponent)
       {
          CombatComponent->EquipWeapon(DefaultWeapon);
       }
    }
}

// 服务器离开游戏（调用游戏模式处理）
void ABlasterCharacter::ServerLeaveGame_Implementation()
{
    ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
    BlasterPlayerState = BlasterPlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;
    if (BlasterGameMode && BlasterPlayerState)
    {
        BlasterGameMode->PlayerLeftGame(BlasterPlayerState);
    }
}

// 玩家状态初始化完成时调用
void ABlasterCharacter::OnPlayerStateInitialized()
{
    BlasterPlayerState->AddToScore(0.f);
    BlasterPlayerState->AddToDefeats(0);
    SetTeamColor(BlasterPlayerState->GetTeam());
    SetSpawnPoint();
}

// 根据团队设置出生点
void ABlasterCharacter::SetSpawnPoint()
{
    if (HasAuthority() && BlasterPlayerState->GetTeam() != ETeam::ET_NoTeam)
    {
        TArray<AActor*> PlayerStarts;
        UGameplayStatics::GetAllActorsOfClass(this, ATeamPlayerStart::StaticClass(), PlayerStarts);
        TArray<ATeamPlayerStart*> TeamPlayerStarts;
        for (auto Start : PlayerStarts)
        {
            ATeamPlayerStart* TeamStart = Cast<ATeamPlayerStart>(Start);
            if (TeamStart && TeamStart->Team == BlasterPlayerState->GetTeam())
            {
                TeamPlayerStarts.Add(TeamStart);
            }
        }
        if (TeamPlayerStarts.Num() > 0)
        {
            ATeamPlayerStart* ChosenPlayerStart = TeamPlayerStarts[FMath::RandRange(0, TeamPlayerStarts.Num() - 1)];
            SetActorLocationAndRotation(ChosenPlayerStart->GetActorLocation(), ChosenPlayerStart->GetActorRotation());
        }
    }
}

// 轮询获取PlayerState和PlayerController
void ABlasterCharacter::PollInit()
{
    if (BlasterPlayerState == nullptr)
    {
        BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
        if (BlasterPlayerState)
        {
            OnPlayerStateInitialized();

            ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
            if (BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(BlasterPlayerState))
            {
                MulticastGainedTheLead();
            }
        }
    }
    if (BlasterPlayerController == nullptr)
    {
       BlasterPlayerController = Cast<ABlasterPlayerController>(Controller);
       if (BlasterPlayerController)
       {
          UpdateHUDHealth();
          UpdateHUDShield();
          UpdateHUDAmmo();
          UpdateHUDGrenade();
       }
    }
}

// 每帧旋转（处理瞄准偏移等）
void ABlasterCharacter::RotateInPlace(float DeltaTime)
{
    // 扛旗时特殊处理：不旋转根骨骼
    if (CombatComponent && CombatComponent->bHoldingTheFlag)
    {
        bUseControllerRotationYaw = false;
        GetCharacterMovement()->bOrientRotationToMovement = true;
        TurningInPlace = ETurningInPlace::ETIP_NotTurning;
        return;
    }
    if (CombatComponent && CombatComponent->GetEquippedWeapon()) GetCharacterMovement()->bOrientRotationToMovement = false;
    if (CombatComponent && CombatComponent->GetEquippedWeapon()) bUseControllerRotationYaw = true;

    if (bDisableGameplay)
    {
       bUseControllerRotationYaw = false;
       TurningInPlace = ETurningInPlace::ETIP_NotTurning;
       return;
    }
    if (GetLocalRole() > ROLE_SimulatedProxy && IsLocallyControlled())
    {
       AimOffset(DeltaTime);
    }
    else
    {
       TimeSinceLastMovementReplication += DeltaTime;
       if (TimeSinceLastMovementReplication > 0.25f)
       {
          OnRep_ReplicatedMovement();
       }
       CalculateAO_Pitch();
    }
}

void ABlasterCharacter::ElimTimerFinished()
{
    BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
    if (BlasterGameMode && !bLeftGame)
    {
       BlasterGameMode->RequestRespawn(this, BlasterPlayerController);
    }
    // 如果玩家主动离开，广播事件
    if (bLeftGame && IsLocallyControlled())
    {
        OnLeftGame.Broadcast();
    }
}

void ABlasterCharacter::DropOrDestroyWeapon(ABlasterWeapon* Weapon)
{
    if (Weapon)
    {
       if (Weapon->bDestroyWeapon)
       {
          Weapon->Destroy();
       }
       else
       {
          Weapon->Dropped();
       }
    }
}

void ABlasterCharacter::DropOrDestroyWeapons()
{
    if (CombatComponent)
    {
       if (CombatComponent->EquippedWeapon)
       {
          DropOrDestroyWeapon(CombatComponent->EquippedWeapon);
       }
       if (CombatComponent->SecondaryWeapon)
       {
          DropOrDestroyWeapon(CombatComponent->SecondaryWeapon);
       }
       // 丢弃旗帜
       if (CombatComponent->TheFlag)
       {
          CombatComponent->TheFlag->Dropped();
       }
    }
}

void ABlasterCharacter::Destroyed()
{
    Super::Destroyed();
    if (ElimBotComponent)
    {
       ElimBotComponent->DestroyComponent();
    }
    
    ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
    bool bMatchNotInProgress = BlasterGameMode && BlasterGameMode->GetMatchState() != MatchState::InProgress;
    
    if (CombatComponent && CombatComponent->EquippedWeapon && bMatchNotInProgress)
    {
       CombatComponent->EquippedWeapon->Destroy();
    }
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
    if (DynamicDissolveMaterialInstance)
    {
       DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
    }
}

void ABlasterCharacter::StartDissolve()
{
    DissolveTrack.BindDynamic(this, &ABlasterCharacter::UpdateDissolveMaterial);
    if (DissolveCurve && DissolveTimeline)
    {
       DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
       DissolveTimeline->Play();
    }
}

void ABlasterCharacter::SetOverlappingWeapon(ABlasterWeapon* BlasterWeapon)
{
    if (OverlappingWeapon)
    {
       OverlappingWeapon->ShowPickupWidget(false);
    }
    
    OverlappingWeapon = BlasterWeapon;
    if (IsLocallyControlled())
    {
       if (OverlappingWeapon)
       {
          OverlappingWeapon->ShowPickupWidget(true);
       }
    }
}

bool ABlasterCharacter::IsWeaponEquipped() const
{
    return CombatComponent && CombatComponent->GetEquippedWeapon() != nullptr;
}

bool ABlasterCharacter::IsAiming() const
{
    return CombatComponent && CombatComponent->GetAiming();
}

FVector ABlasterCharacter::GetHitTarget() const
{
    if (CombatComponent)
    {
       return CombatComponent->HitTarget;
    }
    return FVector();
}

ECombatState ABlasterCharacter::GetCombatState() const
{
    if (CombatComponent == nullptr) return ECombatState::ECS_MAX;
    return CombatComponent->CombatState;
}

bool ABlasterCharacter::IsLocallyReloading() const
{
    if (CombatComponent == nullptr) return false;
    return CombatComponent->bLocallyReloading;
}

ABlasterWeapon* ABlasterCharacter::GetEquippedWeapon() const
{
    if (CombatComponent)
    {
       return CombatComponent->GetEquippedWeapon();
    }
    return nullptr;
}

void ABlasterCharacter::CalculateAO_Pitch()
{
    AO_Pitch = GetBaseAimRotation().Pitch;
    // 对非本地控制者映射 pitch 范围（服务器/模拟代理）
    if (AO_Pitch > 90.f && !IsLocallyControlled())
    {
       FVector2D InRange(270.f, 360.f);
       FVector2D OutRange(-90, 0.f);
       AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
    }
}

bool ABlasterCharacter::IsHoldingTheFlag() const
{
    if (CombatComponent == nullptr) return false;
    return CombatComponent->bHoldingTheFlag;
}

void ABlasterCharacter::SetHoldingTheFlag(bool bHolding) const
{
    if (CombatComponent == nullptr) return;
    CombatComponent->bHoldingTheFlag = bHolding;
}

ETeam ABlasterCharacter::GetTeam()
{
    BlasterPlayerState = BlasterPlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;
    if (BlasterPlayerState == nullptr) return ETeam::ET_NoTeam;
    return BlasterPlayerState->GetTeam();
}