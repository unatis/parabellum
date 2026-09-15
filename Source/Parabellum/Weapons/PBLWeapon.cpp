#include "Weapons/PBLWeapon.h"

#include "Camera/CameraComponent.h"
#include "Character/PBLCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Player/PBLHUD.h"
#include "Targets/PBLTargetDummy.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

APBLWeapon::APBLWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);   // положение задаёт привязка к камере владельца, не сеть

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);            // вид от первого лица тень не отбрасывает
	Mesh->bOnlyOwnerSee = true;            // чужим игрокам покажем оружие в руках на теле (Э5+)

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	MuzzleFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(Mesh);
	if (SphereMesh.Succeeded()) { MuzzleFlash->SetStaticMesh(SphereMesh.Object); }
	MuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MuzzleFlash->SetCastShadow(false);
	MuzzleFlash->SetVisibility(false);

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Mesh);
	MuzzleLight->SetIntensity(0.0f);
	MuzzleLight->SetLightColor(FLinearColor(1.0f, 0.75f, 0.4f));
	MuzzleLight->SetAttenuationRadius(400.0f);
	MuzzleLight->SetCastShadows(false);
	MuzzleLight->SetVisibility(false);
}

void APBLWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(APBLWeapon, AmmoInMag, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APBLWeapon, bReloading, COND_OwnerOnly);
}

void APBLWeapon::BeginPlay()
{
	Super::BeginPlay();
	if (USkeletalMesh* SM = WeaponMesh.LoadSynchronous())
	{
		Mesh->SetSkeletalMeshAsset(SM);
	}
	if (HasAuthority())
	{
		AmmoInMag = MagSize;
	}
	MuzzleFlash->SetRelativeLocation(MuzzleOffset);
	MuzzleFlash->SetRelativeScale3D(FVector(MuzzleFlashSize / 100.0f));
	if (UMaterialInterface* M = MuzzleFlashMaterial.LoadSynchronous()) { MuzzleFlash->SetMaterial(0, M); }
	MuzzleLight->SetRelativeLocation(MuzzleOffset + FVector(0.0f, 5.0f, 0.0f));
}

FVector APBLWeapon::GetMuzzleLocation() const
{
	return Mesh->GetComponentTransform().TransformPosition(MuzzleOffset);
}

void APBLWeapon::AttachToOwnerCamera(APBLCharacter* NewOwner)
{
	OwnerCharacter = NewOwner;
	SetOwner(NewOwner);
	if (NewOwner && NewOwner->GetFirstPersonCamera())
	{
		AttachToComponent(NewOwner->GetFirstPersonCamera(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(ViewOffset);
		SetActorRelativeRotation(ViewRotation);
	}
}

// ---------------- Стрельба ----------------

void APBLWeapon::StartFire()
{
	// Полуавтомат: одно нажатие - один выстрел. Автомат добавим полем bAutomatic, когда появится винтовка.
	FireOnce();
}

void APBLWeapon::StopFire()
{
}

void APBLWeapon::FireOnce()
{
	if (!OwnerCharacter || bReloading) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireInterval) { return; }
	LastFireTime = Now;

	if (AmmoInMag <= 0)
	{
		if (USoundBase* S = DryFireSound.LoadSynchronous()) { UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation()); }
		return;
	}

	// Луч из центра экрана. При включённой системе зрения центр не пересчитывается (плотность CS),
	// поэтому центральный луч совпадает с направлением камеры и в композите.
	const UCameraComponent* Cam = OwnerCharacter->GetFirstPersonCamera();
	const FVector Origin = Cam->GetComponentLocation();
	const FVector Dir = ApplySpread(Cam->GetForwardVector());

	// Мгновенная локальная реакция (как в CS), сервер догонит.
	PlayLocalFireFX();
	{
		FHitResult LocalHit;
		FCollisionQueryParams P(SCENE_QUERY_STAT(PBLWeaponPredict), true);
		P.AddIgnoredActor(this);
		P.AddIgnoredActor(OwnerCharacter);
		const FVector End = Origin + Dir * Range;
		const bool bLocalHit = GetWorld()->LineTraceSingleByChannel(LocalHit, Origin, End, ECC_Visibility, P);
		PlayShotFX(bLocalHit ? LocalHit.ImpactPoint : End);
	}
	OwnerCharacter->ApplyRecoil(RecoilPitch, FMath::RandRange(-RecoilYawRandom, RecoilYawRandom));
	if (!HasAuthority()) { AmmoInMag = FMath::Max(AmmoInMag - 1, 0); }  // предсказание для HUD

	Server_Fire(Origin, Dir);
}

bool APBLWeapon::Server_Fire_Validate(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir)
{
	// Грубая защита: исходная точка не дальше 3 м от камеры владельца.
	if (!OwnerCharacter || !OwnerCharacter->GetFirstPersonCamera()) { return false; }
	return FVector::DistSquared(Origin, OwnerCharacter->GetFirstPersonCamera()->GetComponentLocation()) < FMath::Square(300.0f);
}

void APBLWeapon::Server_Fire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir)
{
	if (bReloading || AmmoInMag <= 0) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - ServerLastFireTime < FireInterval * 0.8f) { return; }   // допуск на джиттер
	ServerLastFireTime = Now;
	AmmoInMag--;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PBLWeaponFire), true);
	Params.AddIgnoredActor(this);
	if (OwnerCharacter) { Params.AddIgnoredActor(OwnerCharacter); }
	const FVector End = Origin + Dir * Range;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params);
	UE_LOG(LogTemp, Display, TEXT("PBL: shot from %s dir %s -> %s actor=%s comp=%s dist=%.0f bone=%s"),
		*Origin.ToCompactString(), *Dir.ToCompactString(), bHit ? TEXT("HIT") : TEXT("miss"),
		bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("-"), bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("-"),
		bHit ? Hit.Distance : 0.0f, *Hit.BoneName.ToString());
	Multicast_ShotFX(bHit ? Hit.ImpactPoint : End);
	if (!bHit) { return; }

	// Множитель за голову считает получатель (у него хитбоксы) - оружие шлёт базовый урон.
	const bool bCharacter = Hit.GetComponent() && Hit.GetComponent()->GetCollisionObjectType() == ECC_Pawn;
	const bool bHead = bCharacter && Hit.GetComponent()->GetName().Contains(TEXT("Head"));
	if (Hit.GetActor())
	{
		const float Applied = UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Dir, Hit, OwnerCharacter ? OwnerCharacter->GetController() : nullptr, this, nullptr);
		if (bCharacter && Applied > 0.0f)
		{
			bool bKill = false;
			if (const APBLTargetDummy* D = Cast<APBLTargetDummy>(Hit.GetActor())) { bKill = !D->IsAlive(); }
			Client_HitConfirmed(bHead, bKill);
		}
	}
	Multicast_HitFX(Hit.ImpactPoint, Hit.ImpactNormal, bCharacter);
}

void APBLWeapon::Multicast_HitFX_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal, bool bHitCharacter)
{
	SpawnImpact(Location, Normal, bHitCharacter);
}

void APBLWeapon::SpawnImpact(const FVector& Location, const FVector& Normal, bool bHitCharacter)
{
	if (bHitCharacter)
	{
		if (USoundBase* S = BodyHitSound.LoadSynchronous()) { UGameplayStatics::PlaySoundAtLocation(this, S, Location); }
		return;   // на теле декали не рисуем
	}
	if (USoundBase* S = ImpactSound.LoadSynchronous()) { UGameplayStatics::PlaySoundAtLocation(this, S, Location); }
	if (UMaterialInterface* Decal = ImpactDecal.LoadSynchronous())
	{
		const FRotator Rot = Normal.Rotation();
		UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(GetWorld(), Decal, FVector(DecalSize, DecalSize, DecalSize), Location, Rot, DecalLifetime);
		if (D) { D->SetFadeScreenSize(0.001f); }
	}
}

void APBLWeapon::Multicast_ShotFX_Implementation(FVector_NetQuantize End)
{
	// Владелец уже отрисовал свой выстрел локально в FireOnce.
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) { return; }
	PlayShotFX(End);
}

void APBLWeapon::PlayShotFX(const FVector& End)
{
	MuzzleFlash->SetVisibility(true);
	MuzzleFlash->SetRelativeRotation(FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f));
	MuzzleLight->SetVisibility(true);
	MuzzleLight->SetIntensity(MuzzleLightIntensity);
	GetWorldTimerManager().SetTimer(MuzzleTimer, this, &APBLWeapon::HideMuzzleFlash, MuzzleFlashTime, false);
	SpawnTracer(GetMuzzleLocation(), End);
}

void APBLWeapon::HideMuzzleFlash()
{
	MuzzleFlash->SetVisibility(false);
	MuzzleLight->SetVisibility(false);
	MuzzleLight->SetIntensity(0.0f);
}

void APBLWeapon::SpawnTracer(const FVector& From, const FVector& To)
{
	UMaterialInterface* M = TracerMaterial.LoadSynchronous();
	static UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!M || !Cyl) { return; }
	const FVector Delta = To - From;
	const float Len = Delta.Size();
	if (Len < 50.0f) { return; }
	// Цилиндр движка: высота 100 по Z, радиус 50. Z - вдоль выстрела.
	const FTransform T(FRotationMatrix::MakeFromZ(Delta / Len).ToQuat(), From + Delta * 0.5f,
		FVector(TracerThickness / 100.0f, TracerThickness / 100.0f, Len / 100.0f));
	AActor* A = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), T);
	if (!A) { return; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
	C->SetStaticMesh(Cyl);
	C->SetMaterial(0, M);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->RegisterComponent();
	A->SetRootComponent(C);
	C->SetWorldTransform(T);
	A->SetLifeSpan(TracerLifetime);
}

void APBLWeapon::Client_HitConfirmed_Implementation(bool bHead, bool bKill)
{
	if (USoundBase* S = (bKill ? KillSound : HitMarkerSound).LoadSynchronous()) { UGameplayStatics::PlaySound2D(this, S); }
	if (OwnerCharacter)
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
		{
			if (APBLHUD* HUD = Cast<APBLHUD>(PC->GetHUD())) { HUD->ShowHitMarker(bHead, bKill); }
		}
	}
}

void APBLWeapon::PlayLocalFireFX()
{
	if (UAnimSequence* A = FireAnim.LoadSynchronous())
	{
		Mesh->PlayAnimation(A, false);
	}
	if (USoundBase* S = FireSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation());
	}
}

FVector APBLWeapon::ApplySpread(const FVector& Dir) const
{
	if (SpreadDeg <= 0.0f) { return Dir; }
	return FMath::VRandCone(Dir, FMath::DegreesToRadians(SpreadDeg));
}

// ---------------- Перезарядка ----------------

void APBLWeapon::StartReload()
{
	if (bReloading || AmmoInMag >= MagSize) { return; }
	Server_Reload();
}

void APBLWeapon::Server_Reload_Implementation()
{
	if (bReloading || AmmoInMag >= MagSize) { return; }
	bReloading = true;
	if (UAnimSequence* A = ReloadAnim.LoadSynchronous()) { Mesh->PlayAnimation(A, false); }
	if (USoundBase* S = ReloadSound.LoadSynchronous()) { UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation()); }
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &APBLWeapon::FinishReload, ReloadTime, false);
}

void APBLWeapon::FinishReload()
{
	AmmoInMag = MagSize;
	bReloading = false;
}

void APBLWeapon::OnRep_AmmoInMag()
{
	// HUD читает AmmoInMag напрямую каждый кадр - здесь пока ничего.
}
