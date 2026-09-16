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
	const float CdScale = M.MediumCdScale > 0.0f ? M.MediumCdScale : 1.0f;
	float Dia = D0, Cd = C.MediumCd * CdScale, Mass = M0;
	R.bExpanded = bExpands;
	int32 Guard = 0;
	while (V > StopVelocity_mps && ++Guard < 200000)
	{
		if (bExpands)
		{
			const float F = FMath::Clamp(S / FMath::Max(C.ExpansionDepth_m, 1e-4f), 0.0f, 1.0f);
			Dia = FMath::Lerp(D0, C.ExpandedDiameter_m, F);
			Cd = FMath::Lerp(C.MediumCd, C.ExpandedCd, F) * CdScale;
			Mass = M0 * FMath::Lerp(1.0f, C.RetainedMassFraction, F);
		}
		else if (bYaws && S > C.YawOnsetDepth_m)
		{
			if (R.YawDepth_m < 0.0f) { R.YawDepth_m = S; }
			const float F = FMath::Clamp((S - C.YawOnsetDepth_m) / YawTransition_m, 0.0f, 1.0f);
			Cd = FMath::Lerp(C.MediumCd, C.YawedCd, F) * CdScale;
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

float PBLPenetration::ThorResidualVelocity(const FPBLCartridgeData& C, const FPBLMaterialData& M, float V_in_mps, float Thickness_m, float AngleFromNormal_rad)
{
	// Имперские единицы THOR: h [in], A [in^2], m [gr], V [fps].
	const float h_in = Thickness_m / 0.0254f;
	const float A_in2 = C.FrontalArea_m2() / (0.0254f * 0.0254f);
	const float m_gr = C.BulletMass_kg / 0.00006479891f;
	const float V_fps = V_in_mps / 0.3048f;
	const float Sec = 1.0f / FMath::Max(FMath::Cos(AngleFromNormal_rad), 0.05f);
	const float Loss = FMath::Pow(10.0f, M.Thor_c) * FMath::Pow(h_in * A_in2, M.Thor_alpha) * FMath::Pow(m_gr, M.Thor_beta) * FMath::Pow(Sec, M.Thor_gamma) * FMath::Pow(V_fps, M.Thor_lambda);
	const float Vr_fps = V_fps - Loss;
	return Vr_fps > 0.0f ? Vr_fps * 0.3048f : 0.0f;
}

FPBLPenetrationResult PBLPenetration::PassLayer(const FPBLCartridgeData& C, const FPBLMaterialData& M, float V_in_mps, float Thickness_m, float AngleFromNormal_rad)
{
	const float Path_m = Thickness_m / FMath::Max(FMath::Cos(AngleFromNormal_rad), 0.05f);
	if (M.IsThor())
	{
		FPBLPenetrationResult R;
		R.ExitVelocity_mps = ThorResidualVelocity(C, M, V_in_mps, Thickness_m, AngleFromNormal_rad);
		R.bStopped = R.ExitVelocity_mps <= 0.0f;
		R.Depth_m = R.bStopped ? FMath::Min(Path_m, 0.5f * Path_m) : Path_m;   // в непробитой плите глубина условна
		R.FinalDiameter_m = C.Diameter_m;
		R.FinalMass_kg = C.BulletMass_kg;
		R.EnergyDeposited_J = 0.5f * C.BulletMass_kg * (V_in_mps * V_in_mps - R.ExitVelocity_mps * R.ExitVelocity_mps);
		return R;
	}
	return Penetrate(C, M, V_in_mps, Path_m);
}

TArray<PBLPenetration::FResolvedLayer> PBLPenetration::ResolveBodyStack(const TArray<FPBLBodyLayer>& Layers, const TMap<FName, FPBLMaterialData>& Materials,
	float PathThickness_m, float ZFromCenter_m, float LateralFromAxis_m)
{
	TArray<FResolvedLayer> Out;
	float Fixed = 0.0f; int32 FillIdx = -1;
	for (const FPBLBodyLayer& L : Layers)
	{
		FResolvedLayer R;
		R.Material = Materials.Find(L.Material);
		R.Name = L.Material;
		if (!R.Material) { continue; }
		bool bPresent = true;
		if (L.Coverage == EPBLLayerCoverage::BandsZ && L.P1 > 0.0f)
		{
			const float Phase = FMath::Fmod(FMath::Abs(ZFromCenter_m), L.P1);
			bPresent = Phase < L.P2;
		}
		else if (L.Coverage == EPBLLayerCoverage::Core)
		{
			bPresent = FMath::Abs(LateralFromAxis_m) < L.P1;
		}
		if (!bPresent) { continue; }
		R.Thickness_m = L.Thickness_m;
		if (L.Thickness_m <= 0.0f) { FillIdx = Out.Num(); } else { Fixed += L.Thickness_m; }
		Out.Add(R);
	}
	// Заполняющий слой берёт остаток; если фиксированные слои толще геометрии - масштабируем все.
	if (FillIdx >= 0) { Out[FillIdx].Thickness_m = FMath::Max(PathThickness_m - Fixed, 0.001f); }
	else if (Fixed > PathThickness_m && Fixed > 0.0f) { for (FResolvedLayer& R : Out) { R.Thickness_m *= PathThickness_m / Fixed; } }
	return Out;
}

void PBLPenetration::PassBody(const FPBLCartridgeData& C, const TArray<FResolvedLayer>& Stack, float V_in_mps, TArray<FLayerPass>& Out)
{
	float V = V_in_mps;
	for (const FResolvedLayer& L : Stack)
	{
		if (V <= StopVelocity_mps) { break; }
		FLayerPass P;
		P.Material = L.Name; P.Thickness_m = L.Thickness_m; P.V_in = V;
		const FPBLPenetrationResult R = Penetrate(C, *L.Material, V, L.Thickness_m);
		P.V_out = R.ExitVelocity_mps; P.Depth_m = R.Depth_m; P.bStopped = R.bStopped; P.FinalDiameter_m = R.FinalDiameter_m;
		Out.Add(P);
		V = R.ExitVelocity_mps;
		if (R.bStopped) { break; }
	}
}

float PBLPenetration::WoundDamage(const FPBLCartridgeData& C, const TArray<FLayerPass>& Passes, const TMap<FName, FPBLMaterialData>& Materials, float& OutEnergy_J)
{
	float Dmg = 0.0f; OutEnergy_J = 0.0f;
	for (const FLayerPass& P : Passes)
	{
		const float E = 0.5f * C.BulletMass_kg * (P.V_in * P.V_in - P.V_out * P.V_out);
		OutEnergy_J += E;
		if (const FPBLMaterialData* M = Materials.Find(P.Material)) { Dmg += E * M->WoundWeight; }
	}
	return Dmg;
}

bool PBLPenetration::ShouldRicochet(const FPBLMaterialData& M, float AngleFromNormal_rad, bool bPerforated)
{
	if (bPerforated || M.RicochetAngleDeg <= 0.0f) { return false; }
	const float AngleToSurfaceDeg = 90.0f - FMath::RadiansToDegrees(FMath::Abs(AngleFromNormal_rad));
	return AngleToSurfaceDeg <= M.RicochetAngleDeg;
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

// pbl.Ballistics.Layer <firearm|cartridge> <material> <thickness_mm> [angle_from_normal_deg] [V_mps] - остаточная скорость за слоем
static FAutoConsoleCommandWithWorldAndArgs CmdLayer(
	TEXT("pbl.Ballistics.Layer"),
	TEXT("Residual velocity behind a layer: pbl.Ballistics.Layer <firearm|cartridge> <material> <thickness_mm> [angle_deg] [V_mps]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3 || !World || !World->GetGameInstance()) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Ballistics.Layer <firearm|cartridge> <material> <thickness_mm> [angle] [V]")); return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		if (!Data) { return; }
		float V = 0.0f;
		const FPBLCartridgeData* C = ResolveCartridge(Data, Args[0], V);
		const FPBLMaterialData* M = Data->FindMaterial(FName(*Args[1]));
		if (!C || !M) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Layer: cartridge or material not found")); return; }
		const float Th = FCString::Atof(*Args[2]) / 1000.0f;
		const float Ang = Args.Num() > 3 ? FMath::DegreesToRadians(FCString::Atof(*Args[3])) : 0.0f;
		if (Args.Num() > 4) { V = FCString::Atof(*Args[4]); }
		const FPBLPenetrationResult R = PBLPenetration::PassLayer(*C, *M, V, Th, Ang);
		UE_LOG(LogTemp, Display, TEXT("LAYER %s -> %s %.1f mm @ %.0f deg: V_in %.1f m/s (%.0f fps) -> %s, E deposited %.0f J%s"),
			*C->Name.ToString(), *M->Name.ToString(), Th * 1000.0f, FMath::RadiansToDegrees(Ang), V, V / 0.3048f,
			R.bStopped ? *FString::Printf(TEXT("STOPPED (depth %.1f mm)"), R.Depth_m * 1000.0f) : *FString::Printf(TEXT("EXIT %.1f m/s (%.0f fps)"), R.ExitVelocity_mps, R.ExitVelocity_mps / 0.3048f),
			R.EnergyDeposited_J, PBLPenetration::ShouldRicochet(*M, Ang, !R.bStopped) ? TEXT(", RICOCHET") : TEXT(""));
	}));

// pbl.Ballistics.Body <firearm|cartridge> <part> <thickness_mm> [z_from_center_cm] [lateral_cm] [V_mps] - прогон по слоям части тела
static FAutoConsoleCommandWithWorldAndArgs CmdBody(
	TEXT("pbl.Ballistics.Body"),
	TEXT("Pass through a body part layer stack: pbl.Ballistics.Body <firearm|cartridge> <part> <thickness_mm> [z_cm] [lateral_cm] [V_mps]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3 || !World || !World->GetGameInstance()) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Ballistics.Body <firearm|cartridge> <part> <thickness_mm> [z_cm] [lateral_cm] [V]")); return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		if (!Data) { return; }
		float V = 0.0f;
		const FPBLCartridgeData* C = ResolveCartridge(Data, Args[0], V);
		const TArray<FPBLBodyLayer>* Layers = Data->FindBodyPart(FName(*Args[1]));
		if (!C || !Layers) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Body: cartridge or body part not found")); return; }
		const float Th = FCString::Atof(*Args[2]) / 1000.0f;
		const float Z = Args.Num() > 3 ? FCString::Atof(*Args[3]) / 100.0f : 0.0f;
		const float Lat = Args.Num() > 4 ? FCString::Atof(*Args[4]) / 100.0f : 0.0f;
		if (Args.Num() > 5) { V = FCString::Atof(*Args[5]); }
		const TArray<PBLPenetration::FResolvedLayer> Stack = PBLPenetration::ResolveBodyStack(*Layers, Data->AllMaterials(), Th, Z, Lat);
		TArray<PBLPenetration::FLayerPass> Passes;
		PBLPenetration::PassBody(*C, Stack, V, Passes);
		UE_LOG(LogTemp, Display, TEXT("BODY %s -> %s %.0f mm (z %+.0f cm, lateral %.0f cm): V_in %.1f m/s"), *C->Name.ToString(), *Args[1], Th * 1000.0f, Z * 100.0f, Lat * 100.0f, V);
		float Total = 0.0f;
		for (const auto& P : Passes)
		{
			Total += P.Depth_m;
			UE_LOG(LogTemp, Display, TEXT("BODY   %-13s %6.1f mm  %6.1f -> %s"), *P.Material.ToString(), P.Thickness_m * 1000.0f, P.V_in,
				P.bStopped ? *FString::Printf(TEXT("STOPPED at %.1f mm into layer"), P.Depth_m * 1000.0f) : *FString::Printf(TEXT("%6.1f m/s"), P.V_out));
		}
		const bool bExit = Passes.Num() > 0 && !Passes.Last().bStopped;
		UE_LOG(LogTemp, Display, TEXT("BODY   total path %.1f mm, %s"), Total * 1000.0f, bExit ? *FString::Printf(TEXT("EXIT %.1f m/s"), Passes.Last().V_out) : TEXT("STOPPED"));
	}));

// pbl.Ballistics.Wound <firearm|cartridge> <part> <thickness_mm> [z_cm] [lateral_cm] [V_mps] - урон по слоям части тела
static FAutoConsoleCommandWithWorldAndArgs CmdWound(
	TEXT("pbl.Ballistics.Wound"),
	TEXT("Wound damage through a body part: pbl.Ballistics.Wound <firearm|cartridge> <part> <thickness_mm> [z_cm] [lateral_cm] [V_mps]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3 || !World || !World->GetGameInstance()) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Ballistics.Wound <firearm|cartridge> <part> <thickness_mm> [z] [lat] [V]")); return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		if (!Data) { return; }
		float V = 0.0f;
		const FPBLCartridgeData* C = ResolveCartridge(Data, Args[0], V);
		const TArray<FPBLBodyLayer>* Layers = Data->FindBodyPart(FName(*Args[1]));
		if (!C || !Layers) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Wound: cartridge or body part not found")); return; }
		const float Th = FCString::Atof(*Args[2]) / 1000.0f;
		const float Z = Args.Num() > 3 ? FCString::Atof(*Args[3]) / 100.0f : 0.0f;
		const float Lat = Args.Num() > 4 ? FCString::Atof(*Args[4]) / 100.0f : 0.0f;
		if (Args.Num() > 5) { V = FCString::Atof(*Args[5]); }
		TArray<PBLPenetration::FLayerPass> Passes;
		PBLPenetration::PassBody(*C, PBLPenetration::ResolveBodyStack(*Layers, Data->AllMaterials(), Th, Z, Lat), V, Passes);
		float E = 0.0f;
		const float Dmg = PBLPenetration::WoundDamage(*C, Passes, Data->AllMaterials(), E);
		FString Detail;
		for (const auto& P : Passes)
		{
			const float Ei = 0.5f * C->BulletMass_kg * (P.V_in * P.V_in - P.V_out * P.V_out);
			const FPBLMaterialData* M = Data->FindMaterial(P.Material);
			Detail += FString::Printf(TEXT(" %s %.0fJ(%.1f)"), *P.Material.ToString(), Ei, Ei * (M ? M->WoundWeight : 0.0f));
		}
		UE_LOG(LogTemp, Display, TEXT("WOUND %s -> %s %.0f mm @ %.0f m/s: damage %.1f of E %.0f J deposited, %s |%s"), *C->Name.ToString(), *Args[1], Th * 1000.0f, V, Dmg, E,
			(Passes.Num() && !Passes.Last().bStopped) ? *FString::Printf(TEXT("EXIT %.0f m/s"), Passes.Last().V_out) : TEXT("STOPPED"), *Detail);
	}));
