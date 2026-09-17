#include "Weapons/PBLEjectedCase.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

APBLEjectedCase::APBLEjectedCase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Case"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	// Гильза не должна толкать игрока и мешать выстрелам, летящим сквозь неё.
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(true);
}

UStaticMesh* PBLAmmo::MeshFor(FName Cartridge, const TCHAR* Kind)
{
	// Имя патрона вида 9x19_124_FMJ: калибр до первого подчёркивания и есть имя модели.
	FString Caliber = Cartridge.ToString();
	int32 Cut = INDEX_NONE;
	if (Caliber.FindChar(TEXT('_'), Cut)) { Caliber = Caliber.Left(Cut); }
	const FString Path = FString::Printf(TEXT("/Game/Weapons/Ammo/%s%s.%s%s"), Kind, *Caliber, Kind, *Caliber);
	// Модели есть пока только для 9x19; для остальных калибров просто ничего не покажем.
	return LoadObject<UStaticMesh>(nullptr, *Path);
}

UStaticMesh* APBLEjectedCase::CaseMeshFor(FName Cartridge)
{
	return PBLAmmo::MeshFor(Cartridge, TEXT("Case"));
}

APBLEjectedCase* APBLEjectedCase::Eject(UWorld* World, const FTransform& At, const FVector& Velocity,
	const FVector& AngularVel_deg, float Mass_kg, UStaticMesh* CaseMesh, float Scale, float Life, float TimeScale)
{
	if (!World || !CaseMesh) { return nullptr; }
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APBLEjectedCase* C = World->SpawnActor<APBLEjectedCase>(APBLEjectedCase::StaticClass(), At.GetLocation(), At.Rotator(), P);
	if (!C) { return nullptr; }
	C->SetActorScale3D(FVector(Scale));
	C->Mesh->SetStaticMesh(CaseMesh);
	C->Mesh->SetSimulatePhysics(true);
	C->Mesh->SetMassOverrideInKg(NAME_None, FMath::Max(Mass_kg, 0.0005f), true);
	const float k = FMath::Clamp(TimeScale, 0.001f, 1.0f);
	C->TimeScale = k;
	C->Mesh->SetPhysicsLinearVelocity(Velocity * k);
	C->Mesh->SetPhysicsAngularVelocityInDegrees(AngularVel_deg * k);
	if (k < 1.0f)
	{
		// Своя тяжесть вместо движковой: та не умеет идти медленнее для одного тела.
		C->Mesh->SetEnableGravity(false);
		C->SetActorTickEnabled(true);
	}
	C->SetLifeSpan(Life / k);
	return C;
}

void APBLEjectedCase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Замедление в k раз: ускорение должно упасть в k*k раз, тогда траектория та же, только дольше.
	const float G = GetWorld() ? GetWorld()->GetGravityZ() : -980.0f;
	Mesh->AddForce(FVector(0.0f, 0.0f, G * TimeScale * TimeScale), NAME_None, true);
}

// ---------------------------------------------------------------------------------------------
// pbl.Cases - сколько гильз в мире и где они лежат: проверка выброса без глаза на экране.

static FAutoConsoleCommandWithWorld CmdCases(
	TEXT("pbl.Cases"),
	TEXT("List ejected cases currently in the world"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W) { return; }
		int32 N = 0;
		for (TActorIterator<APBLEjectedCase> It(W); It; ++It)
		{
			const FVector L = It->GetActorLocation();
			const FVector V = It->GetRootComponent() ? It->GetRootComponent()->GetComponentVelocity() : FVector::ZeroVector;
			UE_LOG(LogTemp, Display, TEXT("CASE %d: %s, скорость %.0f см/с"), ++N, *L.ToCompactString(), V.Size());
		}
		UE_LOG(LogTemp, Display, TEXT("CASES всего %d"), N);
	}));
