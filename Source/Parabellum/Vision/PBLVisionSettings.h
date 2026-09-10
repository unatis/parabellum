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
	 * Поворот боковых камер от направления взгляда. Вместе с SideFOV должны покрыть yaw от BlendStart
	 * до TotalFOV/2 и pitch до +-30 (углы экрана). 60/70 покрывают 25..95 по yaw; при большем TotalFOV расширять.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 30, ClampMax = 90))
	float SideYaw = 60.0f;

	/** FOV боковых камер (квадратный RT, поэтому горизонтальный = вертикальный). */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 40, ClampMax = 140))
	float SideFOV = 80.0f;

	/** Уровень мипа при чтении боковых RT: периферийное размытие и подавление алиасинга захвата. 0 = без. */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 0, ClampMax = 4))
	float SideMip = 1.5f;

	/** Сторона бокового RT = min(ширина, высота экрана) * это. В Панини шов лежит в зоне почти полной плотности - нужно 1.0. */
	UPROPERTY(Config, EditAnywhere, Category = "Side Captures", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float SideResolutionScale = 1.0f;

	// --- Проекция Панини: x = (d+1)sin(yaw)/(d+cos(yaw)). Одна гладкая функция, без зон. ---

	/** Полный горизонтальный FOV композита. Кромка экрана = TotalFOV/2. 180 достижимо при CSFov 90 (d=3). */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 90, ClampMax = 179))
	float TotalFOV = 160.0f;

	/**
	 * Плотность пикселей в центре = как у CS с этим fov (CS-fov задаётся для 4:3, на 16:9 90 -> 106x74).
	 * Из этого и TotalFOV вычисляется d Панини (см. UPBLVisionComponent::SolvePaniniD).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 60, ClampMax = 120))
	float CSFov = 90.0f;

	/** До какого |pitch| камеры ось проекции полностью привязана к вертикали мира (прямой горизонт и прямые вертикали при наклоне головы). */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 0, ClampMax = 89))
	float PitchAlignStart = 40.0f;

	/** К какому |pitch| ось проекции полностью переходит к камерной (мировая у зенита вырождается). */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = 1, ClampMax = 90))
	float PitchAlignEnd = 70.0f;

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
	float BlurStartYaw = 50.0f;

	/** Радиус размытия на краю экрана (90 градусов), в градусах угла. Едва заметное - основной сигнал периферии само сжатие. */
	UPROPERTY(Config, EditAnywhere, Category = "Blur", meta = (ClampMin = 0, ClampMax = 10))
	float BlurMaxDeg = 0.3f;

	/** 0 - композит; 1/2 - левый/правый RT сырой; 3 - только центр; 4 - композит с тонировкой боков (швы). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ClampMin = 0, ClampMax = 5))
	int32 DebugView = 0;
};
