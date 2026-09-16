#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PBLTargetDummy.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;
class USphereComponent;
class UAnimSequence;

/**
 * Мишень тира: манекен с здоровьем. Авторитетна на сервере (урон, смерть, респавн),
 * клиентам рассылает только события для анимации. Стреляет и ходит - Э5.4/ИИ, не здесь.
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLTargetDummy : public AActor
{
	GENERATED_BODY()

public:
	APBLTargetDummy();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	bool IsAlive() const { return Health > 0.0f; }

protected:
	virtual void BeginPlay() override;

	void Die(AController* Killer);
	void Respawn();
	void PlayAnim(UAnimSequence* Anim, bool bLoop);
	void BackToIdle();

	UFUNCTION(NetMulticast, Unreliable) void Multicast_Hit();
	UFUNCTION(NetMulticast, Reliable)   void Multicast_Die();
	UFUNCTION(NetMulticast, Reliable)   void Multicast_Respawn();

	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Target")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Хитбоксы как в CS: капсула тела и сфера головы (сфера едет за костью головы). Меш сам не коллидирует. */
	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Target")
	TObjectPtr<UCapsuleComponent> BodyHitbox;

	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Target")
	TObjectPtr<USphereComponent> HeadHitbox;

	UPROPERTY(Replicated)
	float Health = 100.0f;

	FTimerHandle RespawnTimer;
	FTimerHandle IdleTimer;

	// --- Config ---
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") TSoftObjectPtr<USkeletalMesh> DummyMesh;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") TSoftObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") TSoftObjectPtr<UAnimSequence> HitAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") TSoftObjectPtr<UAnimSequence> DeathAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float MaxHealth = 100.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float HeadshotMultiplier = 4.0f;
	/** Кость головы (Mixamo: mixamorig:Head). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") FName HeadBone = TEXT("mixamorig:Head");
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float HeadRadius = 14.0f;
	/** Центр сферы головы выше начала кости Head по мировой вертикали (в позе покоя), см: кость начинается у шеи, центр черепа ~12 см выше. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float HeadOffsetUp = 12.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float BodyRadius = 22.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float BodyHalfHeight = 75.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float RespawnDelay = 4.0f;
	/** Сколько секунд после смерти тело остаётся видимым (длина анимации Dying ~ 2-3 с). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float DeathAnimHold = 3.0f;
};
