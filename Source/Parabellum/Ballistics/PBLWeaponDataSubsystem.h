#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Ballistics/PBLBallisticsTypes.h"
#include "PBLWeaponDataSubsystem.generated.h"

/**
 * Данные патронов и оружия из Content/Data/*.csv. Текст в репозитории, никаких .uasset:
 * правки видны в diff, загрузка на старте игры. Ищется по имени.
 */
UCLASS()
class PARABELLUM_API UPBLWeaponDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FPBLCartridgeData* FindCartridge(FName Name) const { return Cartridges.Find(Name); }
	const FPBLFirearmData* FindFirearm(FName Name) const { return Firearms.Find(Name); }
	const TMap<FName, FPBLCartridgeData>& AllCartridges() const { return Cartridges; }
	const TMap<FName, FPBLFirearmData>& AllFirearms() const { return Firearms; }

	/** Перечитать CSV (консоль: pbl.Data.Reload). */
	void Reload();

private:
	static bool ReadCsv(const FString& Path, TArray<TMap<FString, FString>>& OutRows);
	TMap<FName, FPBLCartridgeData> Cartridges;
	TMap<FName, FPBLFirearmData> Firearms;
};
