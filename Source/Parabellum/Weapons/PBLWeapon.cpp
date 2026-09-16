#include "Weapons/PBLWeapon.h"

#include "Camera/CameraComponent.h"
#include "Character/PBLCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Player/PBLHUD.h"
#include "Ballistics/PBLBallistics.h"
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
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
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
	const FPBLCartridgeData* C = F ? Data->FindCartridge(CartridgeName.IsNone() ? F->Cartridge : CartridgeName) : nullptr;
	if (!F || !C)
	{
		UE_LOG(LogTemp, Warning, TEXT("PBL Weapon: нет данных для '%s' в Content/Data - hitscan-заглушка"), *FirearmName.ToString());
		return;
	}
	Firearm = *F;
	Cartridge = *C;
	bHasData = true;
	MagSize = Firearm.MagSize;
	if (Firearm.RPM > 0) { FireInterval = 60.0f / Firearm.RPM; }
	// Рассеивание: MOA - полный угол группы; конус - полуугол. 1 MOA = 1/60 град.
	SpreadDeg = Firearm.Dispersion_MOA / 60.0f * 0.5f;
	const float V0 = PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m);
	ZeroAngle_rad = PBLBallistics::SolveZeroAngle(Cartridge, FPBLAtmosphere(), V0, Firearm.SightHeight_m, Firearm.ZeroRange_m);
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
	OutOrigin = LOSOrigin - LocalUp * (Firearm.SightHeight_m * 100.0f);
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
		Proj->Launch(this, Cartridge, Origin, Velocity, LOSOrigin, LOSDir, bAuthoritative, Damage);
	}
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
		// Меш под реальную габаритную длину образца: заглушка (SK_Pistol из Lyra, 24 см) становится размером с Glock 17 (20.2 см).
		if (bHasData && Firearm.OverallLength_m > 0.0f && Mesh && Mesh->GetSkeletalMeshAsset())
		{
			const FVector Ext = Mesh->GetSkeletalMeshAsset()->GetBounds().BoxExtent;
			const float MeshLen_cm = 2.0f * FMath::Max(Ext.X, Ext.Y);
			if (MeshLen_cm > 1.0f) { SetActorScale3D(FVector(Firearm.OverallLength_m * 100.0f / MeshLen_cm)); }
		}
	}
}

// ---------------- Отдача из физики (E10.5) ----------------

void APBLWeapon::ApplyPhysicsRecoil()
{
	PBLRecoil::Kick(Recoil, RecoilInfo, UPBLRecoilSettings::Get().Hold(Firearm.Hold), FMath::FRandRange(-1.0f, 1.0f));
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
	// Визуально оружие откатывается назад и задирается сильнее камеры.
	SetActorRelativeLocation(ViewOffset - FVector(Recoil.VisualKick_cm, 0.0f, 0.0f));
	SetActorRelativeRotation(ViewRotation + FRotator(FMath::RadiansToDegrees(Recoil.Pitch - Recoil.PitchRest) * RS.VisualPitchScale, 0.0f, 0.0f));
	if (!Recoil.IsActive())
	{
		SetActorRelativeLocation(ViewOffset);
		SetActorRelativeRotation(ViewRotation);
		SetActorTickEnabled(false);
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

	// Линия прицеливания - из камеры вдоль взгляда (центр экрана). Разброс - на клиенте, направление
	// уходит серверу как есть: тогда локальная косметическая пуля и серверная летят одинаково.
	// Направление - из управляющего поворота, а не из компонента камеры: тот обновляется на кадр позже
	// (bUsePawnControlRotation), и выстрел сразу после поворота ушёл бы по старому взгляду.
	const UCameraComponent* Cam = OwnerCharacter->GetFirstPersonCamera();
	const FVector LOSOrigin = Cam->GetComponentLocation();
	const FVector AimDir = OwnerCharacter->GetController() ? OwnerCharacter->GetController()->GetControlRotation().Vector() : Cam->GetForwardVector();
	const FVector LOSDir = ApplySpread(AimDir);

	// Мгновенная локальная реакция (как в CS), сервер догонит.
	PlayLocalFireFX();
	PlayMuzzleFX();
	if (bHasData && UPBLRecoilSettings::Get().bPhysicsRecoil) { ApplyPhysicsRecoil(); }
	else { OwnerCharacter->ApplyRecoil(RecoilPitch, FMath::RandRange(-RecoilYawRandom, RecoilYawRandom)); }
	if (!HasAuthority())
	{
		AmmoInMag = FMath::Max(AmmoInMag - 1, 0);  // предсказание для HUD
		FVector O, D; ComputeLaunch(LOSOrigin, LOSDir, O, D);
		if (bHasData) { LaunchProjectile(O, D * PBLBallistics::MuzzleVelocity(Cartridge, Firearm.Barrel_m), LOSOrigin, LOSDir, false); }
	}

	Server_Fire(LOSOrigin, LOSDir);
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

	const FVector LOSOrigin = Origin;
	const FVector LOSDir = FVector(Dir).GetSafeNormal();
	Multicast_ShotFX(LOSOrigin, LOSDir);

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

	FVector O, D; ComputeLaunch(LOSOrigin, LOSDir, O, D);
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

void APBLWeapon::Multicast_ShotFX_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir)
{
	// Владелец уже сделал всё локально; сервер (listen) запускает авторитетную пулю сам.
	if (HasAuthority() || (OwnerCharacter && OwnerCharacter->IsLocallyControlled())) { return; }
	PlayMuzzleFX();
	if (bHasData)
	{
		FVector O, D; ComputeLaunch(Origin, FVector(Dir), O, D);
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

void APBLWeapon::OnProjectileImpact(const FHitResult& Hit, const FVector& ImpactVelocity_mps, float DamageToApply)
{
	if (!HasAuthority()) { return; }
	const bool bCharacter = Hit.GetComponent() && Hit.GetComponent()->GetCollisionObjectType() == ECC_Pawn;
	const bool bHead = bCharacter && Hit.GetComponent()->GetName().Contains(TEXT("Head"));
	if (Hit.GetActor())
	{
		const float Applied = UGameplayStatics::ApplyPointDamage(Hit.GetActor(), DamageToApply, ImpactVelocity_mps.GetSafeNormal(), Hit,
			OwnerCharacter ? OwnerCharacter->GetController() : nullptr, this, nullptr);
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
		FFileHelper::SaveStringToFile(TEXT("time,firearm,cartridge,v0_mps,distance_m,vimp_mps,energy_J,tof_s,drop_cm,hit,target,medium,pen_cm,v_exit_mps,dia_mm,stopped\n"), *Path);
	}
	const FString Line = FString::Printf(TEXT("%s,%s,%s,%.1f,%.2f,%.1f,%.0f,%.4f,%.1f,%d,%d,%s,%.1f,%.1f,%.1f,%d\n"),
		*FDateTime::Now().ToIso8601(), *Firearm.Name.ToString(), *Cartridge.Name.ToString(), R.V0_mps, R.Distance_m, R.ImpactVelocity_mps,
		R.ImpactEnergy_J, R.TimeOfFlight_s, R.DropFromLOS_m * 100.0f, R.bHit ? 1 : 0, R.bHitTarget ? 1 : 0,
		*R.PenetrationMaterial.ToString(), R.Penetration_m * 100.0f, R.MediumExitVelocity_mps, R.FinalDiameter_mm, R.bStoppedInMedium ? 1 : 0);
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
