#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ballistics/PBLBallisticsTypes.h"
#include "PBLProjectile.generated.h"

class APBLWeapon;
class UStaticMeshComponent;

/** Итог полёта пули - для отчёта стрелку, HUD и CSV. */
USTRUCT()
struct FPBLShotReport
{
	GENERATED_BODY()
	UPROPERTY() float V0_mps = 0.0f;
	UPROPERTY() float Distance_m = 0.0f;
	UPROPERTY() float ImpactVelocity_mps = 0.0f;
	UPROPERTY() float ImpactEnergy_J = 0.0f;
	UPROPERTY() float TimeOfFlight_s = 0.0f;
	/** Смещение точки попадания от линии прицеливания, м (вверх +). */
	UPROPERTY() float DropFromLOS_m = 0.0f;
	UPROPERTY() bool bHit = false;
	UPROPERTY() bool bHitTarget = false;
};

/**
 * Пуля. На сервере - авторитетная: интегрирует полёт (PBLBallistics, RK4, подшаги), трассирует между
 * шагами, при попадании наносит урон и сообщает оружию. У клиентов - косметическая копия с тем же
 * начальным состоянием (детерминированный расчёт) - трассирует только чтобы остановиться о препятствие.
 * Не реплицируется: одинаковый расчёт с обеих сторон дешевле сети и точнее интерполяции.
 */
UCLASS()
class PARABELLUM_API APBLProjectile : public AActor
{
	GENERATED_BODY()

public:
	APBLProjectile();

	/** Запуск. LOSOrigin/LOSDir - линия прицеливания для расчёта отклонения в отчёте. */
	void Launch(APBLWeapon* InWeapon, const FPBLCartridgeData& InCartridge, const FVector& Origin, const FVector& Velocity,
		const FVector& LOSOrigin, const FVector& LOSDir, bool bInAuthoritative, float InBaseDamage);

	virtual void Tick(float DeltaSeconds) override;

protected:
	void OnImpact(const FHitResult& Hit);
	void Finish(bool bHit, const FHitResult* Hit);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(Transient) TObjectPtr<APBLWeapon> Weapon;

	FPBLCartridgeData Cartridge;
	FPBLAtmosphere Atmosphere;
	FPBLProjectileState State;
	FVector LaunchOrigin, LOSOrigin, LOSDir;
	float V0 = 0.0f;
	float BaseDamage = 0.0f;
	bool bAuthoritative = false;
	bool bDone = false;

	/** Максимальный подшаг интегрирования, с. */
	float MaxSubstep = 0.002f;
	/** Предел времени полёта, с. */
	float MaxFlightTime = 6.0f;
};
