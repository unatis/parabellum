#include "Collection/PBLCollection.h"

#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UPBLCollectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Save = Cast<UPBLCollectionSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
	if (!Save)
	{
		Save = Cast<UPBLCollectionSave>(UGameplayStatics::CreateSaveGameObject(UPBLCollectionSave::StaticClass()));
		Save->Balance = StartingBalance;
		Persist();
		UE_LOG(LogTemp, Display, TEXT("PBL Collection: новая коллекция, счёт %d"), Save->Balance);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("PBL Collection: %d образцов, счёт %d, на стенде %s"),
			Save->Owned.Num(), Save->Balance, *Save->Active.ToString());
	}
}

const TArray<FName>& UPBLCollectionSubsystem::Owned() const
{
	static const TArray<FName> Empty;
	return Save ? Save->Owned : Empty;
}

FName UPBLCollectionSubsystem::Active() const
{
	if (!Save) { return NAME_None; }
	// Если активного нет или он почему-то не куплен - берём первый купленный.
	if (Save->Active.IsNone() || !Save->Owned.Contains(Save->Active))
	{
		return Save->Owned.Num() > 0 ? Save->Owned[0] : NAME_None;
	}
	return Save->Active;
}

bool UPBLCollectionSubsystem::Buy(FName Weapon, FString& OutReason)
{
	if (!Save) { OutReason = TEXT("коллекция не загружена"); return false; }
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	const FPBLCatalogueEntry* E = Data ? Data->FindCatalogue(Weapon) : nullptr;
	if (!E) { OutReason = FString::Printf(TEXT("%s нет в витрине"), *Weapon.ToString()); return false; }
	if (Save->Owned.Contains(Weapon)) { OutReason = TEXT("уже в коллекции"); return false; }
	if (!E->IsAvailable()) { OutReason = TEXT("образец ещё не готов: модели нет"); return false; }
	if (Save->Balance < E->Price)
	{
		OutReason = FString::Printf(TEXT("не хватает %d"), E->Price - Save->Balance);
		return false;
	}
	Save->Balance -= E->Price;
	Save->Owned.Add(Weapon);
	if (Save->Active.IsNone()) { Save->Active = Weapon; }
	Persist();
	OutReason = FString::Printf(TEXT("куплен %s за %d, остаток %d"), *E->DisplayName, E->Price, Save->Balance);
	return true;
}

bool UPBLCollectionSubsystem::SetActive(FName Weapon)
{
	if (!Save || !Save->Owned.Contains(Weapon)) { return false; }
	Save->Active = Weapon;
	Persist();
	return true;
}

FName UPBLCollectionSubsystem::NextOwned(int32 Direction) const
{
	if (!Save || Save->Owned.Num() == 0) { return NAME_None; }
	const int32 N = Save->Owned.Num();
	const int32 Cur = FMath::Max(0, Save->Owned.IndexOfByKey(Active()));
	return Save->Owned[((Cur + Direction) % N + N) % N];
}

void UPBLCollectionSubsystem::Persist()
{
	if (Save) { UGameplayStatics::SaveGameToSlot(Save, SlotName, 0); }
}
