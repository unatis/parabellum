#pragma once

#include "CoreMinimal.h"
#include "Ballistics/PBLBallisticsTypes.h"

/**
 * Проникание в плотную мягкую среду (гель, вода, ткани) - модель Понселе:
 *   m * dv/ds = -A(s) * (a + Cd(s) * rho * v^2) / v
 * a - прочность среды (Па), rho - плотность, A - текущая площадь сечения пули, Cd - её коэффициент
 * сопротивления в среде (не воздушный). Состояние пули меняется по глубине:
 *   - экспансивная (JHP): если V входа > порога, диаметр линейно растёт до ExpandedDiameter за ExpansionDepth,
 *     Cd переходит к ExpandedCd, масса - к RetainedMassFraction;
 *   - неэкспансивная (FMJ): после YawOnsetDepth кувыркается - Cd переходит к YawedCd за ~5 см (площадь
 *     считаем по сечению, кувырок учтён в Cd).
 * Интегрирование по пути с шагом dS, детерминированное. Калибровка параметров - Content/Data/*.csv,
 * сверка - pbl.Ballistics.GelCheck против Reference_Gel.csv.
 */
struct FPBLPenetrationResult
{
	/** Пройденный путь в среде, м (до остановки или до выхода). */
	float Depth_m = 0.0f;
	/** Скорость на выходе, м/с (0 = остановилась внутри). */
	float ExitVelocity_mps = 0.0f;
	float FinalDiameter_m = 0.0f;
	float FinalMass_kg = 0.0f;
	float EnergyDeposited_J = 0.0f;
	bool bStopped = true;
	/** Глубина, где начался кувырок (для картинки канала), м; <0 = не было. */
	float YawDepth_m = -1.0f;
	bool bExpanded = false;
};

namespace PBLPenetration
{
	/** Скорость, ниже которой пуля считается остановленной, м/с. */
	constexpr float StopVelocity_mps = 1.0f;

	/** Проход через слой толщиной MaxThickness_m (0 = бесконечная среда). */
	PARABELLUM_API FPBLPenetrationResult Penetrate(const FPBLCartridgeData& C, const FPBLMaterialData& M, float V_in_mps,
		float MaxThickness_m = 0.0f, float dS_m = 0.0005f);
}
