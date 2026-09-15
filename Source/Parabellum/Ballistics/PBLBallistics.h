#pragma once

#include "CoreMinimal.h"
#include "Ballistics/PBLBallisticsTypes.h"

/**
 * Внешняя баллистика: модель точечной массы со стандартными драг-функциями G1/G7
 * (табличные Cd по числу Маха, как в баллистических калькуляторах), атмосфера ICAO, гравитация.
 *
 * Замедление: a = (pi/8) * rho * Cd_std(M) * v^2 / BC_SI, где BC_SI = BC[lb/in^2] * 703.0696 кг/м^2.
 * Тождественно классической форме retardation = (rho/rho0) * G(v) / BC.
 * Интегратор RK4 с фиксированным шагом. Детерминирован - клиент может повторить расчёт сервера.
 */
namespace PBLBallistics
{
	constexpr float Gravity = 9.80665f;
	constexpr float BC_LbIn2_to_KgM2 = 703.0696f;

	PARABELLUM_API float AirDensity(const FPBLAtmosphere& Atm);
	PARABELLUM_API float SpeedOfSound(const FPBLAtmosphere& Atm);
	PARABELLUM_API float StandardCd(EPBLDragModel Model, float Mach);
	PARABELLUM_API float MuzzleVelocity(const FPBLCartridgeData& C, float Barrel_m);
	PARABELLUM_API FVector Acceleration(const FPBLCartridgeData& C, const FPBLAtmosphere& Atm, const FVector& Velocity);
	PARABELLUM_API void Step(const FPBLCartridgeData& C, const FPBLAtmosphere& Atm, FPBLProjectileState& S, float Dt);

	/** Угол возвышения ствола (рад) над горизонтальной линией прицеливания (на высоте SightHeight над дулом) для нуля на ZeroRange. */
	PARABELLUM_API float SolveZeroAngle(const FPBLCartridgeData& C, const FPBLAtmosphere& Atm, float V0, float SightHeight_m, float ZeroRange_m);

	/** Таблица траектории по дистанциям (горизонтальная стрельба, ноль на ZeroRange). */
	PARABELLUM_API TArray<FPBLTrajectoryRow> BuildTable(const FPBLCartridgeData& C, const FPBLFirearmData& F, const FPBLAtmosphere& Atm,
		float MaxRange_m, float Step_m, float Dt = 0.0005f);
}
