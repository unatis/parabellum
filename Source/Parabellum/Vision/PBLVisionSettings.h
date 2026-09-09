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
	 * 77.5/90 давали чёрные углы; 60/130 закрывают всё, включая PitchCap 60 у кромки.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 30, ClampMax = 90))
	float SideYaw = 60.0f;

	/** FOV боковых камер (квадратный RT, поэтому горизонтальный = вертикальный). */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 40, ClampMax = 140))
	float SideFOV = 130.0f;

	/** Сторона бокового RT = min(ширина, высота экрана) * это. Периферия размывается - хватит и 0.5. */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float SideResolutionScale = 0.5f;

	// --- Проекция: центр ректилинейный, периферия сжата гиперболой до 90 градусов у края ---

	/** Полуугол ректилинейной зоны. 35 при RectWidth 0.7 = CS FOV 90 по обеим осям. */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 10, ClampMax = 60))
	float RectYaw = 35.0f;

	/** Какую долю полуширины экрана занимает ректилинейная зона. Остаток - под EdgeYaw-RectYaw градусов периферии. */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 0.3, ClampMax = 0.95))
	float RectWidth = 0.70f;

	/** Yaw у кромки экрана. 90 = полные 180 по горизонтали (спека); меньше - мягче сжатие периферии. */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 45, ClampMax = 100))
	float EdgeYaw = 90.0f;

	/** Вертикальное сжатие периферии как степень горизонтального: 1 = изотропно (всё у кромки схлопывается), 0 = только по ширине (тонкие полоски). */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 0, ClampMax = 1))
	float VertCompress = 0.5f;

	/** За сколько градусов после RectYaw вертикальный масштаб переходит от перспективы к сжатию (убирает гребень на границе). */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 1, ClampMax = 40))
	float VertBlendDeg = 10.0f;

	/** Насколько высоко (по pitch) может смотреть кромка экрана. Должно покрываться боковыми камерами (SideFOV/2). */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 30, ClampMax = 85))
	float PitchCap = 60.0f;

	// --- Шов центр/бока: плавный переход по |yaw| ---

	UPROPERTY(Config, EditAnywhere, Category = "Seam", meta = (ClampMin = 0, ClampMax = 90))
	float BlendStartYaw = 35.0f;

	/** Должен быть меньше половины FOV центральной камеры (103/2 = 51.5), иначе шов вылезет за центральный рендер. */
	UPROPERTY(Config, EditAnywhere, Category = "Seam", meta = (ClampMin = 0, ClampMax = 90))
	float BlendEndYaw = 50.0f;

	// --- Периферийное размытие ---

	/**
	 * С какого |yaw| начинается размытие. Ректилинейный центр должен оставаться резким: на плоском
	 * экране игрок может посмотреть на край, и заметное размытие читается как артефакт, а не как зрение.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Blur", meta = (ClampMin = 0, ClampMax = 90))
	float BlurStartYaw = 35.0f;

	/** Радиус размытия на краю экрана (90 градусов), в градусах угла. Едва заметное - основной сигнал периферии само сжатие. */
	UPROPERTY(Config, EditAnywhere, Category = "Blur", meta = (ClampMin = 0, ClampMax = 10))
	float BlurMaxDeg = 0.8f;

	/** 0 - композит; 1/2 - левый/правый RT сырой; 3 - только центр; 4 - композит с тонировкой боков (швы). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ClampMin = 0, ClampMax = 4))
	int32 DebugView = 0;
};
