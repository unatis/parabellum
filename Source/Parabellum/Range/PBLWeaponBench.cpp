#include "Range/PBLWeaponBench.h"

#include "Ballistics/PBLBallistics.h"
#include "Weapons/PBLEjectedCase.h"
#include "Ballistics/PBLRecoil.h"
#include "Ballistics/PBLRecoilSettings.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

APBLWeaponBench::APBLWeaponBench()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);
}

void APBLWeaponBench::BeginPlay()
{
	Super::BeginPlay();
	BuildParts();
	RefreshTargets();
}

void APBLWeaponBench::BuildParts()
{
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	if (!Data) { return; }
	Steps = Data->PartSteps(WeaponName, Stage);

	// Детали берём из всех шагов образца: на неполной разборке внутренние остаются на местах, но должны быть видны.
	TSet<FName> AllParts;
	for (const FPBLWeaponPartStep& P : Data->PartSteps(WeaponName, NAME_None)) { AllParts.Add(P.Part); }

	Pivot->SetWorldScale3D(FVector(DisplayScale));
	for (const FName& Part : AllParts)
	{
		if (PartComps.Contains(Part)) { continue; }
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *PartsPath, *Part.ToString(), *Part.ToString()));
		if (!SM) { UE_LOG(LogTemp, Warning, TEXT("PBL Bench: нет меша детали %s"), *Part.ToString()); continue; }
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(SM);
		// Коллизия только для наведения курсором: стрелять по стенду не нужно.
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->RegisterComponent();
		C->AttachToComponent(Pivot, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetRelativeLocation(FVector::ZeroVector);   // геометрия деталей уже в координатах сборки
		PartComps.Add(Part, C);
	}
	UE_LOG(LogTemp, Display, TEXT("PBL Bench: %s %s - %d деталей, стадия %s (%d шагов)"),
		*WeaponName.ToString(), *Generation.ToString(), PartComps.Num(), *Stage.ToString(), Steps.Num());
}

void APBLWeaponBench::RefreshTargets()
{
	TargetOffset.Reset();
	for (int32 i = 0; i < Steps.Num() && i < Step; ++i)
	{
		const FPBLWeaponPartStep& P = Steps[i];
		TargetOffset.Add(P.Part, P.Dir.GetSafeNormal() * P.Dist_cm);
	}
	SetActorTickEnabled(true);
}

void APBLWeaponBench::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Cycle.IsRunning() && bCycleFrozen) { return; }
	if (Cycle.IsRunning())
	{
		// Цикл считается в реальном времени, а показывается замедленно: иначе всё укладывается в три кадра.
		PBLCycle::Step(Cycle, Firearm, DeltaSeconds * TimeScale);
		ApplyCyclePose();
		if (!bCaseEjected && Cycle.X >= EjectTravel_m) { EjectCase(); }
		if (!Cycle.IsRunning())
		{
			Message = FString::Printf(TEXT("цикл завершён за %.1f мс -> предельный темп %.0f выстр/мин"),
				Cycle.CycleTime * 1000.0f, Cycle.CycleTime > 0.0f ? 60.0f / Cycle.CycleTime : 0.0f);
		}
		return;
	}
	bool bMoving = false;
	for (const auto& It : PartComps)
	{
		UStaticMeshComponent* C = It.Value;
		if (!C) { continue; }
		const FVector* T = TargetOffset.Find(It.Key);
		const FVector Goal = T ? *T : FVector::ZeroVector;
		const FVector Cur = C->GetRelativeLocation();
		if (!Cur.Equals(Goal, 0.05f))
		{
			C->SetRelativeLocation(FMath::VInterpConstantTo(Cur, Goal, DeltaSeconds, MoveSpeed));
			bMoving = true;
		}
	}
	if (!bMoving) { SetActorTickEnabled(false); }
}

void APBLWeaponBench::StepForward()
{
	if (Step < Steps.Num()) { ++Step; RefreshTargets(); }
}

void APBLWeaponBench::StepBack()
{
	if (Step > 0) { --Step; RefreshTargets(); }
}

void APBLWeaponBench::ToggleStage()
{
	Stage = (Stage == TEXT("Field")) ? FName(TEXT("Full")) : FName(TEXT("Field"));
	Step = 0;
	if (UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr)
	{
		Steps = Data->PartSteps(WeaponName, Stage);
	}
	RefreshTargets();
}

void APBLWeaponBench::AddRotation(float Yaw, float Pitch)
{
	FRotator R = Pivot->GetRelativeRotation();
	R.Yaw += Yaw;
	R.Pitch = FMath::Clamp(R.Pitch + Pitch, -85.0f, 85.0f);
	Pivot->SetRelativeRotation(R);
}

void APBLWeaponBench::ResetView()
{
	Pivot->SetRelativeRotation(FRotator::ZeroRotator);
	Step = 0;
	RefreshTargets();
}

FString APBLWeaponBench::GetNextPartName() const
{
	return Steps.IsValidIndex(Step) ? Steps[Step].DisplayName : FString();
}

FString APBLWeaponBench::GetStatusLine() const
{
	const FString StageRu = (Stage == TEXT("Field")) ? TEXT("неполная разборка") : TEXT("полная разборка");
	const FString Next = GetNextPartName();
	return FString::Printf(TEXT("%s %s | %s: шаг %d из %d%s"), *WeaponName.ToString(), *Generation.ToString(),
		*StageRu, Step, Steps.Num(), Next.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" | далее: %s"), *Next));
}

void APBLWeaponBench::GetViewFocus(FVector& OutCenter, float& OutRadius) const
{
	FBox Box(ForceInit);
	for (const auto& It : PartComps)
	{
		if (!It.Value) { continue; }
		// Берём целевое положение детали, а не текущее: кадр подбирается сразу под конец движения.
		const FVector* T = TargetOffset.Find(It.Key);
		const FVector Off = T ? Pivot->GetComponentTransform().TransformVector(*T) : FVector::ZeroVector;
		Box += It.Value->GetStaticMesh()->GetBounds().GetBox().TransformBy(It.Value->GetComponentTransform()).ShiftBy(Off - It.Value->GetRelativeLocation() * DisplayScale);
	}
	if (!Box.IsValid) { OutCenter = GetActorLocation(); OutRadius = 30.0f; return; }
	OutCenter = Box.GetCenter();
	OutRadius = FMath::Max(Box.GetExtent().Size(), 10.0f);
}

const FPBLWeaponPartStep* APBLWeaponBench::FindStep(FName Part) const
{
	for (const FPBLWeaponPartStep& P : Steps) { if (P.Part == Part) { return &P; } }
	return nullptr;
}

FName APBLWeaponBench::TraceHover(const FVector& Start, const FVector& End)
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PBLBenchHover), true);
	FName NewHover = NAME_None;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetActor() == this)
	{
		for (const auto& It : PartComps) { if (It.Value == Hit.GetComponent()) { NewHover = It.Key; break; } }
	}
	if (NewHover != Hovered)
	{
		UMaterialInterface* HL = HighlightMaterial.LoadSynchronous();
		if (TObjectPtr<UStaticMeshComponent>* Old = PartComps.Find(Hovered)) { if (*Old) { (*Old)->SetOverlayMaterial(nullptr); } }
		if (TObjectPtr<UStaticMeshComponent>* New = PartComps.Find(NewHover)) { if (*New && HL) { (*New)->SetOverlayMaterial(HL); } }
		Hovered = NewHover;
	}
	return Hovered;
}

bool APBLWeaponBench::TryTakePart(FName Part, FString& OutReason)
{
	const FPBLWeaponPartStep* P = FindStep(Part);
	if (!P)
	{
		OutReason = FString::Printf(TEXT("%s не снимается на этой стадии"), *Part.ToString());
		Message = OutReason;
		return false;
	}
	if (P->Order <= Step)
	{
		OutReason = TEXT("деталь уже снята");
		Message = OutReason;
		return false;
	}
	if (P->Order > Step + 1)
	{
		// Порядок разборки и есть описание того, что чем удерживается.
		const FString Blocking = Steps.IsValidIndex(Step) ? Steps[Step].DisplayName : FString(TEXT("другая деталь"));
		OutReason = FString::Printf(TEXT("держит: %s - снимите сначала её"), *Blocking);
		Message = OutReason;
		return false;
	}
	StepForward();
	Message = FString::Printf(TEXT("снято: %s"), *P->DisplayName);
	return true;
}

FString APBLWeaponBench::GetHoveredName() const
{
	if (const FPBLWeaponPartStep* P = FindStep(Hovered)) { return P->DisplayName; }
	return Hovered.IsNone() ? FString() : Hovered.ToString();
}

FString APBLWeaponBench::GetHoveredDescription() const
{
	const FPBLWeaponPartStep* P = FindStep(Hovered);
	return P ? P->Description : FString();
}

bool APBLWeaponBench::GetPartCenter(FName Part, FVector& Out) const
{
	if (const TObjectPtr<UStaticMeshComponent>* C = PartComps.Find(Part))
	{
		if (*C) { Out = (*C)->Bounds.Origin; return true; }
	}
	return false;
}

// --------------------------------------------------------------------------------------------------
// Цикл автоматики на стенде: те же детали, но в движении, и движение берётся из импульса выстрела.

bool APBLWeaponBench::RidesWithSlide(FName Part) const
{
	// Всё, что сидит в затворе, уходит назад вместе с ним.
	static const TSet<FName> Group = {
		TEXT("Slide"), TEXT("Extractor"), TEXT("ExtractorPlunger"), TEXT("Striker"),
		TEXT("SpacerSleeve"), TEXT("FiringPinSpring"), TEXT("SpringCups"),
		TEXT("SlideCoverPlate"), TEXT("FiringPinSafety")
	};
	return Group.Contains(Part);
}

void APBLWeaponBench::FireCycle()
{
	if (Step > 0) { Message = TEXT("сначала соберите образец"); return; }
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	const FPBLFirearmData* F = Data ? Data->FindFirearm(WeaponName) : nullptr;
	const FPBLCartridgeData* C = F ? Data->FindCartridge(F->Cartridge) : nullptr;
	if (!F || !C) { Message = TEXT("нет данных образца"); return; }
	Firearm = *F;

	// Импульс тот самый, что даёт отдачу при стрельбе: пуля плюс пороховые газы.
	const float V0 = PBLBallistics::MuzzleVelocity(*C, F->Barrel_m);
	const FPBLRecoilInfo R = PBLRecoil::Compute(*C, *F, V0, UPBLRecoilSettings::Get().Hold(F->Hold));
	Cartridge = *C;
	// Патрон выходит из патронника, пройдя собственную длину; дальше донце встречает отражатель.
	EjectTravel_m = C->CaseLength_m + 0.003f;
	bCaseEjected = false;
	CaseMesh = APBLEjectedCase::CaseMeshFor(F->Cartridge);
	bCycleFrozen = false;
	PBLCycle::Fire(Cycle, Firearm, R.Impulse_Ns);
	if (!Cycle.IsRunning())
	{
		Message = FString::Printf(TEXT("схема %s приводится газом - цикл не смоделирован"), *Firearm.ActionScheme.ToString());
		return;
	}
	Message = FString::Printf(TEXT("выстрел: импульс %.2f Н*с -> затвор пошёл назад со скоростью %.2f м/с"), R.Impulse_Ns, Cycle.V);
	SetActorTickEnabled(true);
}

void APBLWeaponBench::CycleSlowMotion()
{
	if (SlowMotionSteps.Num() == 0) { return; }
	int32 Idx = 0;
	for (int32 i = 0; i < SlowMotionSteps.Num(); ++i)
	{
		if (FMath::IsNearlyEqual(SlowMotionSteps[i], TimeScale)) { Idx = i; break; }
	}
	TimeScale = SlowMotionSteps[(Idx + 1) % SlowMotionSteps.Num()];
	Message = FString::Printf(TEXT("показ 1:%.0f"), 1.0f / FMath::Max(TimeScale, 1e-3f));
}

void APBLWeaponBench::ApplyCyclePose()
{
	// Назад в модели - это -X: разборка снимает затвор вперёд по +X.
	const float Back_cm = Cycle.X * 100.0f;
	// Ствол идёт с затвором только до отпирания, дальше стоит и опускается казённой частью.
	const float BarrelBack_cm = FMath::Min(Cycle.X, Firearm.UnlockTravel_m) * 100.0f;
	for (const auto& It : PartComps)
	{
		UStaticMeshComponent* Comp = It.Value;
		if (!Comp) { continue; }
		if (RidesWithSlide(It.Key))
		{
			Comp->SetRelativeLocation(FVector(-Back_cm, 0.0f, 0.0f));
		}
		else if (It.Key == TEXT("Barrel"))
		{
			Comp->SetRelativeLocation(FVector(-BarrelBack_cm, 0.0f, 0.0f));
			// Ноль сборки стоит на дульном срезе, поэтому поворот вокруг него опускает казённую часть - как в жизни.
			Comp->SetRelativeRotation(FRotator(Firearm.BarrelTilt_deg * Cycle.BarrelTiltAlpha, 0.0f, 0.0f));
		}
		else if (It.Key == TEXT("RecoilSpring"))
		{
			// Пружина сжимается; жёсткий меш показываем на половине хода - это условность показа, не расчёт.
			Comp->SetRelativeLocation(FVector(-0.5f * Back_cm, 0.0f, 0.0f));
		}
	}
}

FString APBLWeaponBench::GetCycleLine() const
{
	const float Slow = 1.0f / FMath::Max(TimeScale, 1e-3f);
	if (!Cycle.IsRunning())
	{
		return FString::Printf(TEXT("F - выстрел (показ 1:%.0f, G - сменить)"), Slow);
	}
	const TCHAR* Ph = Cycle.Phase == EPBLCyclePhase::Recoiling ? TEXT("откат")
		: (Cycle.Phase == EPBLCyclePhase::AtRear ? TEXT("заднее положение") : TEXT("накат"));
	return FString::Printf(TEXT("%s: затвор %.1f мм, скорость %.2f м/с, ствол опущен на %.1f град, %.1f мс от выстрела (показ 1:%.0f)"),
		Ph, Cycle.X * 1000.0f, Cycle.V, Firearm.BarrelTilt_deg * Cycle.BarrelTiltAlpha, Cycle.T * 1000.0f, Slow);
}

void APBLWeaponBench::FreezeCycle(float Milliseconds)
{
	FireCycle();
	if (!Cycle.IsRunning()) { return; }
	const float Target = Milliseconds * 0.001f;
	for (int32 i = 0; i < 100000 && Cycle.IsRunning() && Cycle.T < Target; ++i)
	{
		PBLCycle::Step(Cycle, Firearm, 0.0002f);
		if (!bCaseEjected && Cycle.X >= EjectTravel_m) { EjectCase(); }
	}
	ApplyCyclePose();
	bCycleFrozen = Cycle.IsRunning();
	Message = FString::Printf(TEXT("стоп-кадр: %s"), *GetCycleLine());
	// Читаем обратно то, что реально встало в компоненты: расчёт и картинка должны совпадать.
	for (const FName& Part : { FName(TEXT("Slide")), FName(TEXT("Barrel")), FName(TEXT("Striker")) })
	{
		if (const TObjectPtr<UStaticMeshComponent>* C = PartComps.Find(Part))
		{
			if (*C)
			{
				UE_LOG(LogTemp, Display, TEXT("CYCLE POSE %-8s смещение %7.2f мм, наклон %5.2f град"),
					*Part.ToString(), -(*C)->GetRelativeLocation().X * 10.0f, (*C)->GetRelativeRotation().Pitch);
			}
		}
	}
}

void APBLWeaponBench::EjectCase()
{
	bCaseEjected = true;
	if (!CaseMesh) { return; }
	// Назад гильза уходит со скоростью затвора - это прямо из расчёта цикла. Отражатель разворачивает
	// её вбок и вверх на углы из данных: сам удар отражателя мы не считаем.
	const float R = FMath::DegreesToRadians(Firearm.EjectRight_deg);
	const float U = FMath::DegreesToRadians(Firearm.EjectUp_deg);
	const FVector DirModel(-FMath::Cos(R) * FMath::Cos(U), FMath::Sin(R) * FMath::Cos(U), FMath::Sin(U));

	const FTransform& T = Pivot->GetComponentTransform();
	const FVector Port = T.TransformPosition(Firearm.EjectPort_m * 100.0f);
	// Образец на стенде увеличен, гильза увеличена так же - иначе она вылетает как песчинка.
	const float Speed_cms = Cycle.V * 100.0f * DisplayScale;
	const FVector Vel = T.TransformVectorNoScale(DirModel) * Speed_cms;
	const FVector Spin = T.TransformVectorNoScale(FVector(0.0f, 0.0f, 1.0f)) * (Firearm.EjectSpin_rps * 360.0f);

	APBLEjectedCase::Eject(GetWorld(), FTransform(T.Rotator(), Port), Vel, Spin,
		Cartridge.CaseMass_kg, CaseMesh, DisplayScale, 20.0f, TimeScale);
	Message = FString::Printf(TEXT("гильза пошла на ходе %.1f мм, скорость затвора %.2f м/с"),
		Cycle.X * 1000.0f, Cycle.V);
	UE_LOG(LogTemp, Display, TEXT("PBL Bench: %s"), *Message);
}
