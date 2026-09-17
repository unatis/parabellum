#include "Weapons/PBLProjectile.h"

#include "Ballistics/PBLBallistics.h"
#include "Ballistics/PBLPenetration.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Range/PBLMaterialBlock.h"
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
	const FVector& InLOSOrigin, const FVector& InLOSDir, bool bInAuthoritative, float InBaseDamage, const FPBLAtmosphere& InAtmosphere)
{
	Weapon = InWeapon;
	Cartridge = InCartridge;
	Atmosphere = InAtmosphere;
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
	if (!IsValid(Weapon)) { Weapon = nullptr; }   // оружие могло быть уничтожено в полёте
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
		if (Weapon) { Weapon->DrawTracerSegment(bFirstTracer ? Weapon->GetMuzzleLocation() : PrevPos_cm, Hit.ImpactPoint); bFirstTracer = false; }
		OnImpact(Hit);
		return;
	}

	SetActorLocation(NewPos_cm);
	if (Weapon) { Weapon->DrawTracerSegment(bFirstTracer ? Weapon->GetMuzzleLocation() : PrevPos_cm, NewPos_cm); bFirstTracer = false; }
	if (State.Time > MaxFlightTime || NewPos_cm.Z < -100000.0f)
	{
		Finish(false, nullptr);
	}
}

void APBLProjectile::OnImpact(const FHitResult& Hit)
{
	const bool bPawn = Hit.GetComponent() && Hit.GetComponent()->GetCollisionObjectType() == ECC_Pawn;
	if (bPawn && Cartridge.BulletMass_kg > 0.0f)
	{
		if (WoundPawn(Hit)) { return; }
	}
	if (!bPawn)
	{
		APBLMaterialBlock* Block = Cast<APBLMaterialBlock>(Hit.GetActor());
		if (Block && !Block->GetBodyPart().IsNone()) { if (PenetrateBody(Hit, Block)) { return; } }
		if (PenetrateLayer(Hit, Block ? Block->GetMaterialName() : DefaultWorldMaterial, Block)) { return; }
	}
	if (bAuthoritative && IsValid(Weapon))
	{
		// Урон-заглушка до модели пробития (E10.6): базовый урон, масштабированный остаточной энергией.
		const float E0 = 0.5f * Cartridge.BulletMass_kg * V0 * V0;
		const float E = 0.5f * Cartridge.BulletMass_kg * State.Velocity.SizeSquared();
		const float Dmg = BaseDamage * (E0 > 0.0f ? E / E0 : 1.0f);
		// Пуля здесь останавливается, значит отдала цели всю оставшуюся энергию.
		Weapon->OnProjectileImpact(Hit, State.Velocity, Dmg, E, true);
	}
	Finish(true, &Hit);
}

bool APBLProjectile::PenetrateLayer(const FHitResult& Hit, const FName MaterialName, APBLMaterialBlock* Block)
{
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	const FPBLMaterialData* Mat = Data ? Data->FindMaterial(MaterialName) : nullptr;
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
	// Угол от нормали поверхности; толщина по нормали = путь * cos.
	const float CosA = FMath::Abs(FVector::DotProduct(Dir, Hit.ImpactNormal));
	const float AngleFromNormal = FMath::Acos(FMath::Clamp(CosA, 0.0f, 1.0f));
	const float NormalThickness_m = Thickness_m * FMath::Max(CosA, 0.05f);
	FPBLPenetrationResult R = (Thickness_m > 0.0f) ? PBLPenetration::PassLayer(Cartridge, *Mat, Vin, NormalThickness_m, AngleFromNormal)
	                                              : PBLPenetration::Penetrate(Cartridge, *Mat, Vin, 0.0f);
	++PenLayers;
	// Рикошет: не пробила и угол к поверхности мал - отражаем с потерей скорости и небольшим случайным уводом.
	// Ниже ~40 м/с пуля не рикошетит, а ложится (иначе на пределе энергии скачет по полу до бесконечности).
	if (Vin > 40.0f && PBLPenetration::ShouldRicochet(*Mat, AngleFromNormal, !R.bStopped))
	{
		++PenRicochets;
		FVector NewDir = (Dir - 2.0f * FVector::DotProduct(Dir, Hit.ImpactNormal) * Hit.ImpactNormal).GetSafeNormal();
		NewDir = FMath::VRandCone(NewDir, FMath::DegreesToRadians(3.0f));
		const float Vout = Vin * Mat->RicochetRestitution;
		if (bAuthoritative && Weapon) { Weapon->OnProjectileImpact(Hit, State.Velocity, 0.0f); }
		UE_LOG(LogTemp, Display, TEXT("PBL ricochet: %s at %.0f deg to surface, %.0f -> %.0f m/s"), *MaterialName.ToString(),
			90.0f - FMath::RadiansToDegrees(AngleFromNormal), Vin, Vout);
		State.Position = (Hit.ImpactPoint + Hit.ImpactNormal * 0.5f) / 100.0f;
		State.Velocity = NewDir * Vout;
		SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * 0.5f);
		return true;
	}
	UE_LOG(LogTemp, Verbose, TEXT("PBL penetrate: %s entry %s dir %s V_in %.1f thickness %.3f m -> depth %.3f m %s"),
		*MaterialName.ToString(), *Entry.ToCompactString(), *Dir.ToCompactString(), Vin, Thickness_m, R.Depth_m, R.bStopped ? TEXT("stopped") : TEXT("exit"));

	// В отчёте - первый слой (то, во что стреляли): материал, глубина в нём, скорость на выходе; дальнейшие слои - счётчиком.
	if (PenMaterial.IsNone())
	{
		PenMaterial = Mat->Name;
		PenEntryV = Vin;
		Pen_m = R.Depth_m;
		PenExitV = R.ExitVelocity_mps;
		PenFinalDia_m = R.FinalDiameter_m;
		bPenStopped = R.bStopped;
	}

	const float NeckDepth_cm = R.bExpanded ? Cartridge.ExpansionDepth_m * 100.0f : (R.YawDepth_m >= 0.0f ? R.YawDepth_m * 100.0f : R.Depth_m * 100.0f);
	if (bAuthoritative)
	{
		if (!IsValid(Weapon)) { Weapon = nullptr; }
		if (Block) { Block->AddChannel(Entry, Dir, R.Depth_m * 100.0f, NeckDepth_cm, Cartridge.Diameter_m * 1000.0f, R.FinalDiameter_m * 1000.0f, !R.bStopped); }
		UE_LOG(LogTemp, Display, TEXT("PBL layer: %s %.1f mm @ %.0f deg: %.0f -> %s"), *MaterialName.ToString(), NormalThickness_m * 1000.0f,
			FMath::RadiansToDegrees(AngleFromNormal), Vin, R.bStopped ? TEXT("stopped") : *FString::Printf(TEXT("%.0f m/s"), R.ExitVelocity_mps));
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
	UE_LOG(LogTemp, Verbose, TEXT("PBL layer exit: pos %s vel %s (%.1f m/s)"), *State.Position.ToCompactString(), *State.Velocity.ToCompactString(), State.Velocity.Size());
	return true;
}

/** Дистанция до выхода луча из габаритов компонента (метод плит). Нужна, чтобы после сквозного пробития
 *  возобновить полёт гарантированно снаружи хитбокса: LineTraceComponent по сфере/капсуле часто не находит дальнюю грань,
 *  и пуля стартовала изнутри - попадая в ту же цель второй раз (поймано 2026-09-17 на голове манекена). */
static float RayExitDistanceFromBounds(const UPrimitiveComponent& Comp, const FVector& Origin, const FVector& Dir)
{
	const FBox Box = Comp.Bounds.GetBox();
	float TMax = FLT_MAX;
	for (int32 A = 0; A < 3; ++A)
	{
		const float D = Dir[A];
		if (FMath::Abs(D) < KINDA_SMALL_NUMBER) { continue; }
		const float T1 = (Box.Min[A] - Origin[A]) / D;
		const float T2 = (Box.Max[A] - Origin[A]) / D;
		TMax = FMath::Min(TMax, FMath::Max(T1, T2));
	}
	return (TMax < FLT_MAX) ? FMath::Max(TMax, 0.0f) : 2.0f * Comp.Bounds.SphereRadius;
}

FName APBLProjectile::BodyPartForHit(const FHitResult& Hit) const
{
	if (Hit.GetComponent() && Hit.GetComponent()->GetName().Contains(TEXT("Head"))) { return TEXT("Head"); }
	const AActor* A = Hit.GetActor();
	const float Base_cm = A ? A->GetActorLocation().Z : Hit.ImpactPoint.Z;   // манекен стоит основанием в своей позиции
	const float H = Hit.ImpactPoint.Z - Base_cm;
	if (H > 145.0f) { return TEXT("Neck"); }
	if (H > 85.0f) { return TEXT("Torso"); }
	if (H > 63.0f) { return TEXT("Pelvis"); }
	return TEXT("Leg");
}

bool APBLProjectile::WoundPawn(const FHitResult& Hit)
{
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	UPrimitiveComponent* Comp = Hit.GetComponent();
	const FName Part = BodyPartForHit(Hit);
	const TArray<FPBLBodyLayer>* PartLayers = Data ? Data->FindBodyPart(Part) : nullptr;
	if (!PartLayers || !Comp) { return false; }

	const FVector Dir = State.Velocity.GetSafeNormal();
	const float Vin = State.Velocity.Size();
	const FVector Entry = Hit.ImpactPoint;
	const float Far = Comp->Bounds.SphereRadius * 2.0f + 50.0f;
	FHitResult ExitHit;
	float Path_m = 0.25f;   // если дальняя грань не найдена - средняя толщина тела
	if (Comp->LineTraceComponent(ExitHit, Entry + Dir * Far, Entry + Dir * 0.05f, FCollisionQueryParams(SCENE_QUERY_STAT(PBLWoundExit), true)))
	{
		Path_m = FMath::Max(FVector::Dist(Entry, ExitHit.ImpactPoint) / 100.0f, 0.01f);
	}
	// Точка возобновления полёта - всегда снаружи хитбокса (по габаритам), независимо от того, нашлась ли дальняя грань.
	const FVector GeomExit = Entry + Dir * (RayExitDistanceFromBounds(*Comp, Entry, Dir) + 1.0f);
	// Хитбокс шире тела (капсула r 22 см): путь ограничиваем номинальной глубиной части с поправкой на угол.
	const float Nominal_m = Data->BodyPartThickness(Part);
	if (Nominal_m > 0.0f)
	{
		const float CosA = FMath::Abs(FVector::DotProduct(Dir, Hit.ImpactNormal));
		Path_m = FMath::Min(Path_m, Nominal_m / FMath::Max(CosA, 0.3f));
	}
	const FVector Center = Comp->GetComponentLocation();
	const float ZFromCenter_m = (Entry.Z - Center.Z) / 100.0f;
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	const float Lateral_m = FVector::DotProduct(Entry - Center, Side) / 100.0f;
	TArray<PBLPenetration::FLayerPass> Passes;
	PBLPenetration::PassBody(Cartridge, PBLPenetration::ResolveBodyStack(*PartLayers, Data->AllMaterials(), Path_m, ZFromCenter_m, Lateral_m), Vin, Passes);
	if (Passes.Num() == 0) { return false; }
	float E = 0.0f;
	const float Dmg = PBLPenetration::WoundDamage(Cartridge, Passes, Data->AllMaterials(), E);
	const bool bStopped = Passes.Last().bStopped;
	const float Vout = bStopped ? 0.0f : Passes.Last().V_out;
	float Depth_m = 0.0f;
	FString Summary;
	for (const auto& P : Passes) { Depth_m += P.Depth_m; Summary += FString::Printf(TEXT(" %s->%s"), *P.Material.ToString(), P.bStopped ? TEXT("stop") : *FString::Printf(TEXT("%.0f"), P.V_out)); }

	++PenLayers;
	WoundDmg += Dmg; WoundE += E;
	bHitTargetFlag = true;
	if (PenMaterial.IsNone()) { PenMaterial = Part; PenEntryV = Vin; Pen_m = Depth_m; PenExitV = Vout; PenFinalDia_m = Passes.Last().FinalDiameter_m; bPenStopped = bStopped; }
	if (bAuthoritative)
	{
		UE_LOG(LogTemp, Display, TEXT("PBL wound: %s path %.0f mm: %.0f m/s ->%s | E %.0f J -> damage %.1f%s"), *Part.ToString(), Path_m * 1000.0f, Vin, *Summary, E, Dmg, bStopped ? TEXT(" STOPPED") : TEXT(" EXIT"));
		if (!IsValid(Weapon)) { Weapon = nullptr; }
		if (Weapon) { Weapon->OnProjectileImpact(Hit, State.Velocity, Dmg, E, bStopped); }
	}
	const FVector StopPoint = Entry + Dir * (Depth_m * 100.0f);
	if (bStopped)
	{
		State.Position = StopPoint / 100.0f;
		State.Velocity = FVector::ZeroVector;
		SetActorLocation(StopPoint);
		FHitResult H = Hit;
		Finish(true, &H);
		return true;
	}
	// Сквозное: летим дальше с остаточной скоростью от геометрического выхода из хитбокса (он шире тела - иначе второй удар по той же цели).
	State.Position = (GeomExit + Dir * 0.2f) / 100.0f;
	State.Velocity = Dir * Vout;
	SetActorLocation(GeomExit + Dir * 0.2f);
	return true;
}

bool APBLProjectile::PenetrateBody(const FHitResult& Hit, APBLMaterialBlock* Block)
{
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	const TArray<FPBLBodyLayer>* PartLayers = Data ? Data->FindBodyPart(Block->GetBodyPart()) : nullptr;
	UPrimitiveComponent* Comp = Hit.GetComponent();
	if (!PartLayers || !Comp) { return false; }

	const FVector Dir = State.Velocity.GetSafeNormal();
	const float Vin = State.Velocity.Size();
	const FVector Entry = Hit.ImpactPoint;
	const float Far = Comp->Bounds.SphereRadius * 2.0f + 50.0f;
	FHitResult ExitHit;
	float Path_m = 0.0f;
	if (Comp->LineTraceComponent(ExitHit, Entry + Dir * Far, Entry + Dir * 0.05f, FCollisionQueryParams(SCENE_QUERY_STAT(PBLBodyExit), true)))
	{
		Path_m = FVector::Dist(Entry, ExitHit.ImpactPoint) / 100.0f;
	}
	if (Path_m <= 0.0f) { return false; }
	// Точка входа относительно части: высота от центра (полосы рёбер) и боковое смещение пути от вертикальной оси (кость конечности).
	const FVector Center = Block->GetActorLocation();
	const float ZFromCenter_m = (Entry.Z - Center.Z) / 100.0f;
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	const float Lateral_m = FVector::DotProduct(Entry - Center, Side) / 100.0f;
	const TArray<PBLPenetration::FResolvedLayer> Stack = PBLPenetration::ResolveBodyStack(*PartLayers, Data->AllMaterials(), Path_m, ZFromCenter_m, Lateral_m);
	TArray<PBLPenetration::FLayerPass> Passes;
	PBLPenetration::PassBody(Cartridge, Stack, Vin, Passes);
	if (Passes.Num() == 0) { return false; }

	float Depth_m = 0.0f;
	FString Summary;
	for (const auto& P : Passes)
	{
		if (bAuthoritative) { Block->AddChannel(Entry + Dir * (Depth_m * 100.0f), Dir, P.Depth_m * 100.0f, 0.0f, Cartridge.Diameter_m * 1000.0f, P.FinalDiameter_m * 1000.0f, !P.bStopped); }
		Depth_m += P.Depth_m;
		Summary += FString::Printf(TEXT(" %s %.0fmm->%s"), *P.Material.ToString(), P.Thickness_m * 1000.0f, P.bStopped ? TEXT("stop") : *FString::Printf(TEXT("%.0f"), P.V_out));
	}
	const bool bStopped = Passes.Last().bStopped;
	const float Vout = bStopped ? 0.0f : Passes.Last().V_out;
	++PenLayers;
	if (PenMaterial.IsNone())
	{
		PenMaterial = Block->GetBodyPart();
		PenEntryV = Vin; Pen_m = Depth_m; PenExitV = Vout; PenFinalDia_m = Passes.Last().FinalDiameter_m; bPenStopped = bStopped;
	}
	if (bAuthoritative)
	{
		UE_LOG(LogTemp, Display, TEXT("PBL body: %s path %.0f mm (z %+.0f cm, lat %.0f cm): %.0f ->%s%s"), *Block->GetBodyPart().ToString(), Path_m * 1000.0f,
			ZFromCenter_m * 100.0f, Lateral_m * 100.0f, Vin, *Summary, bStopped ? TEXT(" STOPPED") : TEXT(" EXIT"));
		if (!IsValid(Weapon)) { Weapon = nullptr; }
		if (Weapon) { Weapon->OnProjectileImpact(Hit, State.Velocity, 0.0f); }
	}
	const FVector StopPoint = Entry + Dir * (Depth_m * 100.0f);
	if (bStopped)
	{
		State.Position = StopPoint / 100.0f;
		State.Velocity = FVector::ZeroVector;
		SetActorLocation(StopPoint);
		FHitResult H = Hit;
		Finish(true, &H);
		return true;
	}
	State.Position = (StopPoint + Dir * 0.2f) / 100.0f;
	State.Velocity = Dir * Vout;
	SetActorLocation(StopPoint + Dir * 0.2f);
	return true;
}

void APBLProjectile::Finish(bool bHit, const FHitResult* Hit)
{
	bDone = true;
	if (bAuthoritative && IsValid(Weapon))
	{
		FPBLShotReport R;
		R.V0_mps = V0;
		R.Distance_m = (GetActorLocation() - LaunchOrigin).Size() / 100.0f;
		// Скорость удара: вход в первый слой/цель, если он был; иначе текущая.
		R.ImpactVelocity_mps = !PenMaterial.IsNone() ? PenEntryV : State.Velocity.Size();
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
		R.Layers = PenLayers;
		R.Ricochets = PenRicochets;
		R.WoundDamage = WoundDmg;
		R.WoundEnergy_J = WoundE;
		if (bHitTargetFlag) { R.bHitTarget = true; }
		Weapon->OnProjectileFinished(R);
	}
	Destroy();
}
