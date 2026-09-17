#include "Targets/PBLTargetDummy.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/DamageEvents.h"
#include "Weapons/PBLBulletDamage.h"
#include "Net/UnrealNetwork.h"
#include "Player/PBLPlayerState.h"
#include "TimerManager.h"

APBLTargetDummy::APBLTargetDummy()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // коллизия - у хитбоксов
	Mesh->SetGenerateOverlapEvents(false);

	// Тело: капсула по центру фигуры (низ капсулы на полу). Голова: сфера, привязывается к кости в BeginPlay.
	BodyHitbox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyHitbox"));
	BodyHitbox->SetupAttachment(Mesh);
	BodyHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyHitbox->SetCollisionObjectType(ECC_Pawn);
	BodyHitbox->SetCollisionResponseToAllChannels(ECR_Block);
	BodyHitbox->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	HeadHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitbox"));
	HeadHitbox->SetupAttachment(Mesh);
	HeadHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadHitbox->SetCollisionObjectType(ECC_Pawn);
	HeadHitbox->SetCollisionResponseToAllChannels(ECR_Block);
	HeadHitbox->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void APBLTargetDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APBLTargetDummy, Health);
}

void APBLTargetDummy::BeginPlay()
{
	Super::BeginPlay();
	if (USkeletalMesh* SM = DummyMesh.LoadSynchronous())
	{
		Mesh->SetSkeletalMeshAsset(SM);
	}
	if (HasAuthority()) { Health = MaxHealth; }
	PlayAnim(IdleAnim.LoadSynchronous(), true);

	BodyHitbox->SetCapsuleSize(BodyRadius, BodyHalfHeight);
	BodyHitbox->SetRelativeLocation(FVector(0.0f, 0.0f, BodyHalfHeight));
	HeadHitbox->SetSphereRadius(HeadRadius);
	if (Mesh->GetBoneIndex(HeadBone) != INDEX_NONE)
	{
		HeadHitbox->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HeadBone);
		// Центр черепа выше основания кости: смещение задаём по мировой вертикали в позе покоя, дальше сфера едет с костью.
		HeadHitbox->SetWorldLocation(Mesh->GetBoneLocation(HeadBone) + FVector(0.0f, 0.0f, HeadOffsetUp));
		UE_LOG(LogTemp, Display, TEXT("PBL: %s head hitbox at %s (bone %s, r %.0f)"), *GetName(), *HeadHitbox->GetComponentLocation().ToCompactString(), *Mesh->GetBoneLocation(HeadBone).ToCompactString(), HeadRadius);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PBL: %s: кость '%s' не найдена, сфера головы на фиксированной высоте"), *GetName(), *HeadBone.ToString());
		for (int32 i = 0; i < Mesh->GetNumBones(); ++i)
		{
			const FString N = Mesh->GetBoneName(i).ToString();
			if (N.Contains(TEXT("head"), ESearchCase::IgnoreCase) || N.Contains(TEXT("neck"), ESearchCase::IgnoreCase) || i < 3)
			{
				UE_LOG(LogTemp, Display, TEXT("PBL:   bone[%d] = %s"), i, *N);
			}
		}
		HeadHitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 165.0f));
	}
}

void APBLTargetDummy::PlayAnim(UAnimSequence* Anim, bool bLoop)
{
	if (!Anim) { return; }
	Mesh->PlayAnimation(Anim, bLoop);
	if (UAnimSingleNodeInstance* Single = Mesh->GetSingleNodeInstance()) { Single->SetPlayRate(1.0f); }
}

void APBLTargetDummy::PlayAnimReverse(UAnimSequence* Anim)
{
	if (!Anim) { return; }
	Mesh->PlayAnimation(Anim, false);
	if (UAnimSingleNodeInstance* Single = Mesh->GetSingleNodeInstance())
	{
		Single->SetPlayRate(-FMath::Max(GetUpRate, 0.1f));
		Single->SetPosition(Anim->GetPlayLength(), false);
		Single->SetPlaying(true);
	}
}

float APBLTargetDummy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !IsAlive() || DamageAmount <= 0.0f) { return 0.0f; }

	bool bHead = false;
	float Energy_J = 0.0f;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* P = static_cast<const FPointDamageEvent*>(&DamageEvent);
		bHead = (P->HitInfo.GetComponent() == HeadHitbox);
	}
	if (DamageEvent.IsOfType(FPBLBulletDamageEvent::ClassID))
	{
		Energy_J = static_cast<const FPBLBulletDamageEvent*>(&DamageEvent)->Energy_J;
	}
	if (bHead) { DamageAmount *= HeadshotMultiplier; }

	Health = FMath::Max(Health - DamageAmount, 0.0f);
	// Реакция - от физики удара, а не от очков: голова сбивает с ног при любой энергии, тело - от KnockdownEnergy_J.
	const EPBLHitReaction Reaction = (bHead || Energy_J >= KnockdownEnergy_J) ? EPBLHitReaction::Knockdown : EPBLHitReaction::Stagger;
	UE_LOG(LogTemp, Display, TEXT("PBL: %s hit for %.0f%s, E %.0f J -> %s, health %.0f"), *GetName(), DamageAmount, bHead ? TEXT(" (HEAD)") : TEXT(""),
		Energy_J, Reaction == EPBLHitReaction::Knockdown ? TEXT("KNOCKDOWN") : TEXT("stagger"), Health);

	if (APBLPlayerState* PS = EventInstigator ? EventInstigator->GetPlayerState<APBLPlayerState>() : nullptr)
	{
		PS->AddHit(bHead);
	}

	if (Health <= 0.0f)
	{
		Die(EventInstigator);
	}
	else if (bKnockedDown)
	{
		// Уже лежит: новая анимация не нужна, урон засчитан.
	}
	else if (Reaction == EPBLHitReaction::Knockdown)
	{
		Multicast_Knockdown();
	}
	else
	{
		Multicast_Hit();
	}
	return DamageAmount;
}

void APBLTargetDummy::Multicast_Knockdown_Implementation()
{
	UAnimSequence* Fall = DeathAnim.LoadSynchronous();
	if (!Fall) { Multicast_Hit_Implementation(); return; }
	GetWorldTimerManager().ClearTimer(IdleTimer);
	bKnockedDown = true;
	PlayAnim(Fall, false);
	if (UAnimSingleNodeInstance* Single = Mesh->GetSingleNodeInstance()) { Single->SetPlayRate(FMath::Max(KnockdownRate, 0.1f)); }
	// Упал -> полежал KnockdownHold -> встал (та же анимация задом наперёд).
	const float FallTime = Fall->GetPlayLength() / FMath::Max(KnockdownRate, 0.1f);
	UE_LOG(LogTemp, Display, TEXT("PBL: %s KNOCKDOWN: fall %.2f s + hold %.2f s -> get up"), *GetName(), FallTime, KnockdownHold);
	GetWorldTimerManager().SetTimer(GetUpTimer, this, &APBLTargetDummy::StartGetUp, FallTime + KnockdownHold, false);
}

void APBLTargetDummy::StartGetUp()
{
	UAnimSequence* Fall = DeathAnim.LoadSynchronous();
	if (!IsAlive() || !Fall) { return; }
	PlayAnimReverse(Fall);
	UE_LOG(LogTemp, Display, TEXT("PBL: %s getting up (%.2f s)"), *GetName(), Fall->GetPlayLength() / FMath::Max(GetUpRate, 0.1f));
	GetWorldTimerManager().SetTimer(GetUpTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bKnockedDown = false;
		BackToIdle();
	}), Fall->GetPlayLength() / FMath::Max(GetUpRate, 0.1f), false);
}

void APBLTargetDummy::Die(AController* Killer)
{
	GetWorldTimerManager().ClearTimer(GetUpTimer);
	bKnockedDown = false;
	if (APBLPlayerState* PS = Killer ? Killer->GetPlayerState<APBLPlayerState>() : nullptr)
	{
		PS->AddKill();
	}
	BodyHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Multicast_Die();
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &APBLTargetDummy::Respawn, DeathAnimHold + RespawnDelay, false);
}

void APBLTargetDummy::Respawn()
{
	Health = MaxHealth;
	bKnockedDown = false;
	BodyHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Multicast_Respawn();
}

void APBLTargetDummy::Multicast_Hit_Implementation()
{
	UAnimSequence* Hit = HitAnim.LoadSynchronous();
	if (!Hit) { return; }
	PlayAnim(Hit, false);
	GetWorldTimerManager().SetTimer(IdleTimer, this, &APBLTargetDummy::BackToIdle, FMath::Max(Hit->GetPlayLength() - 0.1f, 0.1f), false);
}

void APBLTargetDummy::BackToIdle()
{
	if (IsAlive()) { PlayAnim(IdleAnim.LoadSynchronous(), true); }
}

void APBLTargetDummy::Multicast_Die_Implementation()
{
	GetWorldTimerManager().ClearTimer(IdleTimer);
	PlayAnim(DeathAnim.LoadSynchronous(), false);
	// Скрываем тело после анимации смерти, до респавна.
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { Mesh->SetVisibility(false); }), DeathAnimHold, false);
}

void APBLTargetDummy::Multicast_Respawn_Implementation()
{
	Mesh->SetVisibility(true);
	PlayAnim(IdleAnim.LoadSynchronous(), true);
}
