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
