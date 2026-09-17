#include "Weapons/PBLWeapon.h"

#include "Camera/CameraComponent.h"
#include "Character/PBLCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Player/PBLHUD.h"
#include "Weapons/PBLBulletDamage.h"
#include "Weapons/PBLEjectedCase.h"
#include "Ballistics/PBLBallistics.h"
#include "Ballistics/PBLCycle.h"
#include "Ballistics/PBLRecoil.h"
#include "Ballistics/PBLRecoilSettings.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "Targets/PBLTargetDummy.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

APBLWeapon::APBLWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;   // включается на время работы пружины отдачи
	bReplicates = true;
	SetReplicatingMovement(false);   // положение задаёт привязка к камере владельца, не сеть

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);            // вид от первого лица тень не отбрасывает
	Mesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);   // свой FOV (ViewmodelFieldOfView персонажа)
	Mesh->bOnlyOwnerSee = true;            // чужим игрокам покажем оружие в руках на теле (Э5+)

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	MuzzleFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(Mesh);
	if (SphereMesh.Succeeded()) { MuzzleFlash->SetStaticMesh(SphereMesh.Object); }
	MuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MuzzleFlash->SetCastShadow(false);
	MuzzleFlash->SetVisibility(false);
	MuzzleFlash->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);

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
	LoadWeaponData();
	// Оружие спавнится в PossessedBy до BeginPlay мира - при привязке меша ещё не было; масштаб ставим и здесь.
	ApplyRealSize();
	PlayIdle();
	if (bLogBones && Mesh->GetSkeletalMeshAsset())
	{
		for (int32 i = 0; i < Mesh->GetNumBones(); ++i)
		{
			const FVector L = Mesh->GetComponentTransform().InverseTransformPosition(Mesh->GetBoneLocation(Mesh->GetBoneName(i)));
			UE_LOG(LogTemp, Display, TEXT("PBL Weapon bone %2d %-28s local (%.1f, %.1f, %.1f)"), i, *Mesh->GetBoneName(i).ToString(), L.X, L.Y, L.Z);
		}
		for (const TSoftObjectPtr<UAnimSequence>* A : { &IdleAnim, &FireAnim, &ReloadAnim, &ReloadEmptyAnim })
		{
			if (UAnimSequence* S = A->LoadSynchronous()) { UE_LOG(LogTemp, Display, TEXT("PBL Weapon anim %s: %.2f s"), *S->GetName(), S->GetPlayLength()); }
		}
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

void APBLWeapon::LoadWeaponData()
{
	bHasData = false;
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UPBLWeaponDataSubsystem* Data = GI ? GI->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	const FPBLFirearmData* F = Data ? Data->FindFirearm(FirearmName) : nullptr;
	const FName CartName = !Tuning.Cartridge.IsNone() ? Tuning.Cartridge : (CartridgeName.IsNone() ? F->Cartridge : CartridgeName);
	const FPBLCartridgeData* C = F ? Data->FindCartridge(CartName) : nullptr;
	if (!F || !C)
	{
		UE_LOG(LogTemp, Warning, TEXT("PBL Weapon: нет данных для '%s' в Content/Data - hitscan-заглушка"), *FirearmName.ToString());
		return;
	}
	Firearm = *F;
	Cartridge = *C;
	bHasData = true;
	// Испытательные переопределения (окно F2): скорость через V0 при опорном стволе, масса, BC, темп, рассеивание, ноль, прицел, атмосфера.
	if (Tuning.V0_mps > 0.0f) { Cartridge.V0_mps = Tuning.V0_mps - (Firearm.Barrel_m - Cartridge.RefBarrel_m) / 0.025f * Cartridge.dV_per_25mm_mps; }
	if (Tuning.BulletMass_g > 0.0f) { Cartridge.BulletMass_kg = Tuning.BulletMass_g / 1000.0f; }
	if (Tuning.BC > 0.0f) { Cartridge.BC = Tuning.BC; }
	if (Tuning.RPM > 0.0f) { Firearm.RPM = FMath::RoundToInt(Tuning.RPM); }
	if (Tuning.Dispersion_MOA >= 0.0f) { Firearm.Dispersion_MOA = Tuning.Dispersion_MOA; }
	if (Tuning.ZeroRange_m > 0.0f) { Firearm.ZeroRange_m = Tuning.ZeroRange_m; }
	if (Tuning.SightHeight_mm > 0.0f) { Firearm.SightHeight_m = Tuning.SightHeight_mm / 1000.0f; }
	Atmosphere = FPBLAtmosphere();
	Atmosphere.Temperature_C = Tuning.Temperature_C;
	Atmosphere.Pressure_hPa = Tuning.Pressure_hPa;
	MagSize = Firearm.MagSize;
	if (Firearm.RPM > 0) { FireInterval = 60.0f / Firearm.RPM; }
	// Рассеивание: MOA - полный угол группы; конус - полуугол. 1 MOA = 1/60 град.
	SpreadDeg = Firearm.Dispersion_MOA / 60.0f * 0.5f;
	const float V0 = PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m);
	ZeroAngle_rad = PBLBallistics::SolveZeroAngle(Cartridge, Atmosphere, V0, Firearm.SightHeight_m, Firearm.ZeroRange_m);
	RecoilInfo = PBLRecoil::Compute(Cartridge, Firearm, V0, UPBLRecoilSettings::Get().Hold(Firearm.Hold));
	UE_LOG(LogTemp, Display, TEXT("PBL Weapon: recoil impulse %.2f N*s, free %.2f m/s / %.2f J, muzzle rise peak %.2f deg at %.0f ms, residual %.2f deg"),
		RecoilInfo.Impulse_Ns, RecoilInfo.FreeVelocity_mps, RecoilInfo.FreeEnergy_J, FMath::RadiansToDegrees(RecoilInfo.PeakAngle_rad), RecoilInfo.TimeToPeak_s * 1000.0f,
		FMath::RadiansToDegrees(RecoilInfo.PeakAngle_rad * UPBLRecoilSettings::Get().Hold(Firearm.Hold).ResidualFraction));
	UE_LOG(LogTemp, Display, TEXT("PBL Weapon: %s / %s: V0 %.1f m/s, mag %d, interval %.3f s, spread %.2f deg, zero %.0f m -> angle %.2f MOA"),
		*Firearm.Name.ToString(), *Cartridge.Name.ToString(), V0, MagSize, FireInterval, SpreadDeg, Firearm.ZeroRange_m, FMath::RadiansToDegrees(ZeroAngle_rad) * 60.0f);
}

void APBLWeapon::ComputeLaunch(const FVector& LOSOrigin, const FVector& LOSDir, FVector& OutOrigin, FVector& OutDir) const
{
	// Дуло на SightHeight ниже линии прицеливания, ствол приподнят на угол нуля вокруг оси "вправо".
	const FVector Up = FVector::UpVector;
	const FVector Right = FVector::CrossProduct(LOSDir, Up).GetSafeNormal();
	const FVector LocalUp = FVector::CrossProduct(Right, LOSDir).GetSafeNormal();
	// Пуля рождается у среза ствола: на дистанции дула вдоль линии прицеливания (боковой сдвиг viewmodel не физический - ствол под линией глаз).
	float MuzzleAhead_cm = 0.0f;
	if (Mesh) { MuzzleAhead_cm = FMath::Max(0.0f, FVector::DotProduct(GetMuzzleLocation() - LOSOrigin, LOSDir)); }
	OutOrigin = LOSOrigin + LOSDir * MuzzleAhead_cm - LocalUp * (Firearm.SightHeight_m * 100.0f);
	OutDir = (LOSDir * FMath::Cos(ZeroAngle_rad) + LocalUp * FMath::Sin(ZeroAngle_rad)).GetSafeNormal();
}

void APBLWeapon::LaunchProjectile(const FVector& Origin, const FVector& Velocity, const FVector& LOSOrigin, const FVector& LOSDir, bool bAuthoritative)
{
	UClass* Cls = ProjectileClass.LoadSynchronous();
	if (!Cls) { Cls = APBLProjectile::StaticClass(); }
	FActorSpawnParameters P;
	P.Owner = this;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APBLProjectile* Proj = GetWorld()->SpawnActor<APBLProjectile>(Cls, Origin, Velocity.Rotation(), P))
	{
		Proj->Launch(this, Cartridge, Origin, Velocity, LOSOrigin, LOSDir, bAuthoritative, Damage, Atmosphere);
	}
}

FVector APBLWeapon::GetMuzzleLocation() const
{
	if (!MuzzleSocket.IsNone() && Mesh->DoesSocketExist(MuzzleSocket)) { return Mesh->GetSocketLocation(MuzzleSocket); }
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
		ApplyRealSize();
	}
}

void APBLWeapon::ApplyTuning(const FPBLWeaponTuning& NewTuning)
{
	Tuning = NewTuning;
	LoadWeaponData();
	if (bHasData && Firearm.RPM > 0) { FireInterval = 60.0f / Firearm.RPM; }
	if (Tuning.TrailLifetime_s >= 0.0f) { TrajectoryTrailLifetime = Tuning.TrailLifetime_s; }
	UE_LOG(LogTemp, Display, TEXT("PBL Weapon: tuning applied (cartridge %s, V0 %.1f m/s, mass %.2f g, BC %.3f, MOA %.1f, zero %.0f m, recoil x%.2f)"),
		*Cartridge.Name.ToString(), GetMuzzleVelocity(), Cartridge.BulletMass_kg * 1000.0f, Cartridge.BC, Firearm.Dispersion_MOA, Firearm.ZeroRange_m, Tuning.RecoilScale);
}

float APBLWeapon::GetMuzzleVelocity() const
{
	return bHasData ? PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m) : 0.0f;
}

void APBLWeapon::ApplyRealSize()
{
	// Меш под реальную габаритную длину образца: любая модель (заглушка Lyra 24 см, Glock с Fab 9.6 см) становится 20.2 см.
	if (bScaleMeshToLength && bHasData && Firearm.OverallLength_m > 0.0f && Mesh && Mesh->GetSkeletalMeshAsset())
	{
		const FVector Ext = Mesh->GetSkeletalMeshAsset()->GetBounds().BoxExtent;
		const float MeshLen_cm = 2.0f * FMath::Max(Ext.X, Ext.Y);
		if (MeshLen_cm > 1.0f) { SetActorScale3D(FVector(Firearm.OverallLength_m * 100.0f / MeshLen_cm)); }
		UE_LOG(LogTemp, Display, TEXT("PBL Weapon: mesh %.1f cm -> scale %.3f"), MeshLen_cm, Firearm.OverallLength_m * 100.0f / MeshLen_cm);
	}
	if (OwnerCharacter && OwnerCharacter->GetFirstPersonCamera() && Mesh && Mesh->GetSkeletalMeshAsset())
	{
		const FTransform Cam = OwnerCharacter->GetFirstPersonCamera()->GetComponentTransform();
		const FVector MuzzleRel = Cam.InverseTransformPosition(GetMuzzleLocation());
		UE_LOG(LogTemp, Display, TEXT("PBL Weapon: muzzle rel. camera fwd %.1f right %.1f up %.1f cm"), MuzzleRel.X, MuzzleRel.Y, MuzzleRel.Z);
	}
}

// ---------------- Отдача из физики (E10.5) ----------------

void APBLWeapon::ApplyPhysicsRecoil()
{
	FPBLRecoilInfo Scaled = RecoilInfo;
	Scaled.Omega0_rad_s *= Tuning.RecoilScale; Scaled.PeakAngle_rad *= Tuning.RecoilScale; Scaled.FreeVelocity_mps *= Tuning.RecoilScale;
	PBLRecoil::Kick(Recoil, Scaled, UPBLRecoilSettings::Get().Hold(Firearm.Hold), FMath::FRandRange(-1.0f, 1.0f));
	SetActorTickEnabled(true);
}

void APBLWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!OwnerCharacter || !bHasData) { SetActorTickEnabled(false); return; }
	const UPBLRecoilSettings& RS = UPBLRecoilSettings::Get();
	float DPitch = 0.0f, DYaw = 0.0f;
	PBLRecoil::Step(Recoil, RecoilInfo, RS.Hold(Firearm.Hold), DeltaSeconds, DPitch, DYaw);
	// Линия прицеливания следует углу хвата: приращение уходит в управляющий поворот, мышь игрока складывается с ним.
	OwnerCharacter->ApplyRecoil(FMath::RadiansToDegrees(DPitch), FMath::RadiansToDegrees(DYaw));
	// Прицел: альфа позы идёт к цели за AimTime.
	const float Target = bAiming ? 1.0f : 0.0f;
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, Target, DeltaSeconds, 1.0f / FMath::Max(AimTime, 0.01f));
	UpdateViewTransform();
	if (!Recoil.IsActive() && FMath::IsNearlyEqual(AimAlpha, Target)) { SetActorTickEnabled(false); }
}

void APBLWeapon::UpdateViewTransform()
{
	const UPBLRecoilSettings& RS = UPBLRecoilSettings::Get();
	// Поза = бедро + (прицел - бедро)·alpha (плавная кривая), плюс визуальная отдача: откат назад и задир сильнее камеры.
	const float A = FMath::SmoothStep(0.0f, 1.0f, AimAlpha);
	// Довороты (прицел, визуальная отдача) - вокруг глаза (точки крепления к камере), а не вокруг корня рига,
	// который у рига рук лежит на 1.5 м ниже: иначе 3° превращаются в 8 см сдвига.
	const FRotator Delta = AimRotation * A + FRotator(FMath::RadiansToDegrees(Recoil.Pitch - Recoil.PitchRest) * RS.VisualPitchScale, 0.0f, 0.0f);
	const FQuat Q = Delta.Quaternion();
	const FVector BaseLoc = ViewOffset + AimOffset * A - FVector(Recoil.VisualKick_cm, 0.0f, 0.0f);
	SetActorRelativeLocation(Q.RotateVector(BaseLoc));
	SetActorRelativeRotation((Q * ViewRotation.Quaternion()).Rotator());
}

void APBLWeapon::SetAiming(bool bInAiming)
{
	if (bAiming == bInAiming) { return; }
	bAiming = bInAiming;
	SetActorTickEnabled(true);
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

	// Линия прицеливания - из камеры вдоль взгляда (центр экрана). Разброс - на клиенте, направление
	// уходит серверу как есть: тогда локальная косметическая пуля и серверная летят одинаково.
	// Направление - из управляющего поворота, а не из компонента камеры: тот обновляется на кадр позже
	// (bUsePawnControlRotation), и выстрел сразу после поворота ушёл бы по старому взгляду.
	const UCameraComponent* Cam = OwnerCharacter->GetFirstPersonCamera();
	const FVector LOSOrigin = Cam->GetComponentLocation();
	const FVector AimDir = OwnerCharacter->GetController() ? OwnerCharacter->GetController()->GetControlRotation().Vector() : Cam->GetForwardVector();
	const FVector LOSDir = ApplySpread(AimDir);
	// От бедра (прицел не выведен): пуля идёт из дула вдоль оси ствола viewmodel - без крестика, «куда смотрит ствол».
	const bool bAimedShot = AimAlpha > 0.5f;
	const FVector HipOrigin = GetMuzzleLocation();
	const FVector HipDir = ApplySpread(Mesh->GetComponentTransform().TransformVectorNoScale(BoreAxisLocal).GetSafeNormal());

	// Мгновенная локальная реакция (как в CS), сервер догонит.
	PlayLocalFireFX();
	PlayMuzzleFX();
	if (bHasData && UPBLRecoilSettings::Get().bPhysicsRecoil) { ApplyPhysicsRecoil(); }
	else { OwnerCharacter->ApplyRecoil(RecoilPitch, FMath::RandRange(-RecoilYawRandom, RecoilYawRandom)); }
	if (!HasAuthority())
	{
		AmmoInMag = FMath::Max(AmmoInMag - 1, 0);  // предсказание для HUD
		FVector O, D;
		if (bAimedShot) { ComputeLaunch(LOSOrigin, LOSDir, O, D); } else { O = HipOrigin; D = HipDir; }
		if (bHasData) { LaunchProjectile(O, D * PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m), LOSOrigin, LOSDir, false); }
	}

	if (bAimedShot) { Server_Fire(LOSOrigin, LOSDir, true); } else { Server_Fire(HipOrigin, HipDir, false); }
}

bool APBLWeapon::Server_Fire_Validate(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir, bool bAimed)
{
	// Грубая защита: исходная точка не дальше 3 м от камеры владельца.
	if (!OwnerCharacter || !OwnerCharacter->GetFirstPersonCamera()) { return false; }
	return FVector::DistSquared(Origin, OwnerCharacter->GetFirstPersonCamera()->GetComponentLocation()) < FMath::Square(300.0f);
}

void APBLWeapon::Server_Fire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir, bool bAimed)
{
	if (bReloading || AmmoInMag <= 0) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - ServerLastFireTime < FireInterval * 0.8f) { return; }   // допуск на джиттер
	ServerLastFireTime = Now;
	AmmoInMag--;

	const FVector LOSOrigin = Origin;
	const FVector LOSDir = FVector(Dir).GetSafeNormal();
	Multicast_ShotFX(LOSOrigin, LOSDir, bAimed);

	if (!bHasData)
	{
		// Заглушка без данных: мгновенный луч (старое поведение).
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PBLWeaponFire), true);
		Params.AddIgnoredActor(this);
		if (OwnerCharacter) { Params.AddIgnoredActor(OwnerCharacter); }
		if (GetWorld()->LineTraceSingleByChannel(Hit, LOSOrigin, LOSOrigin + LOSDir * Range, ECC_Visibility, Params))
		{
			OnProjectileImpact(Hit, LOSDir * 350.0f, Damage);
		}
		return;
	}

	FVector O, D;
	if (bAimed) { ComputeLaunch(LOSOrigin, LOSDir, O, D); } else { O = LOSOrigin; D = LOSDir; }   // от бедра Origin/Dir - это дуло и ось ствола
	LaunchProjectile(O, D * PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m), LOSOrigin, LOSDir, true);
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

void APBLWeapon::Multicast_ShotFX_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir, bool bAimed)
{
	// Владелец уже сделал всё локально; сервер (listen) запускает авторитетную пулю сам.
	if (HasAuthority() || (OwnerCharacter && OwnerCharacter->IsLocallyControlled())) { return; }
	PlayMuzzleFX();
	if (bHasData)
	{
		FVector O, D;
		if (bAimed) { ComputeLaunch(Origin, FVector(Dir), O, D); } else { O = Origin; D = FVector(Dir); }
		LaunchProjectile(O, D * PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m), Origin, FVector(Dir), false);
	}
}

void APBLWeapon::PlayMuzzleFX()
{
	MuzzleFlash->SetVisibility(true);
	MuzzleFlash->SetRelativeRotation(FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f));
	MuzzleLight->SetVisibility(true);
	MuzzleLight->SetIntensity(MuzzleLightIntensity);
	GetWorldTimerManager().SetTimer(MuzzleTimer, this, &APBLWeapon::HideMuzzleFlash, MuzzleFlashTime, false);
}

void APBLWeapon::HideMuzzleFlash()
{
	MuzzleFlash->SetVisibility(false);
	MuzzleLight->SetVisibility(false);
	MuzzleLight->SetIntensity(0.0f);
}

void APBLWeapon::SpawnTracer(const FVector& From, const FVector& To)
{
	const FVector Delta = To - From;
	if (Delta.Size() < 5.0f) { return; }
	if (Delta.Size() >= 50.0f) { SpawnSegment(From, To, TracerThickness, TracerMaterial.LoadSynchronous(), TracerLifetime, NAME_None); }
	if (TrajectoryTrailLifetime > 0.0f)
	{
		UMaterialInterface* TM = TrailMaterial.IsNull() ? TracerMaterial.LoadSynchronous() : TrailMaterial.LoadSynchronous();
		SpawnSegment(From, To, TrajectoryTrailThickness, TM, TrajectoryTrailLifetime, TEXT("PBLTrail"));
	}
}

AActor* APBLWeapon::SpawnSegment(const FVector& From, const FVector& To, float Thickness_cm, UMaterialInterface* M, float Lifetime, FName Tag)
{
	// Не кэшировать в static: сырой указатель не держит объект, после GC он битый (крэш в SpawnTracer 2026-09-16). LoadObject на уже загруженном = поиск.
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UWorld* W = GetWorld();
	if (!M || !Cyl || !W) { return nullptr; }
	const FVector Delta = To - From;
	const float Len = Delta.Size();
	// Цилиндр движка: высота 100 по Z, радиус 50. Z - вдоль выстрела.
	const FTransform T(FRotationMatrix::MakeFromZ(Delta / Len).ToQuat(), From + Delta * 0.5f,
		FVector(Thickness_cm / 100.0f, Thickness_cm / 100.0f, Len / 100.0f));
	AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), T);
	if (!A) { return nullptr; }
	if (!Tag.IsNone()) { A->Tags.Add(Tag); }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
	C->SetStaticMesh(Cyl);
	C->SetMaterial(0, M);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->RegisterComponent();
	A->SetRootComponent(C);
	C->SetWorldTransform(T);
	A->SetLifeSpan(Lifetime);
	return A;
}

static FAutoConsoleCommandWithWorld CmdClearTrails(TEXT("pbl.Range.ClearTrails"), TEXT("Remove bullet trajectory trails"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) { return; }
		for (TActorIterator<AActor> It(World); It; ++It) { if (It->ActorHasTag(TEXT("PBLTrail"))) { It->Destroy(); } }
	}));

void APBLWeapon::OnProjectileImpact(const FHitResult& Hit, const FVector& ImpactVelocity_mps, float DamageToApply, float Energy_J, bool bStopped)
{
	if (!HasAuthority()) { return; }
	const bool bCharacter = Hit.GetComponent() && Hit.GetComponent()->GetCollisionObjectType() == ECC_Pawn;
	const bool bHead = bCharacter && Hit.GetComponent()->GetName().Contains(TEXT("Head"));
	if (Hit.GetActor())
	{
		// Свой тип события: цель получает не только очки урона, но и физику удара (энергия в тканях, сквозное или нет).
		FPBLBulletDamageEvent Event;
		Event.Damage = DamageToApply;
		Event.HitInfo = Hit;
		Event.ShotDirection = ImpactVelocity_mps.GetSafeNormal();
		Event.DamageTypeClass = UDamageType::StaticClass();
		Event.Energy_J = Energy_J;
		Event.ImpactVelocity_mps = ImpactVelocity_mps.Size();
		Event.bStoppedInTarget = bStopped;
		const float Applied = Hit.GetActor()->TakeDamage(DamageToApply, Event, OwnerCharacter ? OwnerCharacter->GetController() : nullptr, this);
		if (bCharacter && Applied > 0.0f)
		{
			bool bKill = false;
			if (const APBLTargetDummy* D = Cast<APBLTargetDummy>(Hit.GetActor())) { bKill = !D->IsAlive(); }
			Client_HitConfirmed(bHead, bKill);
		}
	}
	Multicast_HitFX(Hit.ImpactPoint, Hit.ImpactNormal, bCharacter);
}

void APBLWeapon::OnProjectileFinished(const FPBLShotReport& Report)
{
	if (!HasAuthority()) { return; }
	UE_LOG(LogTemp, Display, TEXT("PBL shot: V0 %.1f  dist %.2f m  Vimp %.1f  E %.0f J  t %.3f s  drop %+.1f cm  %s"),
		Report.V0_mps, Report.Distance_m, Report.ImpactVelocity_mps, Report.ImpactEnergy_J, Report.TimeOfFlight_s, Report.DropFromLOS_m * 100.0f,
		Report.bHit ? (Report.bHitTarget ? TEXT("TARGET") : *FString::Printf(TEXT("hit %s"), *Report.HitActor.ToString())) : TEXT("no hit"));
	if (!Report.PenetrationMaterial.IsNone())
	{
		UE_LOG(LogTemp, Display, TEXT("PBL shot: %s penetration %.1f cm (%.1f in), V_in %.1f, %s, final dia %.1f mm"),
			*Report.PenetrationMaterial.ToString(), Report.Penetration_m * 100.0f, Report.Penetration_m / 0.0254f, Report.MediumEntryVelocity_mps,
			Report.bStoppedInMedium ? TEXT("STOPPED") : *FString::Printf(TEXT("EXIT at %.1f m/s"), Report.MediumExitVelocity_mps), Report.FinalDiameter_mm);
	}
	if (Report.Ricochets > 0) { UE_LOG(LogTemp, Display, TEXT("PBL shot: %d ricochet(s), %d layer(s)"), Report.Ricochets, Report.Layers); }
	if (Report.WoundDamage > 0.0f) { UE_LOG(LogTemp, Display, TEXT("PBL shot: wound damage %.1f (E %.0f J in tissue)"), Report.WoundDamage, Report.WoundEnergy_J); }
	if (bLogShotsCsv) { AppendShotCsv(Report); }
	Client_ShotReport(Report);
}

void APBLWeapon::Client_ShotReport_Implementation(FPBLShotReport Report)
{
	LastReport = Report;
	LastReportTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void APBLWeapon::AppendShotCsv(const FPBLShotReport& R) const
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Ballistics");
	const FString Path = Dir / TEXT("shots.csv");
	IFileManager::Get().MakeDirectory(*Dir, true);
	if (!IFileManager::Get().FileExists(*Path))
	{
		FFileHelper::SaveStringToFile(TEXT("time,firearm,cartridge,v0_mps,distance_m,vimp_mps,energy_J,tof_s,drop_cm,hit,target,medium,pen_cm,v_exit_mps,dia_mm,stopped,layers,ricochets\n"), *Path);
	}
	const FString Line = FString::Printf(TEXT("%s,%s,%s,%.1f,%.2f,%.1f,%.0f,%.4f,%.1f,%d,%d,%s,%.1f,%.1f,%.1f,%d,%d,%d\n"),
		*FDateTime::Now().ToIso8601(), *Firearm.Name.ToString(), *Cartridge.Name.ToString(), R.V0_mps, R.Distance_m, R.ImpactVelocity_mps,
		R.ImpactEnergy_J, R.TimeOfFlight_s, R.DropFromLOS_m * 100.0f, R.bHit ? 1 : 0, R.bHitTarget ? 1 : 0,
		*R.PenetrationMaterial.ToString(), R.Penetration_m * 100.0f, R.MediumExitVelocity_mps, R.FinalDiameter_mm, R.bStoppedInMedium ? 1 : 0, R.Layers, R.Ricochets);
	FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
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

void APBLWeapon::EjectCase()
{
	// Гильзу уносит назад вместе с затвором, поэтому её скорость - это скорость затвора на том ходе,
	// где патрон уже вышел из патронника. Ход считается из импульса выстрела, как и отдача.
	if (!bHasData || !Firearm.IsRecoilOperated()) { return; }
	if (!CaseMesh) { CaseMesh = APBLEjectedCase::CaseMeshFor(Firearm.Cartridge); }
	if (!CaseMesh) { return; }
	const float V0 = PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m);
	const float Impulse = Cartridge.BulletMass_kg * V0 + Cartridge.PowderMass_kg * Cartridge.GasVelocity_mps;
	const float Speed_mps = PBLCycle::VelocityAtTravel(Firearm, Impulse, Cartridge.CaseLength_m + 0.003f);
	if (Speed_mps <= 0.0f) { return; }

	// Окно выброса задано от дульного среза вдоль оси канала, поэтому не зависит от того, какой
	// именно моделью оружия мы играем: для стрельбы и для оружейки они разные.
	const FTransform& T = Mesh->GetComponentTransform();
	const FVector Fwd = T.TransformVectorNoScale(BoreAxisLocal).GetSafeNormal();
	FVector Right = FVector::CrossProduct(T.GetUnitAxis(EAxis::Z), Fwd).GetSafeNormal();
	if (Right.IsNearlyZero()) { Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal(); }
	const FVector Up = FVector::CrossProduct(Fwd, Right).GetSafeNormal();
	const FVector Port = GetMuzzleLocation()
		+ (Fwd * Firearm.EjectPort_m.X + Right * Firearm.EjectPort_m.Y + Up * Firearm.EjectPort_m.Z) * 100.0f;

	const float R = FMath::DegreesToRadians(Firearm.EjectRight_deg);
	const float U = FMath::DegreesToRadians(Firearm.EjectUp_deg);
	const FVector Dir = -Fwd * (FMath::Cos(R) * FMath::Cos(U)) + Right * (FMath::Sin(R) * FMath::Cos(U)) + Up * FMath::Sin(U);
	// Гильза наследует движение стрелка: он для неё - система отсчёта.
	const FVector Carry = OwnerCharacter ? OwnerCharacter->GetVelocity() : FVector::ZeroVector;
	APBLEjectedCase::Eject(GetWorld(), FTransform(Fwd.Rotation(), Port), Dir * Speed_mps * 100.0f + Carry,
		Up * (Firearm.EjectSpin_rps * 360.0f), Cartridge.CaseMass_kg, CaseMesh);
}

void APBLWeapon::PlayLocalFireFX()
{
	EjectCase();
	if (UAnimSequence* A = FireAnim.LoadSynchronous())
	{
		Mesh->PlayAnimation(A, false);
		GetWorldTimerManager().SetTimer(IdleTimer, this, &APBLWeapon::PlayIdle, FMath::Max(A->GetPlayLength(), 0.05f), false);
	}
	USoundBase* S = FireSounds.Num() > 0 ? FireSounds[FMath::RandRange(0, FireSounds.Num() - 1)].LoadSynchronous() : FireSound.LoadSynchronous();
	if (S)
	{
		const float Vol = 1.0f + FMath::FRandRange(-FireSoundVolumeVariation, FireSoundVolumeVariation);
		const float Pitch = 1.0f + FMath::FRandRange(-FireSoundPitchVariation, FireSoundPitchVariation);
		UGameplayStatics::PlaySoundAtLocation(this, S, GetMuzzleLocation(), Vol, Pitch, 0.0f, FireAttenuation.LoadSynchronous());
	}
}

void APBLWeapon::PlayIdle()
{
	if (UAnimSequence* A = IdleAnim.LoadSynchronous()) { Mesh->PlayAnimation(A, true); }
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
	UAnimSequence* A = (AmmoInMag <= 0 && !ReloadEmptyAnim.IsNull()) ? ReloadEmptyAnim.LoadSynchronous() : ReloadAnim.LoadSynchronous();
	if (A)
	{
		Mesh->PlayAnimation(A, false);
		GetWorldTimerManager().SetTimer(IdleTimer, this, &APBLWeapon::PlayIdle, FMath::Max(A->GetPlayLength(), ReloadTime), false);
	}
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
