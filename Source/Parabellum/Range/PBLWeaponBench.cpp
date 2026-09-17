#include "Range/PBLWeaponBench.h"

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
