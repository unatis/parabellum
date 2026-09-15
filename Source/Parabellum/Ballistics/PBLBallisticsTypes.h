#pragma once

#include "CoreMinimal.h"
#include "PBLBallisticsTypes.generated.h"

/** Единицы: СИ (м, кг, с), кроме мест, где явно указано иначе. Данные - Content/Data/*.csv. */

UENUM()
enum class EPBLDragModel : uint8 { G1, G7 };

USTRUCT(BlueprintType)
struct FPBLCartridgeData
{
	GENERATED_BODY()
	UPROPERTY() FName Name;
	UPROPERTY() float BulletMass_kg = 0.008f;
	UPROPERTY() float Diameter_m = 0.009f;
	/** Начальная скорость при опорной длине ствола. */
	UPROPERTY() float V0_mps = 350.0f;
	UPROPERTY() float RefBarrel_m = 0.102f;
	/** Прирост скорости на каждые 25 мм ствола (м/с). Линейное приближение в разумном диапазоне. */
	UPROPERTY() float dV_per_25mm_mps = 7.0f;
	UPROPERTY() EPBLDragModel DragModel = EPBLDragModel::G1;
	/** Баллистический коэффициент в lb/in^2 (как публикуют производители). */
	UPROPERTY() float BC = 0.15f;
	UPROPERTY() FName Construction = TEXT("FMJ");
	UPROPERTY() float Length_m = 0.015f;

	// --- Терминальная баллистика в плотной среде (модель Понселе, см. PBLPenetration.h) ---
	/** Коэффициент сопротивления носом вперёд относительно площади сечения (в среде, не в воздухе). */
	UPROPERTY() float MediumCd = 0.2f;
	/** Глубина начала кувырка неэкспансивной пули, м (0 = не кувыркается). Cd переходит в YawedCd за ~5 см. */
	UPROPERTY() float YawOnsetDepth_m = 0.0f;
	UPROPERTY() float YawedCd = 0.7f;
	/** Порог скорости раскрытия JHP, м/с (0 = не раскрывается). */
	UPROPERTY() float ExpansionThresholdV_mps = 0.0f;
	UPROPERTY() float ExpandedDiameter_m = 0.0f;
	UPROPERTY() float ExpandedCd = 0.3f;
	/** Глубина, на которой раскрытие завершается, м. */
	UPROPERTY() float ExpansionDepth_m = 0.025f;
	UPROPERTY() float RetainedMassFraction = 1.0f;

	float FrontalArea_m2() const { return PI * 0.25f * Diameter_m * Diameter_m; }
	float SectionalDensity_kgm2() const { return BulletMass_kg / (Diameter_m * Diameter_m); }
};

USTRUCT(BlueprintType)
struct FPBLFirearmData
{
	GENERATED_BODY()
	UPROPERTY() FName Name;
	UPROPERTY() FName Cartridge;
	UPROPERTY() float Barrel_m = 0.114f;
	/** 0 = только одиночный. */
	UPROPERTY() int32 RPM = 0;
	UPROPERTY() FName Action = TEXT("Semi");
	UPROPERTY() float Mass_kg = 0.9f;
	UPROPERTY() int32 MagSize = 17;
	/** Высота прицельной линии над осью ствола. */
	UPROPERTY() float SightHeight_m = 0.02f;
	UPROPERTY() float ZeroRange_m = 25.0f;
	/** Механическое рассеивание, угловые минуты (полный угол группы). */
	UPROPERTY() float Dispersion_MOA = 4.0f;
};

/** Материал среды (Content/Data/Materials.csv). Пока одна модель - Понселе для мягких сред (гель, вода, ткани). */
USTRUCT(BlueprintType)
struct FPBLMaterialData
{
	GENERATED_BODY()
	UPROPERTY() FName Name;
	UPROPERTY() FName Model = TEXT("Poncelet");
	UPROPERTY() float Density_kgm3 = 1030.0f;
	/** Статическая составляющая сопротивления (прочность), Па. */
	UPROPERTY() float Strength_Pa = 160000.0f;
};

/** Эталонная строка (Content/Data/Reference_Gel.csv) для автосверки pbl.Ballistics.GelCheck. */
struct FPBLGelReference
{
	FName Cartridge;
	FName Material;
	float V_mps = 0.0f;
	float Depth_m = 0.0f;
	float ExpandedDiameter_m = 0.0f;
	FString Source;
};

USTRUCT(BlueprintType)
struct FPBLAtmosphere
{
	GENERATED_BODY()
	UPROPERTY() float Temperature_C = 15.0f;
	UPROPERTY() float Pressure_hPa = 1013.25f;
	UPROPERTY() float Humidity = 0.0f;   // 0..1
	UPROPERTY() FVector Wind_mps = FVector::ZeroVector;
};

/** Состояние пули для интегрирования. */
struct FPBLProjectileState
{
	FVector Position = FVector::ZeroVector;   // м
	FVector Velocity = FVector::ZeroVector;   // м/с
	float Time = 0.0f;
};

/** Строка таблицы траектории (для сверки с публичными калькуляторами). */
struct FPBLTrajectoryRow
{
	float Range_m = 0.0f;
	float Drop_m = 0.0f;       // относительно линии прицеливания, отрицательное = ниже
	float Velocity_mps = 0.0f;
	float Energy_J = 0.0f;
	float Time_s = 0.0f;
};
