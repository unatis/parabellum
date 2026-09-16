#include "Range/PBLMaterialBlock.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APBLMaterialBlock::APBLMaterialBlock()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Block = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Block"));
	SetRootComponent(Block);
	if (Cube.Succeeded()) { Block->SetStaticMesh(Cube.Object); }
	Block->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Block->SetCollisionResponseToAllChannels(ECR_Block);
	Block->SetCastShadow(false);
}

void APBLMaterialBlock::BeginPlay()
{
	Super::BeginPlay();
	Block->SetRelativeScale3D(Size / 100.0f);
	if (UMaterialInterface* M = GelMaterial.LoadSynchronous()) { Block->SetMaterial(0, M); }
}

void APBLMaterialBlock::AddChannel(const FVector& Entry, const FVector& Dir, float Depth_cm, float NeckDepth_cm, float EntryDia_mm, float FinalDia_mm, bool bExit)
{
	if (!HasAuthority()) { return; }
	Multicast_AddChannel(Entry, Dir, Depth_cm, NeckDepth_cm, EntryDia_mm, FinalDia_mm, bExit);
}

void APBLMaterialBlock::ClearChannels()
{
	if (!HasAuthority()) { return; }
	Multicast_Clear();
}

void APBLMaterialBlock::Multicast_Clear_Implementation()
{
	for (UStaticMeshComponent* C : ChannelParts) { if (C) { C->DestroyComponent(); } }
	ChannelParts.Reset();
	ChannelCount = 0;
}

void APBLMaterialBlock::Multicast_AddChannel_Implementation(FVector_NetQuantize Entry, FVector_NetQuantizeNormal Dir, float Depth_cm, float NeckDepth_cm, float EntryDia_mm, float FinalDia_mm, bool bExit)
{
	if (ChannelCount >= MaxChannels) { Multicast_Clear_Implementation(); }
	++ChannelCount;
	const FVector D = FVector(Dir).GetSafeNormal();
	const FVector End = FVector(Entry) + D * Depth_cm;
	const float EntryDia_cm = FMath::Max(EntryDia_mm / 10.0f * ChannelVisualScale, 0.6f);
	const float MainDia_cm = FMath::Max(FinalDia_mm / 10.0f * ChannelVisualScale, EntryDia_cm);
	// Шейка (узкий канал до кувырка/раскрытия), затем основной канал, в конце - пуля (если осталась в блоке).
	const float Neck = FMath::Clamp(NeckDepth_cm, 0.0f, Depth_cm);
	if (Neck > 0.5f) { AddCylinder(Entry, FVector(Entry) + D * Neck, EntryDia_cm); }
	if (Depth_cm - Neck > 0.5f) { AddCylinder(FVector(Entry) + D * Neck, End, Neck > 0.5f ? MainDia_cm * 1.6f : MainDia_cm); }
	if (!bExit) { AddSphere(End, FMath::Max(FinalDia_mm / 10.0f, 0.9f) * 1.2f); }
}

UStaticMeshComponent* APBLMaterialBlock::AddCylinder(const FVector& From, const FVector& To, float Dia_cm)
{
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cyl) { return nullptr; }
	const FVector Delta = To - From;
	const float Len = Delta.Size();
	if (Len < 0.1f) { return nullptr; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Cyl);
	if (UMaterialInterface* M = ChannelMaterial.LoadSynchronous()) { C->SetMaterial(0, M); }
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->RegisterComponent();
	C->AttachToComponent(Block, FAttachmentTransformRules::KeepWorldTransform);
	// Цилиндр движка: высота 100 по Z, диаметр 100.
	C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Delta / Len).ToQuat(), From + Delta * 0.5f, FVector(Dia_cm / 100.0f, Dia_cm / 100.0f, Len / 100.0f)));
	ChannelParts.Add(C);
	return C;
}

UStaticMeshComponent* APBLMaterialBlock::AddSphere(const FVector& At, float Dia_cm)
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Sphere) { return nullptr; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Sphere);
	if (UMaterialInterface* M = ChannelMaterial.LoadSynchronous()) { C->SetMaterial(0, M); }
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->RegisterComponent();
	C->AttachToComponent(Block, FAttachmentTransformRules::KeepWorldTransform);
	C->SetWorldTransform(FTransform(FQuat::Identity, At, FVector(Dia_cm / 100.0f)));
	ChannelParts.Add(C);
	return C;
}

static FAutoConsoleCommandWithWorld CmdClearGel(TEXT("pbl.Range.ClearGel"), TEXT("Remove wound channels from all gel blocks"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) { return; }
		for (TActorIterator<APBLMaterialBlock> It(World); It; ++It) { It->ClearChannels(); }
	}));
