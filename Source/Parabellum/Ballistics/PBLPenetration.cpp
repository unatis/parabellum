#include "Ballistics/PBLPenetration.h"

#include "Ballistics/PBLBallistics.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

FPBLPenetrationResult PBLPenetration::Penetrate(const FPBLCartridgeData& C, const FPBLMaterialData& M, float V_in_mps, float MaxThickness_m, float dS_m)
{
	FPBLPenetrationResult R;
	const float Rho = M.Density_kgm3;
	const float a = M.Strength_Pa;
	const float D0 = C.Diameter_m;
	const float M0 = C.BulletMass_kg;
	const bool bExpands = C.ExpansionThresholdV_mps > 0.0f && C.ExpandedDiameter_m > D0 && V_in_mps > C.ExpansionThresholdV_mps;
	const bool bYaws = !bExpands && C.YawOnsetDepth_m > 0.0f;
	constexpr float YawTransition_m = 0.05f;

	float V = V_in_mps;
	float S = 0.0f;
	float Dia = D0, Cd = C.MediumCd, Mass = M0;
	R.bExpanded = bExpands;
	int32 Guard = 0;
	while (V > StopVelocity_mps && ++Guard < 200000)
	{
		if (bExpands)
		{
			const float F = FMath::Clamp(S / FMath::Max(C.ExpansionDepth_m, 1e-4f), 0.0f, 1.0f);
			Dia = FMath::Lerp(D0, C.ExpandedDiameter_m, F);
			Cd = FMath::Lerp(C.MediumCd, C.ExpandedCd, F);
			Mass = M0 * FMath::Lerp(1.0f, C.RetainedMassFraction, F);
		}
		else if (bYaws && S > C.YawOnsetDepth_m)
		{
			if (R.YawDepth_m < 0.0f) { R.YawDepth_m = S; }
			const float F = FMath::Clamp((S - C.YawOnsetDepth_m) / YawTransition_m, 0.0f, 1.0f);
			Cd = FMath::Lerp(C.MediumCd, C.YawedCd, F);
		}
		const float A = PI * 0.25f * Dia * Dia;
		const float dV = -A * (a + Cd * Rho * V * V) / (Mass * V) * dS_m;
		V = FMath::Max(V + dV, 0.0f);
		S += dS_m;
		if (MaxThickness_m > 0.0f && S >= MaxThickness_m) { break; }
	}
	R.Depth_m = (MaxThickness_m > 0.0f) ? FMath::Min(S, MaxThickness_m) : S;
	R.bStopped = V <= StopVelocity_mps;
	R.ExitVelocity_mps = R.bStopped ? 0.0f : V;
	R.FinalDiameter_m = Dia;
	R.FinalMass_kg = Mass;
	R.EnergyDeposited_J = 0.5f * M0 * V_in_mps * V_in_mps - 0.5f * Mass * R.ExitVelocity_mps * R.ExitVelocity_mps;
	return R;
}

// ---------------------------------------------------------------------------------------------------------------------
// Консоль: pbl.Ballistics.Gel <firearm|cartridge> [material] [V_mps]  - кривая проникания
//          pbl.Ballistics.GelCheck                                    - сверка всех эталонов Reference_Gel.csv

static const FPBLCartridgeData* ResolveCartridge(UPBLWeaponDataSubsystem* Data, const FString& Name, float& OutV)
{
	if (const FPBLFirearmData* F = Data->FindFirearm(FName(*Name)))
	{
		if (const FPBLCartridgeData* C = Data->FindCartridge(F->Cartridge)) { OutV = PBLBallistics::MuzzleVelocity(*C, F->Barrel_m); return C; }
		return nullptr;
	}
	if (const FPBLCartridgeData* C = Data->FindCartridge(FName(*Name))) { OutV = C->V0_mps; return C; }
	return nullptr;
}

static FAutoConsoleCommandWithWorldAndArgs CmdGel(
	TEXT("pbl.Ballistics.Gel"),
	TEXT("Penetration curve in a soft medium: pbl.Ballistics.Gel <firearm|cartridge> [material=Gel10] [V_mps]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !World || !World->GetGameInstance()) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Ballistics.Gel <firearm|cartridge> [material] [V]")); return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		if (!Data) { return; }
		float V = 0.0f;
		const FPBLCartridgeData* C = ResolveCartridge(Data, Args[0], V);
		if (!C) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Gel: '%s' not found"), *Args[0]); return; }
		const FPBLMaterialData* M = Data->FindMaterial(FName(Args.Num() > 1 ? *Args[1] : TEXT("Gel10")));
		if (!M) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Gel: material not found")); return; }
		if (Args.Num() > 2) { V = FCString::Atof(*Args[2]); }

		const FPBLPenetrationResult Full = PBLPenetration::Penetrate(*C, *M, V);
		UE_LOG(LogTemp, Display, TEXT("GEL %s in %s (rho %.0f, a %.0f Pa): V_in %.1f m/s, %s, mass %.2f g, d %.2f mm"),
			*C->Name.ToString(), *M->Name.ToString(), M->Density_kgm3, M->Strength_Pa, V, *C->Construction.ToString(), C->BulletMass_kg * 1000.0f, C->Diameter_m * 1000.0f);
		UE_LOG(LogTemp, Display, TEXT("GEL  depth_cm   vel_mps   dia_mm"));
		for (float L = 0.05f; L < Full.Depth_m; L += 0.05f)
		{
			const FPBLPenetrationResult P = PBLPenetration::Penetrate(*C, *M, V, L);
			UE_LOG(LogTemp, Display, TEXT("GEL  %8.1f  %8.1f  %7.2f"), L * 100.0f, P.ExitVelocity_mps, P.FinalDiameter_m * 1000.0f);
		}
		UE_LOG(LogTemp, Display, TEXT("GEL  TOTAL %.1f cm (%.1f in), final dia %.2f mm, %s%s, E deposited %.0f J"),
			Full.Depth_m * 100.0f, Full.Depth_m / 0.0254f, Full.FinalDiameter_m * 1000.0f,
			Full.bExpanded ? TEXT("expanded") : TEXT("intact"),
			Full.YawDepth_m >= 0.0f ? *FString::Printf(TEXT(", yaw at %.1f cm"), Full.YawDepth_m * 100.0f) : TEXT(""), Full.EnergyDeposited_J);
	}));

static FAutoConsoleCommandWithWorld CmdGelCheck(
	TEXT("pbl.Ballistics.GelCheck"),
	TEXT("Compare model penetration with Content/Data/Reference_Gel.csv (acceptance: within 5%)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World || !World->GetGameInstance()) { return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		if (!Data) { return; }
		int32 Pass = 0, Total = 0;
		UE_LOG(LogTemp, Display, TEXT("GELCHECK  cartridge         material  V_mps  depth_model_cm  depth_ref_cm  err%%   dia_model  dia_ref  verdict"));
		for (const FPBLGelReference& G : Data->GelReferences())
		{
			const FPBLCartridgeData* C = Data->FindCartridge(G.Cartridge);
			const FPBLMaterialData* M = Data->FindMaterial(G.Material);
			if (!C || !M) { UE_LOG(LogTemp, Warning, TEXT("GELCHECK  %s / %s: data missing"), *G.Cartridge.ToString(), *G.Material.ToString()); continue; }
			const FPBLPenetrationResult P = PBLPenetration::Penetrate(*C, *M, G.V_mps);
			const float Err = (P.Depth_m - G.Depth_m) / G.Depth_m * 100.0f;
			const bool bOk = FMath::Abs(Err) <= 5.0f;
			++Total; Pass += bOk ? 1 : 0;
			UE_LOG(LogTemp, Display, TEXT("GELCHECK  %-16s  %-8s  %5.0f  %14.1f  %12.1f  %+5.1f  %9.1f  %7.1f  %s"),
				*G.Cartridge.ToString(), *G.Material.ToString(), G.V_mps, P.Depth_m * 100.0f, G.Depth_m * 100.0f, Err,
				P.FinalDiameter_m * 1000.0f, G.ExpandedDiameter_m * 1000.0f, bOk ? TEXT("OK") : TEXT("FAIL"));
		}
		UE_LOG(LogTemp, Display, TEXT("GELCHECK  %d/%d within 5%%"), Pass, Total);
	}));
