#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PBLTargetDummy.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;
class USphereComponent;
class UAnimSequence;

/** Реакция на попадание: по зоне и энергии, отданной в теле. */
UENUM()
enum class EPBLHitReaction : uint8
{
	Stagger,    // шатание: слабый удар в тело
	Knockdown   // сбивает с ног: голова или тело выше порога энергии
};

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
	/** Проигрывание задом наперёд (вставание = падение наоборот: отдельной анимации подъёма в паке нет). */
	void PlayAnimReverse(UAnimSequence* Anim);
	void BackToIdle();
	/** Лежит - начать вставать. */
	void StartGetUp();

	UFUNCTION(NetMulticast, Unreliable) void Multicast_Hit();
	UFUNCTION(NetMulticast, Reliable)   void Multicast_Knockdown();
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
	FTimerHandle GetUpTimer;
	/** Сбит с ног и ещё не встал (реакция, не смерть). */
	bool bKnockedDown = false;

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
	/** Энергия в теле, от которой сбивает с ног (Дж). Попадание в голову сбивает при любой энергии. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float KnockdownEnergy_J = 500.0f;
	/** Сколько лежать до попытки встать, с. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float KnockdownHold = 1.2f;
	/** Скорость падения при сбивании с ног (анимация Dying длинная - для удара её ускоряем). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float KnockdownRate = 2.0f;
	/** Скорость вставания (та же анимация задом наперёд). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Target") float GetUpRate = 1.5f;
};
