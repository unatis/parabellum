#include "Weapons/PBLProjectile.h"

#include "Ballistics/PBLBallistics.h"
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
		// Состояние в точке удара: линейная интерполяция по отрезку кадра.
		const float A = FMath::Clamp(Hit.Distance / FMath::Max((NewPos_cm - PrevPos_cm).Size(), 1e-3f), 0.0f, 1.0f);
		State.Position = Hit.ImpactPoint / 100.0f;
		State.Time -= DeltaSeconds * (1.0f - A);
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

void APBLProjectile::Finish(bool bHit, const FHitResult* Hit)
{
	bDone = true;
	if (bAuthoritative && Weapon)
	{
		FPBLShotReport R;
		R.V0_mps = V0;
		R.Distance_m = (GetActorLocation() - LaunchOrigin).Size() / 100.0f;
		R.ImpactVelocity_mps = State.Velocity.Size();
		R.ImpactEnergy_J = 0.5f * Cartridge.BulletMass_kg * R.ImpactVelocity_mps * R.ImpactVelocity_mps;
		R.TimeOfFlight_s = State.Time;
		// Отклонение от линии прицеливания: проекция вектора (удар - LOSOrigin) на перпендикуляр к LOS по вертикали.
		const FVector Rel = (GetActorLocation() - LOSOrigin) / 100.0f;
		const FVector Along = LOSDir * FVector::DotProduct(Rel, LOSDir);
		R.DropFromLOS_m = (Rel - Along).Z;
		R.bHit = bHit;
		R.bHitTarget = bHit && Hit && Hit->GetComponent() && Hit->GetComponent()->GetCollisionObjectType() == ECC_Pawn;
		Weapon->OnProjectileFinished(R);
	}
	Destroy();
}
