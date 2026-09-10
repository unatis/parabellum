#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PBLVisionComponent.generated.h"

class UCameraComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

/**
 * Фовеальное зрение: два боковых SceneCapture на камере владельца + Post Process
 * Material, который сшивает их с центральным рендером в 180 градусов по горизонтали.
 *
 * Чисто клиентская вещь: сервер про неё не знает, репликации нет. Всё, что она
 * меняет для геймплея - это депроекция экран->мир (Э8), и та живёт отдельно.
 */
UCLASS(ClassGroup = (Parabellum), meta = (BlueprintSpawnableComponent))
class PARABELLUM_API UPBLVisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPBLVisionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	bool Setup(UCameraComponent* Camera);
	void ApplyEnabled(bool bEnable);
	void UpdateRenderTargetSize();
	void PushParameters();
	void UpdateSideRotation(float EffectivePitchDeg);
	/** Эффективный наклон системы проекции: до PitchAlignStart - полный, к PitchAlignEnd - ноль. Зеркало шейдера. */
	static float EffectivePitch(float CamPitchDeg);
	/** d Панини, при котором плотность центра равна CS с CSFov при данном аспекте и кромка = TotalFOV/2. */
	static float SolvePaniniD(float TotalFOV, float CSFov, float Aspect);

	UPROPERTY(Transient) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> CaptureL;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> CaptureR;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> RenderTargetL;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> RenderTargetR;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CompositeMID;

	int32 CurrentSideSize = 0;
	float LastLoggedD = -1.0f;
	bool bActive = false;
	bool bSetupDone = false;
};
