#include "Vision/PBLVisionComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UnrealClient.h"
#include "Vision/PBLVisionSettings.h"

// pbl.Vision: -1 = как в настройках, 0 = выкл, 1 = вкл. Для A/B по Э9.4.
static TAutoConsoleVariable<int32> CVarPBLVision(TEXT("pbl.Vision"), -1, TEXT("Foveal vision: -1 settings, 0 off, 1 on"));
// pbl.VisionDebug: -1 = как в настройках; 0 композит, 1/2 боковые RT сырые, 3 только центр, 4 тонировка.
static TAutoConsoleVariable<int32> CVarPBLVisionDebug(TEXT("pbl.VisionDebug"), -1, TEXT("Foveal vision debug view"));
// Живой подбор проекции из консоли; отрицательное = брать из настроек.
static TAutoConsoleVariable<float> CVarTotalFOV(TEXT("pbl.Vision.FOV"), -1.0f, TEXT("Total horizontal FOV of the composite"));
static TAutoConsoleVariable<float> CVarPaniniD(TEXT("pbl.Vision.D"), -1.0f, TEXT("Panini parameter d"));
static TAutoConsoleVariable<float> CVarBlurMax(TEXT("pbl.Vision.Blur"), -1.0f, TEXT("Max peripheral blur, degrees"));

static float Pick(const TAutoConsoleVariable<float>& CVar, float Fallback)
{
	const float V = CVar.GetValueOnGameThread();
	return V >= 0.0f ? V : Fallback;
}

namespace
{
	USceneCaptureComponent2D* MakeSideCapture(AActor* Owner, UCameraComponent* Camera, const TCHAR* Name, float Yaw, float FOV)
	{
		USceneCaptureComponent2D* Cap = NewObject<USceneCaptureComponent2D>(Owner, Name);
		Cap->SetupAttachment(Camera);
		Cap->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
		Cap->FOVAngle = FOV;
		Cap->ProjectionType = ECameraProjectionMode::Perspective;
		// HDR линейный цвет без постобработки: композит сам делает bloom/тонмап один раз.
		Cap->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
		Cap->bCaptureEveryFrame = true;
		Cap->bCaptureOnMovement = false;
		Cap->bAlwaysPersistRenderingState = true; // нужно Lumen/TSR-истории в захватах (5.3+)
		// Экранные эффекты на боках дают швы - выключаем (спека, "Известные проблемы").
		Cap->ShowFlags.SetScreenSpaceReflections(false);
		Cap->ShowFlags.SetAmbientOcclusion(false);
		Cap->ShowFlags.SetMotionBlur(false);
		Cap->ShowFlags.SetBloom(false);
		Cap->ShowFlags.SetLensFlares(false);
		Cap->ShowFlags.SetVignette(false);
		Cap->ShowFlags.SetGrain(false);
		Cap->ShowFlags.SetTemporalAA(false);
		Cap->ShowFlags.SetAntiAliasing(false);
		Cap->RegisterComponent();
		return Cap;
	}

	UTextureRenderTarget2D* MakeSideRT(UObject* Outer, const TCHAR* Name, int32 Size)
	{
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Outer, Name);
		RT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
		RT->ClearColor = FLinearColor::Black;
		RT->bAutoGenerateMips = false;
		RT->InitAutoFormat(Size, Size);
		RT->UpdateResourceImmediate(true);
		return RT;
	}
}

UPBLVisionComponent::UPBLVisionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // после движения камеры, до рендера
}

void UPBLVisionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Зрение есть только у того, кто смотрит в экран.
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		SetComponentTickEnabled(false);
		return;
	}

	UCameraComponent* Cam = GetOwner()->FindComponentByClass<UCameraComponent>();
	if (!Cam)
	{
		UE_LOG(LogTemp, Error, TEXT("PBL Vision: у владельца нет UCameraComponent"));
		SetComponentTickEnabled(false);
		return;
	}
	bSetupDone = Setup(Cam);
}

bool UPBLVisionComponent::Setup(UCameraComponent* InCamera)
{
	const UPBLVisionSettings* S = GetDefault<UPBLVisionSettings>();
	Camera = InCamera;

	UMaterialInterface* Mat = S->CompositeMaterial.LoadSynchronous();
	if (!Mat)
	{
		UE_LOG(LogTemp, Error, TEXT("PBL Vision: не загрузился материал '%s' - запусти Tools/make_vision_material.bat"), *S->CompositeMaterial.ToString());
		return false;
	}

	CurrentSideSize = 256; // реальный размер выставит UpdateRenderTargetSize при первом тике
	RenderTargetL = MakeSideRT(this, TEXT("PBL_SideRT_L"), CurrentSideSize);
	RenderTargetR = MakeSideRT(this, TEXT("PBL_SideRT_R"), CurrentSideSize);

	CaptureL = MakeSideCapture(GetOwner(), Camera, TEXT("PBL_SideCapture_L"), -S->SideYaw, S->SideFOV);
	CaptureR = MakeSideCapture(GetOwner(), Camera, TEXT("PBL_SideCapture_R"),  S->SideYaw, S->SideFOV);
	CaptureL->TextureTarget = RenderTargetL;
	CaptureR->TextureTarget = RenderTargetR;

	CompositeMID = UMaterialInstanceDynamic::Create(Mat, this);
	CompositeMID->SetTextureParameterValue(TEXT("SideL"), RenderTargetL);
	CompositeMID->SetTextureParameterValue(TEXT("SideR"), RenderTargetR);
	PushParameters();

	Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, CompositeMID));
	Camera->PostProcessBlendWeight = 1.0f;

	UE_LOG(LogTemp, Display, TEXT("PBL Vision: setup ok (yaw ±%.1f, fov %.0f, scale %.2f, material %s)"),
		S->SideYaw, S->SideFOV, S->SideResolutionScale, *Mat->GetName());

	ApplyEnabled(S->bEnabled);
	return true;
}

void UPBLVisionComponent::ApplyEnabled(bool bEnable)
{
	bActive = bEnable;
	if (CaptureL) { CaptureL->bCaptureEveryFrame = bEnable; CaptureL->SetVisibility(bEnable); }
	if (CaptureR) { CaptureR->bCaptureEveryFrame = bEnable; CaptureR->SetVisibility(bEnable); }
	if (Camera && CompositeMID)
	{
		for (FWeightedBlendable& B : Camera->PostProcessSettings.WeightedBlendables.Array)
		{
			if (B.Object == CompositeMID) { B.Weight = bEnable ? 1.0f : 0.0f; }
		}
	}
}

void UPBLVisionComponent::UpdateRenderTargetSize()
{
	if (!GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport) { return; }
	const FIntPoint View = GEngine->GameViewport->Viewport->GetSizeXY();
	if (View.X <= 0 || View.Y <= 0) { return; }

	const float Scale = GetDefault<UPBLVisionSettings>()->SideResolutionScale;
	const int32 Size = FMath::Clamp(FMath::RoundToInt(FMath::Min(View.X, View.Y) * Scale), 64, 4096);
	if (Size == CurrentSideSize) { return; }

	CurrentSideSize = Size;
	RenderTargetL->ResizeTarget(Size, Size);
	RenderTargetR->ResizeTarget(Size, Size);
	UE_LOG(LogTemp, Display, TEXT("PBL Vision: side RT %dx%d (viewport %dx%d)"), Size, Size, View.X, View.Y);
}

void UPBLVisionComponent::PushParameters()
{
	if (!CompositeMID || !Camera) { return; }
	const UPBLVisionSettings* S = GetDefault<UPBLVisionSettings>();
	const int32 CVarDebug = CVarPBLVisionDebug.GetValueOnGameThread();
	const int32 Debug = CVarDebug >= 0 ? CVarDebug : S->DebugView;

	// Имена совпадают с Shaders/FovealComposite.ush и SCALARS в make_vision_material.py.
	float Aspect = 16.0f / 9.0f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint V = GEngine->GameViewport->Viewport->GetSizeXY();
		if (V.X > 0 && V.Y > 0) { Aspect = (float)V.X / (float)V.Y; }
	}
	CompositeMID->SetScalarParameterValue(TEXT("Aspect"), Aspect);
	CompositeMID->SetScalarParameterValue(TEXT("CenterFOV"), Camera->FieldOfView);
	CompositeMID->SetScalarParameterValue(TEXT("SideYaw"), S->SideYaw);
	CompositeMID->SetScalarParameterValue(TEXT("SideFOV"), S->SideFOV);
	CompositeMID->SetScalarParameterValue(TEXT("TotalFOV"), Pick(CVarTotalFOV, S->TotalFOV));
	CompositeMID->SetScalarParameterValue(TEXT("PaniniD"), Pick(CVarPaniniD, S->PaniniD));
	CompositeMID->SetScalarParameterValue(TEXT("BlendStart"), S->BlendStartYaw);
	CompositeMID->SetScalarParameterValue(TEXT("BlendEnd"), S->BlendEndYaw);
	CompositeMID->SetScalarParameterValue(TEXT("BlurStartYaw"), S->BlurStartYaw);
	CompositeMID->SetScalarParameterValue(TEXT("BlurMaxDeg"), Pick(CVarBlurMax, S->BlurMaxDeg));
	CompositeMID->SetScalarParameterValue(TEXT("Debug"), (float)Debug);
}

void UPBLVisionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bSetupDone) { return; }

	const int32 CVarOn = CVarPBLVision.GetValueOnGameThread();
	const bool bWant = CVarOn >= 0 ? (CVarOn != 0) : GetDefault<UPBLVisionSettings>()->bEnabled;
	if (bWant != bActive) { ApplyEnabled(bWant); }
	if (!bActive) { return; }

	UpdateRenderTargetSize();
	PushParameters();
}

void UPBLVisionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Camera && CompositeMID)
	{
		Camera->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
			[this](const FWeightedBlendable& B) { return B.Object == CompositeMID; });
	}
	if (CaptureL) { CaptureL->DestroyComponent(); }
	if (CaptureR) { CaptureR->DestroyComponent(); }
	Super::EndPlay(EndPlayReason);
}
