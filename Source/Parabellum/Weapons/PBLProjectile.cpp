#include "Weapons/PBLProjectile.h"

#include "Ballistics/PBLBallistics.h"
#include "Ballistics/PBLPenetration.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Range/PBLGelBlock.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Weapons/PBLWeapon.h"
#include "UObject/ConstructorHelpers.h"

APBLProjectile::APBLProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	SetRootComponent(Visual);
	if (Sphere.Succeeded()) { Visual->SetStaticMesh(Sphere.Object); }
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	Visual->SetRelativeScale3D(FVector(0.03f));   // 3 см шарик; на скорости всё равно виден только трейсер
	Visual->SetVisibility(false);
}

void APBLProjectile::Launch(APBLWeapon* InWeapon, const FPBLCartridgeData& InCartridge, const FVector& Origin, const FVector& Velocity,
	const FVector& InLOSOrigin, const FVector& InLOSDir, bool bInAuthoritative, float InBaseDamage)
{
	Weapon = InWeapon;
	Cartridge = InCartridge;
	Atmosphere = FPBLAtmosphere();
	// Мир UE в сантиметрах, расчёт - в метрах.
	State.Position = Origin / 100.0f;
	State.Velocity = Velocity;
	State.Time = 0.0f;
	LaunchOrigin = Origin;
	LOSOrigin = InLOSOrigin;
	LOSDir = InLOSDir.GetSafeNormal();
	V0 = Velocity.Size();
	bAuthoritative = bInAuthoritative;
	BaseDamage = InBaseDamage;
	SetActorLocation(Origin);
}

void APBLProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDone || !GetWorld()) { return; }

	const FVector PrevPos_cm = State.Position * 100.0f;
	const FPBLProjectileState PrevState = State;
	// Подшаги RK4 внутри кадра; трасса - по всему отрезку кадра (прямая на 6 м - достаточно).
	const int32 N = FMath::Max(1, FMath::CeilToInt(DeltaSeconds / MaxSubstep));
	const float Dt = DeltaSeconds / N;
	for (int32 i = 0; i < N; ++i) { PBLBallistics::Step(Cartridge, Atmosphere, State, Dt); }
	const FVector NewPos_cm = State.Position * 100.0f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PBLProjectile), true);
	Params.AddIgnoredActor(this);
	if (Weapon) { Params.AddIgnoredActor(Weapon); if (Weapon->GetOwner()) { Params.AddIgnoredActor(Weapon->GetOwner()); } }
	if (GetWorld()->LineTraceSingleByChannel(Hit, PrevPos_cm, NewPos_cm, ECC_Visibility, Params))
	{
		// Состояние в точке удара: переинтегрируем от начала кадра мелким шагом до дистанции удара -
		// скорость и время точные даже при длинном кадре (headless 10 fps = 35 м за кадр).
		const float HitDist_m = Hit.Distance / 100.0f;
		State = PrevState;
		const float FineDt = FMath::Min(Dt, 0.0002f);
		for (int32 Guard = 0; Guard < 100000 && (State.Position - PrevState.Position).Size() < HitDist_m; ++Guard)
		{
			PBLBallistics::Step(Cartridge, Atmosphere, State, FineDt);
		}
		State.Position = Hit.ImpactPoint / 100.0f;
		SetActorLocation(Hit.ImpactPoint);
		if (Weapon) { Weapon->DrawTracerSegment(PrevPos_cm, Hit.ImpactPoint); }
		OnImpact(Hit);
		return;
	}

	SetActorLocation(NewPos_cm);
	if (Weapon) { Weapon->DrawTracerSegment(PrevPos_cm, NewPos_cm); }
	if (State.Time > MaxFlightTime || NewPos_cm.Z < -100000.0f)
	{
		Finish(false, nullptr);
	}
}

void APBLProjectile::OnImpact(const FHitResult& Hit)
{
	if (APBLGelBlock* Block = Cast<APBLGelBlock>(Hit.GetActor()))
	{
		if (PenetrateBlock(Hit, Block)) { return; }
	}
	if (bAuthoritative && Weapon)
	{
		// Урон-заглушка до модели пробития (E10.6): базовый урон, масштабированный остаточной энергией.
		const float E0 = 0.5f * Cartridge.BulletMass_kg * V0 * V0;
		const float E = 0.5f * Cartridge.BulletMass_kg * State.Velocity.SizeSquared();
		const float Dmg = BaseDamage * (E0 > 0.0f ? E / E0 : 1.0f);
		Weapon->OnProjectileImpact(Hit, State.Velocity, Dmg);
	}
	Finish(true, &Hit);
}

bool APBLProjectile::PenetrateBlock(const FHitResult& Hit, APBLGelBlock* Block)
{
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	const FPBLMaterialData* Mat = Data ? Data->FindMaterial(Block->GetMaterialName()) : nullptr;
	UPrimitiveComponent* Comp = Hit.GetComponent();
	if (!Mat || !Comp) { return false; }

	const FVector Dir = State.Velocity.GetSafeNormal();
	const float Vin = State.Velocity.Size();
	const FVector Entry = Hit.ImpactPoint;
	// Толщина по ходу пули: обратная трасса от точки далеко за блоком к точке входа - ловит дальнюю грань.
	const float Far = Comp->Bounds.SphereRadius * 2.0f + 50.0f;
	FHitResult ExitHit;
	float Thickness_m = 0.0f;   // 0 = бесконечная среда (дальняя грань не найдена)
	if (Comp->LineTraceComponent(ExitHit, Entry + Dir * Far, Entry + Dir * 0.05f, FCollisionQueryParams(SCENE_QUERY_STAT(PBLGelExit), true)))
	{
		Thickness_m = FVector::Dist(Entry, ExitHit.ImpactPoint) / 100.0f;
	}
	const FPBLPenetrationResult R = PBLPenetration::Penetrate(Cartridge, *Mat, Vin, Thickness_m);
	UE_LOG(LogTemp, Verbose, TEXT("PBL penetrate: %s entry %s dir %s V_in %.1f thickness %.3f m -> depth %.3f m %s"),
		*Block->GetActorNameOrLabel(), *Entry.ToCompactString(), *Dir.ToCompactString(), Vin, Thickness_m, R.Depth_m, R.bStopped ? TEXT("stopped") : TEXT("exit"));

	if (PenMaterial.IsNone()) { PenMaterial = Mat->Name; PenEntryV = Vin; }
	Pen_m += R.Depth_m;
	PenExitV = R.ExitVelocity_mps;
	PenFinalDia_m = R.FinalDiameter_m;
	bPenStopped = R.bStopped;

	const float NeckDepth_cm = R.bExpanded ? Cartridge.ExpansionDepth_m * 100.0f : (R.YawDepth_m >= 0.0f ? R.YawDepth_m * 100.0f : R.Depth_m * 100.0f);
	if (bAuthoritative)
	{
		Block->AddChannel(Entry, Dir, R.Depth_m * 100.0f, NeckDepth_cm, Cartridge.Diameter_m * 1000.0f, R.FinalDiameter_m * 1000.0f, !R.bStopped);
		if (Weapon) { Weapon->OnProjectileImpact(Hit, State.Velocity, 0.0f); }
	}

	const FVector StopPoint = Entry + Dir * (R.Depth_m * 100.0f);
	if (R.bStopped)
	{
		State.Position = StopPoint / 100.0f;
		State.Velocity = FVector::ZeroVector;
		SetActorLocation(StopPoint);
		// Скорость удара в отчёте - скорость входа в среду.
		FHitResult H = Hit;
		Finish(true, &H);
		return true;
	}
	// Прошла насквозь: продолжаем полёт сразу за дальней гранью с остаточной скоростью.
	State.Position = (StopPoint + Dir * 0.2f) / 100.0f;
	State.Velocity = Dir * R.ExitVelocity_mps;
	SetActorLocation(StopPoint + Dir * 0.2f);
	return true;
}

void APBLProjectile::Finish(bool bHit, const FHitResult* Hit)
{
	bDone = true;
	if (bAuthoritative && Weapon)
	{
		FPBLShotReport R;
		R.V0_mps = V0;
		R.Distance_m = (GetActorLocation() - LaunchOrigin).Size() / 100.0f;
		R.ImpactVelocity_mps = bPenStopped ? PenEntryV : State.Velocity.Size();
		R.ImpactEnergy_J = 0.5f * Cartridge.BulletMass_kg * R.ImpactVelocity_mps * R.ImpactVelocity_mps;
		R.TimeOfFlight_s = State.Time;
		// Отклонение от линии прицеливания: проекция вектора (удар - LOSOrigin) на перпендикуляр к LOS по вертикали.
		const FVector Rel = (GetActorLocation() - LOSOrigin) / 100.0f;
		const FVector Along = LOSDir * FVector::DotProduct(Rel, LOSDir);
		R.DropFromLOS_m = (Rel - Along).Z;
		R.bHit = bHit;
		R.bHitTarget = bHit && Hit && Hit->GetComponent() && Hit->GetComponent()->GetCollisionObjectType() == ECC_Pawn;
		if (bHit && Hit && Hit->GetActor()) { R.HitActor = FName(*Hit->GetActor()->GetActorNameOrLabel()); }
		R.PenetrationMaterial = PenMaterial;
		R.Penetration_m = Pen_m;
		R.MediumEntryVelocity_mps = PenEntryV;
		R.MediumExitVelocity_mps = PenExitV;
		R.FinalDiameter_mm = PenFinalDia_m * 1000.0f;
		R.bStoppedInMedium = bPenStopped;
		Weapon->OnProjectileFinished(R);
	}
	Destroy();
}
