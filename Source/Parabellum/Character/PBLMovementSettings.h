#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PBLMovementSettings.generated.h"

/**
 * Параметры тела и движения. Один объект на игру, читается через
 * GetDefault<UPBLMovementSettings>(), значения - из Config/DefaultGame.ini.
 *
 * Стартовые числа переведены из CS (1 юнит Source = 1.905 см). Это первое
 * приближение для Э2.3 - подбираются на глаз, а не считаются.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Parabellum Movement"))
class PARABELLUM_API UPBLMovementSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Parabellum"); }

	// --- Тело. CS: рост 72u, ширина 32u, глаза 64u; присев 54u / 46u ---

	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ForceUnits = cm))
	float CapsuleRadius = 30.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ForceUnits = cm))
	float CapsuleHalfHeight = 68.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ForceUnits = cm))
	float CrouchedCapsuleHalfHeight = 51.0f;

	/** Высота глаз от низа капсулы, стоя. */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ForceUnits = cm))
	float EyeHeight = 122.0f;

	/** Высота глаз от низа капсулы, присев. */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ForceUnits = cm))
	float CrouchedEyeHeight = 88.0f;

	/** Скорость сглаживания камеры при приседании/вставании (FInterpTo). ~12 = около четверти секунды. */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ClampMin = 1))
	float CrouchCameraInterpSpeed = 12.0f;

	// --- Движение. CS: бег 250 u/s, присев ~85 u/s ---

	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ForceUnits = "cm/s"))
	float MaxWalkSpeed = 480.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ForceUnits = "cm/s"))
	float MaxCrouchSpeed = 160.0f;

	/** Скорость бега (Shift). Бегом с вытянутым оружием не целятся, поэтому в прицеле бег не работает. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ForceUnits = "cm/s"))
	float MaxSprintSpeed = 700.0f;

	/** Длина шага при обычной ходьбе, см. У человека ростом 180 это 75-80 (0.41-0.45 роста). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement|Походка", meta = (ForceUnits = "cm"))
	float StepLength = 75.0f;

	/** Насколько шаг удлиняется с ростом скорости, см на каждый м/с сверх ходьбы.
	    Бегущий не просто чаще перебирает ногами - он и шире шагает, иначе темп выходит вздорным. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement|Походка")
	float StepLengthGain = 18.0f;

	/** Размах оружия при ходьбе: вверх-вниз на частоте шага, вбок - на половинной (полный цикл
	    походки - два шага), плюс небольшой крен. Значения при обычной ходьбе; с бегом растут. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement|Походка", meta = (ForceUnits = "cm"))
	float BobVertical = 1.1f;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Походка", meta = (ForceUnits = "cm"))
	float BobLateral = 1.6f;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Походка", meta = (ForceUnits = "deg"))
	float BobRoll = 0.9f;

	/** Во сколько раз гасится раскачка при прицеливании: оружие придерживают, но не намертво. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement|Походка")
	float BobAimScale = 0.3f;

	/** Множитель скорости ходьбы при прицеливании через мушку (реально с вытянутым пистолетом идут шагом ~0.5-0.6 от быстрого шага). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement")
	float AimSpeedScale = 0.6f;

	/** В CS разгон почти мгновенный. Больше = резче старт и стоп. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement")
	float MaxAcceleration = 4096.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Movement")
	float BrakingDeceleration = 4096.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ForceUnits = "cm/s"))
	float JumpVelocity = 420.0f;

	/** 0 = в воздухе не управляем, 1 = как на земле. В CS воздушный контроль заметный. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = 0, ClampMax = 1))
	float AirControl = 0.35f;

	// --- Мышь. Как в CS: градусы = counts * 0.022 * sensitivity ---

	UPROPERTY(Config, EditAnywhere, Category = "Mouse")
	float MouseSensitivity = 2.0f;

	/** m_yaw из Source. Не трогать - это единица, в которой все привыкли мерить sens. */
	UPROPERTY(Config, EditAnywhere, Category = "Mouse", AdvancedDisplay)
	float MouseDegreesPerCount = 0.022f;

	/**
	 * Эмпирический множитель на сырую дельту мыши из Enhanced Input. Подобран 2026-09-09
	 * по ощущению пользователя в два шага (8 -> "ещё в два раза" -> 16): его CS sens 1 ==
	 * наш sens 16 при множителе 1. Откуда берётся 1/16 - в коде не найдено (не AxisConfig:
	 * EI читает RawValue). Если найдётся источник - убрать множитель и вернуть 1.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Mouse", AdvancedDisplay)
	float MouseCountsScale = 32.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Mouse")
	bool bInvertMouseY = false;
};
