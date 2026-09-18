#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PBLCollection.generated.h"

/** Что именно принадлежит игроку: запись хранится в сейве и переживает перезапуск. */
UCLASS()
class PARABELLUM_API UPBLCollectionSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** Купленные образцы - имена из Catalogue.csv. */
	UPROPERTY() TArray<FName> Owned;
	/** Внутренняя валюта. Настоящих платежей в проекте нет. */
	UPROPERTY() int32 Balance = 0;
	/** Какой образец стоит на стенде в оружейке. */
	UPROPERTY() FName Active;
	/** Версия записи: чтобы потом уметь читать старые сейвы. */
	UPROPERTY() int32 Version = 1;
};

/**
 * Коллекция игрока: что куплено, сколько денег, какой образец на стенде.
 *
 * Хранится локально в сейве. Все изменения идут через Buy/SetActive - это сделано намеренно:
 * когда коллекция переедет на сервер, эти два метода станут серверными вызовами, а остальной
 * код (оружейка, магазин) останется прежним. Поэтому ни оружейка, ни магазин не трогают
 * список напрямую.
 */
UCLASS(Config = Game)
class PARABELLUM_API UPBLCollectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsOwned(FName Weapon) const { return Save && Save->Owned.Contains(Weapon); }
	const TArray<FName>& Owned() const;
	int32 Balance() const { return Save ? Save->Balance : 0; }

	/** Купить образец. false + причина, если не хватает денег, уже куплен или нет в витрине. */
	bool Buy(FName Weapon, FString& OutReason);
	/** Поставить образец на стенд. Работает только для купленного. */
	bool SetActive(FName Weapon);
	FName Active() const;

	/** Следующий/предыдущий купленный образец по кругу; NAME_None, если коллекция пуста. */
	FName NextOwned(int32 Direction) const;

	void Persist();

private:
	UPROPERTY(Transient) TObjectPtr<UPBLCollectionSave> Save;
	/** Стартовый счёт нового игрока. */
	UPROPERTY(Config) int32 StartingBalance = 2500;
	UPROPERTY(Config) FString SlotName = TEXT("PBLCollection");
};
