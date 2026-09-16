#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ballistics/PBLBallisticsTypes.h"
#include "PBLProjectile.generated.h"

class APBLWeapon;
class APBLMaterialBlock;
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
	UPROPERTY() FName HitActor;
	// --- Проникание в среду (E10.6) ---
	UPROPERTY() FName PenetrationMaterial;
	/** Суммарный путь в плотной среде, м. */
	UPROPERTY() float Penetration_m = 0.0f;
	UPROPERTY() float MediumEntryVelocity_mps = 0.0f;
	/** Скорость на выходе из последнего слоя, м/с (0 = остановилась внутри). */
	UPROPERTY() float MediumExitVelocity_mps = 0.0f;
	UPROPERTY() float FinalDiameter_mm = 0.0f;
	UPROPERTY() bool bStoppedInMedium = false;
	UPROPERTY() int32 Layers = 0;
	UPROPERTY() int32 Ricochets = 0;
	/** Урон по цели из слоёв тканей (0 = не по цели). */
	UPROPERTY() float WoundDamage = 0.0f;
	UPROPERTY() float WoundEnergy_J = 0.0f;
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
		const FVector& LOSOrigin, const FVector& LOSDir, bool bInAuthoritative, float InBaseDamage, const FPBLAtmosphere& InAtmosphere = FPBLAtmosphere());

	virtual void Tick(float DeltaSeconds) override;

protected:
	void OnImpact(const FHitResult& Hit);
	/** Попадание в слой материала (блок или геометрия мира): PassLayer по толщине вдоль пути, рикошет при скользящем угле.
	 *  true = пуля обработана (остановлена, прошла или отскочила). */
	bool PenetrateLayer(const FHitResult& Hit, const FName MaterialName, APBLMaterialBlock* Block);
	/** Часть тела: стек слоёв (кожа/жир/мышцы/кости/органы) по толщине блока вдоль пути. */
	bool PenetrateBody(const FHitResult& Hit, APBLMaterialBlock* Block);
	/** Попадание в живую цель (хитбокс): зона по компоненту/высоте → стек тканей → урон по энергии в слоях; сквозное - продолжает полёт. */
	bool WoundPawn(const FHitResult& Hit);
	/** Часть тела по хитбоксу: сфера головы → Head; капсула - по высоте над основанием актора. */
	FName BodyPartForHit(const FHitResult& Hit) const;
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
	/** Накопленные данные проникания для отчёта. */
	FName PenMaterial;
	float Pen_m = 0.0f, PenEntryV = 0.0f, PenExitV = 0.0f, PenFinalDia_m = 0.0f;
	bool bPenStopped = false;
	int32 PenLayers = 0, PenRicochets = 0;
	float WoundDmg = 0.0f, WoundE = 0.0f;
	bool bHitTargetFlag = false;
	/** Материал геометрии мира без блока (стены/пол полигона). */
	FName DefaultWorldMaterial = TEXT("Concrete");
	/** Первый сегмент трассера рисуем от видимого дула оружия. */
	bool bFirstTracer = true;

	/** Максимальный подшаг интегрирования, с. */
	float MaxSubstep = 0.002f;
	/** Предел времени полёта, с. */
	float MaxFlightTime = 6.0f;
};
