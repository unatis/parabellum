#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PBLRecoilSettings.generated.h"

/** Параметры стрелка для одного типа хвата: упругость и инерция тела, держащего оружие. */
USTRUCT()
struct FPBLHoldParams
{
	GENERATED_BODY()
	/** Момент инерции частей тела, поворачивающихся с оружием (кисти/предплечья или плечо+руки), кг·м². */
	UPROPERTY(Config, EditAnywhere) float BodyInertia_kgm2 = 0.02f;
	/** Жёсткость хвата - возвращающий момент на радиан, Н·м/рад. */
	UPROPERTY(Config, EditAnywhere) float Stiffness_Nm_per_rad = 150.0f;
	/** Демпфирование хвата, Н·м·с/рад. */
	UPROPERTY(Config, EditAnywhere) float Damping_Nms_per_rad = 2.2f;
	/** Доля пика подброса, которую стрелок не возвращает сам (сдвиг точки равновесия - её компенсирует игрок мышью). */
	UPROPERTY(Config, EditAnywhere) float ResidualFraction = 0.25f;
	/** Случайный уход по горизонту как доля угловой скорости подброса (асимметрия хвата). */
	UPROPERTY(Config, EditAnywhere) float YawFraction = 0.3f;
	/** Систематический боковой увод (доля угловой скорости подброса, + = вправо): асимметрия хвата правши. */
	UPROPERTY(Config, EditAnywhere) float YawBias = 0.1f;
};

/**
 * Отдача из физики (E10.5). Импульс выстрела p = m_пули·V0 + m_пороха·V_газов; момент L = p·h (h - ось ствола над
 * шарниром хвата); угловая скорость подброса ω0 = L / (m_оружия·d² + I_тела). Дальше хват - демпфированная пружина:
 * I·θ'' + c·θ' + k·(θ - θ_rest) = 0. Камера (линия прицеливания) следует θ; θ_rest получает остаток, который игрок
 * компенсирует сам. Все параметры оружия/патрона - в CSV, параметры стрелка - здесь (DefaultGame.ini).
 * Свободная энергия отдачи ½·m·(p/m)² сверяется с публичными калькуляторами: pbl.Ballistics.Recoil <оружие>.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Parabellum Recoil"))
class PARABELLUM_API UPBLRecoilSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UPBLRecoilSettings& Get() { return *GetDefault<UPBLRecoilSettings>(); }
	const FPBLHoldParams& Hold(FName Name) const { return Name == TEXT("Rifle") ? RifleHold : PistolHold; }

	UPROPERTY(Config, EditAnywhere, Category = "Recoil") bool bPhysicsRecoil = true;
	UPROPERTY(Config, EditAnywhere, Category = "Recoil") FPBLHoldParams PistolHold;
	UPROPERTY(Config, EditAnywhere, Category = "Recoil") FPBLHoldParams RifleHold;
	/** Визуальный откат меша оружия назад: см на 1 м/с свободной скорости отдачи, и время затухания, с. */
	UPROPERTY(Config, EditAnywhere, Category = "Visual") float VisualKick_cm_per_mps = 0.6f;
	UPROPERTY(Config, EditAnywhere, Category = "Visual") float VisualKickDecay_s = 0.05f;
	/** Дополнительный визуальный поворот меша относительно камеры: доля угла подброса. */
	UPROPERTY(Config, EditAnywhere, Category = "Visual") float VisualPitchScale = 0.6f;
};
