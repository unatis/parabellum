#pragma once

#include "CoreMinimal.h"
#include "Ballistics/PBLBallisticsTypes.h"

struct FPBLHoldParams;

/** Статические величины отдачи одного выстрела - для консоли/HUD/сверки. */
struct FPBLRecoilInfo
{
	float Impulse_Ns = 0.0f;          // p
	float FreeVelocity_mps = 0.0f;    // p / m_оружия
	float FreeEnergy_J = 0.0f;        // ½ m v²
	float AngularImpulse_Nms = 0.0f;  // L = p·h
	float Inertia_kgm2 = 0.0f;        // I = m·d² + I_тела
	float Omega0_rad_s = 0.0f;        // L / I
	float PeakAngle_rad = 0.0f;       // оценка пика подброса (демпфированная пружина)
	float TimeToPeak_s = 0.0f;
	float SettleTime_s = 0.0f;        // до ~5 % от пика
};

/** Состояние пружины хвата (по тангажу и курсу), интегрируется каждый кадр у владельца. */
struct FPBLRecoilState
{
	float Pitch = 0.0f, PitchVel = 0.0f, PitchRest = 0.0f;
	float Yaw = 0.0f, YawVel = 0.0f, YawRest = 0.0f;
	float VisualKick_cm = 0.0f;
	bool IsActive() const { return FMath::Abs(PitchVel) > 1e-4f || FMath::Abs(YawVel) > 1e-4f || FMath::Abs(Pitch - PitchRest) > 1e-5f || FMath::Abs(Yaw - YawRest) > 1e-5f || VisualKick_cm > 1e-3f; }
};

namespace PBLRecoil
{
	PARABELLUM_API FPBLRecoilInfo Compute(const FPBLCartridgeData& C, const FPBLFirearmData& F, float V0_mps, const FPBLHoldParams& Hold);
	/** Выстрел: добавить угловую скорость (и сдвиг равновесия) в состояние. YawSign - случайный знак/доля [-1..1]. */
	PARABELLUM_API void Kick(FPBLRecoilState& S, const FPBLRecoilInfo& Info, const FPBLHoldParams& Hold, float YawRandom);
	/** Шаг пружины; возвращает приращение углов (рад) за шаг - его применяют к камере. */
	PARABELLUM_API void Step(FPBLRecoilState& S, const FPBLRecoilInfo& Info, const FPBLHoldParams& Hold, float Dt, float& OutDPitch, float& OutDYaw);
}
