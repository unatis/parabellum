#pragma once

#include "CoreMinimal.h"
#include "Ballistics/PBLBallisticsTypes.h"

/**
 * Цикл автоматики: движение подвижных частей считается из того же импульса выстрела, что и отдача,
 * а не задаётся анимацией. Для короткого хода ствола с перекосом (схема Браунинга):
 *
 *   начальная скорость затвора   v0 = p / m_оружия   (свободная скорость отдачи: в запертом положении
 *                                                     ствол, затвор и рамка идут вместе)
 *   откат                        m*dv/dt = -(F0 + k*x) - потерь на трение
 *   отпирание                    после UnlockTravel ствол опускается на BarrelTilt и останавливается
 *   накат                        пружина возвращает затвор вперёд
 *
 * Отсюда получаются ход затвора, время цикла и предельный темп стрельбы - величины, которые можно
 * сверять с измеренными. Всё в СИ.
 */
UENUM()
enum class EPBLCyclePhase : uint8
{
	Ready,        // затвор в переднем положении
	Recoiling,    // откат
	AtRear,       // дошёл до заднего положения (упор)
	Returning     // накат
};

struct FPBLCycleState
{
	EPBLCyclePhase Phase = EPBLCyclePhase::Ready;
	/** Смещение затвора назад, м. */
	float X = 0.0f;
	/** Скорость затвора, м/с (назад - положительная). */
	float V = 0.0f;
	/** Время от выстрела, с. */
	float T = 0.0f;
	/** Доля перекоса ствола, 0 заперт, 1 полностью опущен. */
	float BarrelTiltAlpha = 0.0f;
	/** Замеры за цикл - для отчёта. */
	float PeakV = 0.0f;
	float MaxX = 0.0f;
	float CycleTime = 0.0f;
	bool IsRunning() const { return Phase != EPBLCyclePhase::Ready; }
};

namespace PBLCycle
{
	/** Начальная скорость подвижных частей из импульса выстрела, м/с. */
	PARABELLUM_API float InitialSlideVelocity(const FPBLFirearmData& F, float Impulse_Ns);

	/** Выстрел: перевести цикл в откат. */
	PARABELLUM_API void Fire(FPBLCycleState& S, const FPBLFirearmData& F, float Impulse_Ns);

	/** Шаг интегрирования (подшаги внутри). */
	PARABELLUM_API void Step(FPBLCycleState& S, const FPBLFirearmData& F, float Dt);

	/** Расчёт цикла целиком, без отрисовки: ход, время отката, время цикла, предельный темп. */
	PARABELLUM_API void Predict(const FPBLFirearmData& F, float Impulse_Ns, float& OutTravel_m,
		float& OutRecoilTime_s, float& OutCycleTime_s, float& OutMaxRPM);
}
