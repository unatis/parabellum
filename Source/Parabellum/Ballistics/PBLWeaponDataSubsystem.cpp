#include "Ballistics/PBLWeaponDataSubsystem.h"

#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UPBLWeaponDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reload();
}

// Простой CSV: первая строка - заголовок; поля в кавычках могут содержать запятые.
bool UPBLWeaponDataSubsystem::ReadCsv(const FString& Path, TArray<TMap<FString, FString>>& OutRows)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path)) { return false; }
	auto Split = [](const FString& Line)
	{
		TArray<FString> Fields; FString Cur; bool bQuoted = false;
		for (TCHAR Ch : Line)
		{
			if (Ch == TEXT('"')) { bQuoted = !bQuoted; }
			else if (Ch == TEXT(',') && !bQuoted) { Fields.Add(Cur); Cur.Reset(); }
			else { Cur.AppendChar(Ch); }
		}
		Fields.Add(Cur);
		for (FString& F : Fields) { F.TrimStartAndEndInline(); }
		return Fields;
	};
	TArray<FString> Header;
	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#"))) { continue; }
		if (Header.IsEmpty()) { Header = Split(Line); continue; }
		const TArray<FString> Fields = Split(Line);
		TMap<FString, FString> Row;
		for (int32 i = 0; i < Header.Num() && i < Fields.Num(); ++i) { Row.Add(Header[i], Fields[i]); }
		OutRows.Add(MoveTemp(Row));
	}
	return true;
}

void UPBLWeaponDataSubsystem::Reload()
{
	Cartridges.Reset();
	Firearms.Reset();
	Materials.Reset();
	References.Reset();
	BodyParts.Reset();
	BodyPartThickness_m.Reset();
	PartSteps_.Reset();
	auto Num = [](const TMap<FString, FString>& R, const TCHAR* Key, float Default = 0.0f) { const FString* V = R.Find(Key); return (V && !V->IsEmpty()) ? FCString::Atof(**V) : Default; };
	const FString Dir = FPaths::ProjectContentDir() / TEXT("Data");

	TArray<TMap<FString, FString>> Rows;
	if (ReadCsv(Dir / TEXT("Cartridges.csv"), Rows))
	{
		for (const auto& R : Rows)
		{
			FPBLCartridgeData C;
			C.Name = FName(*R.FindRef(TEXT("Name")));
			C.BulletMass_kg = FCString::Atof(*R.FindRef(TEXT("BulletMass_g"))) / 1000.0f;
			C.Diameter_m = FCString::Atof(*R.FindRef(TEXT("Diameter_mm"))) / 1000.0f;
			C.V0_mps = FCString::Atof(*R.FindRef(TEXT("V0_mps")));
			C.RefBarrel_m = FCString::Atof(*R.FindRef(TEXT("RefBarrel_mm"))) / 1000.0f;
			C.dV_per_25mm_mps = FCString::Atof(*R.FindRef(TEXT("dV_per_25mm_mps")));
			C.DragModel = R.FindRef(TEXT("DragModel")).Equals(TEXT("G7"), ESearchCase::IgnoreCase) ? EPBLDragModel::G7 : EPBLDragModel::G1;
			C.BC = FCString::Atof(*R.FindRef(TEXT("BC")));
			C.Construction = FName(*R.FindRef(TEXT("Construction")));
			C.Length_m = Num(R, TEXT("Length_mm"), 15.0f) / 1000.0f;
			C.PowderMass_kg = Num(R, TEXT("PowderMass_g"), 0.4f) / 1000.0f;
			C.GasVelocity_mps = Num(R, TEXT("GasVelocity_mps"), 1430.0f);
			C.MediumCd = Num(R, TEXT("MediumCd"), 0.2f);
			C.YawOnsetDepth_m = Num(R, TEXT("YawOnsetDepth_mm")) / 1000.0f;
			C.YawedCd = Num(R, TEXT("YawedCd"), 0.7f);
			C.ExpansionThresholdV_mps = Num(R, TEXT("ExpansionThresholdV_mps"));
			C.ExpandedDiameter_m = Num(R, TEXT("ExpandedDiameter_mm")) / 1000.0f;
			C.ExpandedCd = Num(R, TEXT("ExpandedCd"), 0.3f);
			C.ExpansionDepth_m = Num(R, TEXT("ExpansionDepth_mm"), 25.0f) / 1000.0f;
			C.RetainedMassFraction = Num(R, TEXT("RetainedMassFraction"), 1.0f);
			if (!C.Name.IsNone()) { Cartridges.Add(C.Name, C); }
		}
	}
	else { UE_LOG(LogTemp, Warning, TEXT("PBL Data: %s не прочитан"), *(Dir / TEXT("Cartridges.csv"))); }

	Rows.Reset();
	if (ReadCsv(Dir / TEXT("Firearms.csv"), Rows))
	{
		for (const auto& R : Rows)
		{
			FPBLFirearmData F;
			F.Name = FName(*R.FindRef(TEXT("Name")));
			F.Cartridge = FName(*R.FindRef(TEXT("Cartridge")));
			F.Barrel_m = FCString::Atof(*R.FindRef(TEXT("Barrel_mm"))) / 1000.0f;
			F.RPM = FCString::Atoi(*R.FindRef(TEXT("RPM")));
			F.Action = FName(*R.FindRef(TEXT("Action")));
			F.Mass_kg = FCString::Atof(*R.FindRef(TEXT("Mass_kg")));
			F.OverallLength_m = Num(R, TEXT("OverallLength_mm")) / 1000.0f;
			F.MagSize = FCString::Atoi(*R.FindRef(TEXT("MagSize")));
			F.SightHeight_m = FCString::Atof(*R.FindRef(TEXT("SightHeight_mm"))) / 1000.0f;
			F.ZeroRange_m = FCString::Atof(*R.FindRef(TEXT("ZeroRange_m")));
			F.Dispersion_MOA = FCString::Atof(*R.FindRef(TEXT("Dispersion_MOA")));
			{ const FString H = R.FindRef(TEXT("Hold")); if (!H.IsEmpty()) { F.Hold = FName(*H); } }
			F.BoreAbovePivot_m = Num(R, TEXT("BoreAbovePivot_mm"), 75.0f) / 1000.0f;
			F.CoMToPivot_m = Num(R, TEXT("CoMToPivot_mm"), 120.0f) / 1000.0f;
			{ const FString A = R.FindRef(TEXT("Action_Scheme")); if (!A.IsEmpty()) { F.ActionScheme = FName(*A); } }
			F.SlideMass_kg = Num(R, TEXT("SlideMass_kg"), 0.33f);
			F.SlideTravel_m = Num(R, TEXT("SlideTravel_mm"), 50.0f) / 1000.0f;
			F.SpringPreload_N = Num(R, TEXT("SpringPreload_N"), 25.0f);
			F.SpringRate_Npm = Num(R, TEXT("SpringRate_N_per_m"), 1000.0f);
			F.UnlockTravel_m = Num(R, TEXT("UnlockTravel_mm"), 3.0f) / 1000.0f;
			F.BarrelTilt_deg = Num(R, TEXT("BarrelTilt_deg"), 3.0f);
			F.SlideResist_N = Num(R, TEXT("SlideResist_N"), 12.0f);
			F.FeedResist_N = Num(R, TEXT("FeedResist_N"), 30.0f);
			if (!F.Name.IsNone()) { Firearms.Add(F.Name, F); }
		}
	}
	else { UE_LOG(LogTemp, Warning, TEXT("PBL Data: %s не прочитан"), *(Dir / TEXT("Firearms.csv"))); }

	Rows.Reset();
	if (ReadCsv(Dir / TEXT("Materials.csv"), Rows))
	{
		for (const auto& R : Rows)
		{
			FPBLMaterialData M;
			M.Name = FName(*R.FindRef(TEXT("Name")));
			M.Model = FName(*R.FindRef(TEXT("Model")));
			M.Density_kgm3 = Num(R, TEXT("Density_kgm3"), 1000.0f);
			M.Strength_Pa = Num(R, TEXT("Strength_Pa"));
			M.MediumCdScale = Num(R, TEXT("MediumCdScale"), 1.0f);
			M.ThresholdEnergyDensity_Jm2 = Num(R, TEXT("ThresholdEnergyDensity_Jm2"));
			M.Thor_c = Num(R, TEXT("Thor_c")); M.Thor_alpha = Num(R, TEXT("Thor_alpha")); M.Thor_beta = Num(R, TEXT("Thor_beta"));
			M.Thor_gamma = Num(R, TEXT("Thor_gamma")); M.Thor_lambda = Num(R, TEXT("Thor_lambda"));
			M.RicochetAngleDeg = Num(R, TEXT("RicochetAngleDeg"));
			M.RicochetRestitution = Num(R, TEXT("RicochetRestitution"), 0.5f);
			M.WoundWeight = Num(R, TEXT("WoundWeight"));
			if (!M.Name.IsNone()) { Materials.Add(M.Name, M); }
		}
	}
	else { UE_LOG(LogTemp, Warning, TEXT("PBL Data: %s не прочитан"), *(Dir / TEXT("Materials.csv"))); }

	Rows.Reset();
	if (ReadCsv(Dir / TEXT("Reference_Gel.csv"), Rows))
	{
		for (const auto& R : Rows)
		{
			FPBLGelReference G;
			G.Cartridge = FName(*R.FindRef(TEXT("Cartridge")));
			G.Material = FName(*R.FindRef(TEXT("Material")));
			G.V_mps = Num(R, TEXT("V_mps"));
			G.Depth_m = Num(R, TEXT("Depth_cm")) / 100.0f;
			G.ExpandedDiameter_m = Num(R, TEXT("ExpandedDiameter_mm")) / 1000.0f;
			G.Source = R.FindRef(TEXT("Source"));
			if (!G.Cartridge.IsNone()) { References.Add(G); }
		}
	}

	Rows.Reset();
	if (ReadCsv(Dir / TEXT("BodyLayers.csv"), Rows))
	{
		TArray<TPair<int32, FName>> Keys;
		for (const auto& R : Rows)
		{
			const FName Part(*R.FindRef(TEXT("Part")));
			if (Part.IsNone()) { continue; }
			FPBLBodyLayer L;
			L.Material = FName(*R.FindRef(TEXT("Material")));
			L.Thickness_m = Num(R, TEXT("Thickness_mm")) / 1000.0f;
			const FString Cov = R.FindRef(TEXT("Coverage"));
			L.Coverage = Cov.Equals(TEXT("BandsZ"), ESearchCase::IgnoreCase) ? EPBLLayerCoverage::BandsZ : Cov.Equals(TEXT("Core"), ESearchCase::IgnoreCase) ? EPBLLayerCoverage::Core : EPBLLayerCoverage::Full;
			L.P1 = Num(R, TEXT("P1")) / 1000.0f;
			L.P2 = Num(R, TEXT("P2")) / 1000.0f;
			BodyParts.FindOrAdd(Part).Add(L);   // строки в CSV идут по порядку Order
		}
	}
	Rows.Reset();
	if (ReadCsv(Dir / TEXT("BodyParts.csv"), Rows))
	{
		for (const auto& R : Rows)
		{
			const FName Part(*R.FindRef(TEXT("Part")));
			if (!Part.IsNone()) { BodyPartThickness_m.Add(Part, Num(R, TEXT("Thickness_mm")) / 1000.0f); }
		}
	}
	Rows.Reset();
	if (ReadCsv(Dir / TEXT("WeaponParts.csv"), Rows))
	{
		for (const auto& R : Rows)
		{
			FPBLWeaponPartStep P;
			P.Weapon = FName(*R.FindRef(TEXT("Weapon")));
			P.Generation = FName(*R.FindRef(TEXT("Generation")));
			P.Part = FName(*R.FindRef(TEXT("Part")));
			P.DisplayName = R.FindRef(TEXT("DisplayName"));
			P.Stage = FName(*R.FindRef(TEXT("Stage")));
			P.Group = FName(*R.FindRef(TEXT("Group")));
			P.Order = FCString::Atoi(*R.FindRef(TEXT("Order")));
			P.Dir = FVector(Num(R, TEXT("DirX")), Num(R, TEXT("DirY")), Num(R, TEXT("DirZ")));
			P.Dist_cm = Num(R, TEXT("Dist_cm"));
			P.Description = R.FindRef(TEXT("Description"));
			if (!P.Part.IsNone()) { PartSteps_.Add(P); }
		}
	}

	UE_LOG(LogTemp, Display, TEXT("PBL Data: %d cartridges, %d firearms, %d materials, %d gel references, %d body parts, %d part steps from %s"),
		Cartridges.Num(), Firearms.Num(), Materials.Num(), References.Num(), BodyParts.Num(), PartSteps_.Num(), *Dir);
}

static FAutoConsoleCommandWithWorld CmdDataReload(TEXT("pbl.Data.Reload"), TEXT("Reload Content/Data/*.csv"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World && World->GetGameInstance())
		{
			if (UPBLWeaponDataSubsystem* D = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>()) { D->Reload(); }
		}
	}));

TArray<FPBLWeaponPartStep> UPBLWeaponDataSubsystem::PartSteps(FName Weapon, FName Stage) const
{
	TArray<FPBLWeaponPartStep> Out;
	for (const FPBLWeaponPartStep& P : PartSteps_)
	{
		if (P.Weapon == Weapon && (Stage.IsNone() || P.Stage == Stage)) { Out.Add(P); }
	}
	Out.Sort([](const FPBLWeaponPartStep& A, const FPBLWeaponPartStep& B) { return A.Order < B.Order; });
	return Out;
}
