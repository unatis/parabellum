#include "Weapons/PBLWeapon.h"

#include "Camera/CameraComponent.h"
#include "Character/PBLCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
	if (!bHit) { return; }

	// Множитель за голову считает получатель (у него хитбоксы) - оружие шлёт базовый урон.
	const bool bCharacter = Hit.GetComponent() && Hit.GetComponent()->GetCollisionObjectType() == ECC_Pawn;
	if (Hit.GetActor())
	{
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Dir, Hit, OwnerCharacter ? OwnerCharacter->GetController() : nullptr, this, nullptr);
	}
	Multicast_HitFX(Hit.ImpactPoint, Hit.ImpactNormal, bCharacter);
}

void APBLWeapon::Multicast_HitFX_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal, bool bHitCharacter)
{
	SpawnImpact(Location, Normal, bHitCharacter);
}

void APBLWeapon::SpawnImpact(const FVector& Location, const FVector& Normal, bool bHitCharacter)
{
	if (bHitCharacter) { return; }   // на теле декали не рисуем (Э5: реакция мишени)
	if (UMaterialInterface* Decal = ImpactDecal.LoadSynchronous())
	{
		const FRotator Rot = Normal.Rotation();
		UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(GetWorld(), Decal, FVector(DecalSize, DecalSize, DecalSize), Location, Rot, DecalLifetime);
		if (D) { D->SetFadeScreenSize(0.001f); }
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
