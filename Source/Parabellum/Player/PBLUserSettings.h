#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PBLUserSettings.generated.h"

/**
 * Пользовательские настройки на диске.
 *
 * Почему не UPBLMovementSettings напрямую: тот помечен DefaultConfig, то есть лежит в
 * DefaultGame.ini - это файл проекта, он под контролем версий и в игре не пишется.
 * SaveConfig() на нём отрабатывает молча и ничего не сохраняет (проверено: значение
 * возвращалось к проектному при следующем запуске). Поэтому проектные значения остаются
 * значениями по умолчанию, а правки игрока живут здесь и накладываются поверх при старте.
 */
UCLASS()
class PARABELLUM_API UPBLUserSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() float WalkSpeed = 0.0f;
	UPROPERTY() float SprintSpeed = 0.0f;
	UPROPERTY() float CrouchSpeed = 0.0f;
	UPROPERTY() float MouseSensitivity = 0.0f;
	/** Ноль в любом поле означает «не задано» - берём проектное значение. */
	UPROPERTY() int32 Version = 1;
};

/** Загружает настройки при старте и кладёт их поверх проектных; сохраняет по требованию. */
UCLASS(Config = Game)
class PARABELLUM_API UPBLUserSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Снять текущие значения из настроек движения и записать на диск. */
	void Capture();
	/** Забыть пользовательские правки и вернуться к проектным значениям. */
	void ResetToProjectDefaults();

private:
	void ApplyToMovementSettings();

	UPROPERTY(Transient) TObjectPtr<UPBLUserSettingsSave> Save;
	/** Проектные значения, снятые до наложения пользовательских. Восстановить их иначе
	    нечем: часть из них задана в коде, а не в ini, и ReloadConfig() их не вернёт. */
	float ProjectWalk = 0.0f, ProjectSprint = 0.0f, ProjectCrouch = 0.0f, ProjectSens = 0.0f;
	UPROPERTY(Config) FString SlotName = TEXT("PBLUserSettings");
};
