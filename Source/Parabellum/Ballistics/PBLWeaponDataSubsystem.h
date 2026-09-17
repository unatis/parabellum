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
	const FPBLMaterialData* FindMaterial(FName Name) const { return Materials.Find(Name); }
	const TMap<FName, FPBLMaterialData>& AllMaterials() const { return Materials; }
	const TArray<FPBLGelReference>& GelReferences() const { return References; }
	/** Шаги разборки образца на нужной стадии, в порядке съёма. */
	TArray<FPBLWeaponPartStep> PartSteps(FName Weapon, FName Stage) const;
	const TArray<FPBLWeaponPartStep>& AllPartSteps() const { return PartSteps_; }
	/** Слои части тела (BodyLayers.csv), в порядке прохождения спереди назад; nullptr если части нет. */
	const TArray<FPBLBodyLayer>* FindBodyPart(FName Part) const { return BodyParts.Find(Part); }
	const TMap<FName, TArray<FPBLBodyLayer>>& AllBodyParts() const { return BodyParts; }
	/** Номинальная толщина части тела, м (BodyParts.csv); 0 если не задана. */
	float BodyPartThickness(FName Part) const { const float* T = BodyPartThickness_m.Find(Part); return T ? *T : 0.0f; }

	/** Перечитать CSV (консоль: pbl.Data.Reload). */
	void Reload();

private:
	static bool ReadCsv(const FString& Path, TArray<TMap<FString, FString>>& OutRows);
	TMap<FName, FPBLCartridgeData> Cartridges;
	TMap<FName, FPBLFirearmData> Firearms;
	TMap<FName, FPBLMaterialData> Materials;
	TArray<FPBLGelReference> References;
	TMap<FName, TArray<FPBLBodyLayer>> BodyParts;
	TMap<FName, float> BodyPartThickness_m;
	TArray<FPBLWeaponPartStep> PartSteps_;
};
