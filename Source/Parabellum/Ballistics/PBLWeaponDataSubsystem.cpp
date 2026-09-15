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
			F.MagSize = FCString::Atoi(*R.FindRef(TEXT("MagSize")));
			F.SightHeight_m = FCString::Atof(*R.FindRef(TEXT("SightHeight_mm"))) / 1000.0f;
			F.ZeroRange_m = FCString::Atof(*R.FindRef(TEXT("ZeroRange_m")));
			F.Dispersion_MOA = FCString::Atof(*R.FindRef(TEXT("Dispersion_MOA")));
			if (!F.Name.IsNone()) { Firearms.Add(F.Name, F); }
		}
	}
	else { UE_LOG(LogTemp, Warning, TEXT("PBL Data: %s не прочитан"), *(Dir / TEXT("Firearms.csv"))); }

	UE_LOG(LogTemp, Display, TEXT("PBL Data: %d cartridges, %d firearms from %s"), Cartridges.Num(), Firearms.Num(), *Dir);
}

static FAutoConsoleCommandWithWorld CmdDataReload(TEXT("pbl.Data.Reload"), TEXT("Reload Content/Data/*.csv"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World && World->GetGameInstance())
		{
			if (UPBLWeaponDataSubsystem* D = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>()) { D->Reload(); }
		}
	}));
