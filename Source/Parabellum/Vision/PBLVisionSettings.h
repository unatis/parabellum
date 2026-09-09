#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PBLVisionSettings.generated.h"

class UMaterialInterface;

/**
 * Параметры системы зрения (фовеальный композит из трёх рендеров).
 * Все углы в градусах. Подбираются на глаз по скриншотам, поэтому всё в ini.
 * Консоль: pbl.Vision 0/1 - выключить/включить, pbl.VisionDebug N - отладочные виды.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Parabellum Vision"))
class PARABELLUM_API UPBLVisionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Parabellum"); }

	UPROPERTY(Config, EditAnywhere, Category = "Vision")
	bool bEnabled = true;

	/** Post Process Material композита. Генерируется Tools/make_vision_material.py. */
	UPROPERTY(Config, EditAnywhere, Category = "Vision")
	TSoftObjectPtr<UMaterialInterface> CompositeMaterial;

	// --- Боковые рендеры ---

	/**
	 * Поворот боковых камер от направления взгляда. Вместе с SideFOV должен закрывать
	 * yaw от ~5 до 90+ и pitch до +-35: бока начинаются с SideYaw - SideFOV/2, и всё, что
	 * левее этого угла и выше охвата прямоугольного центра, иначе остаётся чёрным.
	 * 77.5/90 давали чёрные углы; 60/110 закрывают всё.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 30, ClampMax = 90))
	float SideYaw = 60.0f;

	/** FOV боковых камер (квадратный RT, поэтому горизонтальный = вертикальный). */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 40, ClampMax = 140))
	float SideFOV = 110.0f;

	/** Сторона бокового RT = min(ширина, высота экрана) * это. Периферия размывается - хватит и 0.5. */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float SideResolutionScale = 0.5f;

	// --- Проекция: x = yaw / (yaw + CompressB), край экрана = 90 градусов ---

	/** Меньше - сильнее увеличен центр и сжаты края. 33 -> центральные 60 градусов = 65% ширины. */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 5, ClampMax = 200))
	float CompressB = 33.2f;

	// --- Шов центр/бока: плавный переход по |yaw| ---

	UPROPERTY(Config, EditAnywhere, Category = "Seam", meta = (ClampMin = 0, ClampMax = 90))
	float BlendStartYaw = 35.0f;

	/** Должен быть меньше половины FOV центральной камеры (103/2 = 51.5), иначе шов вылезет за центральный рендер. */
	UPROPERTY(Config, EditAnywhere, Category = "Seam", meta = (ClampMin = 0, ClampMax = 90))
	float BlendEndYaw = 50.0f;

	// --- Периферийное размытие ---

	/** С какого |yaw| начинается размытие. Начинать внутри центрального рендера, чтобы шов попал в размытое. */
	UPROPERTY(Config, EditAnywhere, Category = "Blur", meta = (ClampMin = 0, ClampMax = 90))
	float BlurStartYaw = 25.0f;

	/** Радиус размытия на краю экрана (90 градусов), в градусах угла. */
	UPROPERTY(Config, EditAnywhere, Category = "Blur", meta = (ClampMin = 0, ClampMax = 10))
	float BlurMaxDeg = 1.5f;

	/** 0 - композит; 1/2 - левый/правый RT сырой; 3 - только центр; 4 - композит с тонировкой боков (швы). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ClampMin = 0, ClampMax = 4))
	int32 DebugView = 0;
};
