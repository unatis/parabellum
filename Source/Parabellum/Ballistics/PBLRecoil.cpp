#include "Ballistics/PBLRecoil.h"

#include "Ballistics/PBLBallistics.h"
#include "Ballistics/PBLRecoilSettings.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

FPBLRecoilInfo PBLRecoil::Compute(const FPBLCartridgeData& C, const FPBLFirearmData& F, float V0_mps, const FPBLHoldParams& Hold)
{
	FPBLRecoilInfo R;
	R.Impulse_Ns = C.BulletMass_kg * V0_mps + C.PowderMass_kg * C.GasVelocity_mps;
	R.FreeVelocity_mps = F.Mass_kg > 0.0f ? R.Impulse_Ns / F.Mass_kg : 0.0f;
	R.FreeEnergy_J = 0.5f * F.Mass_kg * R.FreeVelocity_mps * R.FreeVelocity_mps;
	R.AngularImpulse_Nms = R.Impulse_Ns * F.BoreAbovePivot_m;
	R.Inertia_kgm2 = F.Mass_kg * F.CoMToPivot_m * F.CoMToPivot_m + Hold.BodyInertia_kgm2;
	R.Omega0_rad_s = R.Inertia_kgm2 > 0.0f ? R.AngularImpulse_Nms / R.Inertia_kgm2 : 0.0f;
	// Демпфированный осциллятор с начальной скоростью ω0 из нуля: θ(t) = ω0/ωd · e^{-ζωn t} · sin(ωd t).
	const float Wn = FMath::Sqrt(FMath::Max(Hold.Stiffness_Nm_per_rad, 1e-3f) / R.Inertia_kgm2);
	const float Zeta = FMath::Clamp(Hold.Damping_Nms_per_rad / (2.0f * FMath::Sqrt(Hold.Stiffness_Nm_per_rad * R.Inertia_kgm2)), 0.0f, 0.999f);
	const float Wd = Wn * FMath::Sqrt(1.0f - Zeta * Zeta);
	R.TimeToPeak_s = FMath::Atan2(Wd, Zeta * Wn) / Wd;
	R.PeakAngle_rad = R.Omega0_rad_s / Wd * FMath::Exp(-Zeta * Wn * R.TimeToPeak_s) * FMath::Sin(Wd * R.TimeToPeak_s);
	R.SettleTime_s = 3.0f / FMath::Max(Zeta * Wn, 1e-3f);
	return R;
}

void PBLRecoil::Kick(FPBLRecoilState& S, const FPBLRecoilInfo& Info, const FPBLHoldParams& Hold, float YawRandom)
{
	S.PitchVel += Info.Omega0_rad_s;
	const float YawFactor = Hold.YawFraction * YawRandom + Hold.YawBias;
	S.YawVel += Info.Omega0_rad_s * YawFactor;
	// Остаток, который стрелок не возвращает: сдвиг равновесия (игрок компенсирует мышью, как в CS).
	S.PitchRest += Info.PeakAngle_rad * Hold.ResidualFraction;
	S.YawRest += Info.PeakAngle_rad * Hold.ResidualFraction * YawFactor;
	S.VisualKick_cm += Info.FreeVelocity_mps * UPBLRecoilSettings::Get().VisualKick_cm_per_mps;
}

void PBLRecoil::Step(FPBLRecoilState& S, const FPBLRecoilInfo& Info, const FPBLHoldParams& Hold, float Dt, float& OutDPitch, float& OutDYaw)
{
	const float P0 = S.Pitch, Y0 = S.Yaw;
	const float I = FMath::Max(Info.Inertia_kgm2, 1e-4f);
	const int32 N = FMath::Max(1, FMath::CeilToInt(Dt / 0.002f));
	const float h = Dt / N;
	for (int32 i = 0; i < N; ++i)
	{
		// Полунеявный Эйлер: устойчив для жёсткой пружины при малом шаге.
		S.PitchVel += (-Hold.Stiffness_Nm_per_rad * (S.Pitch - S.PitchRest) - Hold.Damping_Nms_per_rad * S.PitchVel) / I * h;
		S.Pitch += S.PitchVel * h;
		S.YawVel += (-Hold.Stiffness_Nm_per_rad * (S.Yaw - S.YawRest) - Hold.Damping_Nms_per_rad * S.YawVel) / I * h;
		S.Yaw += S.YawVel * h;
	}
	S.VisualKick_cm *= FMath::Exp(-Dt / FMath::Max(UPBLRecoilSettings::Get().VisualKickDecay_s, 1e-3f));
	OutDPitch = S.Pitch - P0;
	OutDYaw = S.Yaw - Y0;
}

static FAutoConsoleCommandWithWorldAndArgs CmdRecoil(
	TEXT("pbl.Ballistics.Recoil"),
	TEXT("Recoil figures for a firearm (free recoil vs public calculators, muzzle rise model): pbl.Ballistics.Recoil <firearm> [cartridge]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !World || !World->GetGameInstance()) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Ballistics.Recoil <firearm> [cartridge]")); return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		const FPBLFirearmData* F = Data ? Data->FindFirearm(FName(*Args[0])) : nullptr;
		if (!F) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Recoil: firearm '%s' not found"), *Args[0]); return; }
		const FPBLCartridgeData* C = Data->FindCartridge(Args.Num() > 1 ? FName(*Args[1]) : F->Cartridge);
		if (!C) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Recoil: cartridge not found")); return; }
		const FPBLHoldParams& Hold = UPBLRecoilSettings::Get().Hold(F->Hold);
		const float V0 = PBLBallistics::MuzzleVelocity(*C, F->Barrel_m);
		const FPBLRecoilInfo R = PBLRecoil::Compute(*C, *F, V0, Hold);
		UE_LOG(LogTemp, Display, TEXT("RECOIL %s / %s: bullet %.2f g @ %.0f m/s (%.0f fps), powder %.2f g (%.1f gr) @ %.0f m/s, firearm %.3f kg (%.2f lb)"),
			*F->Name.ToString(), *C->Name.ToString(), C->BulletMass_kg * 1000.0f, V0, V0 / 0.3048f, C->PowderMass_kg * 1000.0f, C->PowderMass_kg / 0.00006479891f,
			C->GasVelocity_mps, F->Mass_kg, F->Mass_kg / 0.45359237f);
		UE_LOG(LogTemp, Display, TEXT("RECOIL  impulse %.2f N*s | free recoil velocity %.2f m/s (%.1f fps) | free recoil energy %.2f J (%.2f ft*lbf)"),
			R.Impulse_Ns, R.FreeVelocity_mps, R.FreeVelocity_mps / 0.3048f, R.FreeEnergy_J, R.FreeEnergy_J / 1.3558f);
		UE_LOG(LogTemp, Display, TEXT("RECOIL  hold %s: h %.0f mm, I %.4f kg*m^2, k %.0f N*m/rad, c %.2f | L %.3f N*m*s, omega0 %.2f rad/s | muzzle rise peak %.2f deg at %.0f ms, settle ~%.0f ms, residual %.2f deg"),
			*F->Hold.ToString(), F->BoreAbovePivot_m * 1000.0f, R.Inertia_kgm2, Hold.Stiffness_Nm_per_rad, Hold.Damping_Nms_per_rad, R.AngularImpulse_Nms, R.Omega0_rad_s,
			FMath::RadiansToDegrees(R.PeakAngle_rad), R.TimeToPeak_s * 1000.0f, R.SettleTime_s * 1000.0f, FMath::RadiansToDegrees(R.PeakAngle_rad * Hold.ResidualFraction));
	}));
