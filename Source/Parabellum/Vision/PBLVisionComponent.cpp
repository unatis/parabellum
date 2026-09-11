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
// Боковые камеры: поворот и FOV на лету (-1 = из настроек). FOV меньше нужного даст чёрные дыры, больше - хуже разрешение боков.
static TAutoConsoleVariable<float> CVarSideYaw(TEXT("pbl.Vision.SideYaw"), -1.0f, TEXT("Side capture yaw (deg), -1 = settings"));
static TAutoConsoleVariable<float> CVarSideFOV(TEXT("pbl.Vision.SideFOV"), -1.0f, TEXT("Side capture FOV (deg), -1 = settings"));
static TAutoConsoleVariable<int32> CVarPBLVisionDebug(TEXT("pbl.VisionDebug"), -1, TEXT("Foveal vision debug view"));
// Живой подбор проекции из консоли; отрицательное = брать из настроек.
static TAutoConsoleVariable<float> CVarSideBack(TEXT("pbl.Vision.SideBack"), 0.0f, TEXT("Side cameras offset backwards along view, cm"));
static TAutoConsoleVariable<int32> CVarDynamic(TEXT("pbl.Vision.Dynamic"), -1, TEXT("Dynamic peripheral FOV on turning: -1 settings, 0 off, 1 on"));
static TAutoConsoleVariable<int32> CVarMode(TEXT("pbl.Vision.Mode"), -1, TEXT("0 Panini, 1 flat panels; -1 settings"));
static TAutoConsoleVariable<float> CVarPanelFrac(TEXT("pbl.Vision.PanelFrac"), -1.0f, TEXT("Panel mode: center panel width fraction"));
static TAutoConsoleVariable<float> CVarPanelScale(TEXT("pbl.Vision.PanelScale"), -1.0f, TEXT("Panel mode: density vs CS"));
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
		// FinalColorHDR: линейный HDR после постобработки - ради TAA/TSR (иначе Lumen-шум на боках).
		// Экспозиция в нём уже применена; композит делит на EyeAdaptationLookup() (SideExposureFix).
		Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorHDR;
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
		Cap->ShowFlags.SetTemporalAA(true);
		Cap->ShowFlags.SetAntiAliasing(true);
		// Lumen на боках ОСТАВЛЯЕМ: без него тени на боках темнее, чем в центре, и на стыке видна
		// вертикальная тёмная полоса (2026-09-10). "Шум Lumen" оказался крапиной материала пола.
		Cap->RegisterComponent();
		return Cap;
	}

	UTextureRenderTarget2D* MakeSideRT(UObject* Outer, const TCHAR* Name, int32 Size)
	{
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Outer, Name);
		RT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
		RT->ClearColor = FLinearColor::Black;
		RT->bAutoGenerateMips = true;   // мипы = дешёвое периферийное размытие при чтении (SideMip)
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

	VisionCenterFOV = Camera->FieldOfView;
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
	// Выключено = честный CS (fov 90 при реальном аспекте), а не широкая 120-градусная камера покрытия.
	if (Camera)
	{
		float Aspect = 16.0f / 9.0f;
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			const FIntPoint V = GEngine->GameViewport->Viewport->GetSizeXY();
			if (V.X > 0 && V.Y > 0) { Aspect = (float)V.X / (float)V.Y; }
		}
		const float OffFov = CSEquivalentHFov(GetDefault<UPBLVisionSettings>()->CSFov, Aspect);
		Camera->SetFieldOfView(bEnable ? VisionCenterFOV : OffFov);
		UE_LOG(LogTemp, Display, TEXT("PBL Vision: %s, camera FOV %.1f"), bEnable ? TEXT("ON") : TEXT("OFF (CS-equivalent)"), Camera->FieldOfView);
	}
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
	const float SideYaw = Pick(CVarSideYaw, S->SideYaw);
	const float SideFOV = Pick(CVarSideFOV, S->SideFOV);
	CompositeMID->SetScalarParameterValue(TEXT("SideYaw"), SideYaw);
	CompositeMID->SetScalarParameterValue(TEXT("SideFOV"), SideFOV);
	if (CaptureL && !FMath::IsNearlyEqual(CaptureL->FOVAngle, SideFOV)) { CaptureL->FOVAngle = SideFOV; }
	if (CaptureR && !FMath::IsNearlyEqual(CaptureR->FOVAngle, SideFOV)) { CaptureR->FOVAngle = SideFOV; }
	const float MaxFOV = Pick(CVarTotalFOV, S->TotalFOV);
	const int32 CVD = CVarDynamic.GetValueOnGameThread();
	const bool bDynamic = CVD >= 0 ? (CVD != 0) : S->bDynamicFOV;
	const float TotalFOV = bDynamic ? UpdateDynamicFOV(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f, MaxFOV, Aspect) : MaxFOV;
	// d Панини вычисляется из плотности центра (CSFov) и TotalFOV; pbl.Vision.D > 0 - ручное переопределение.
	const float DSolved = SolvePaniniD(TotalFOV, S->CSFov, Aspect);
	const float PaniniD = CVarPaniniD.GetValueOnGameThread() > 0.0f ? CVarPaniniD.GetValueOnGameThread() : DSolved;
	CompositeMID->SetScalarParameterValue(TEXT("TotalFOV"), TotalFOV);
	CompositeMID->SetScalarParameterValue(TEXT("PaniniD"), PaniniD);
	if (!FMath::IsNearlyEqual(PaniniD, LastLoggedD, 0.01f))
	{
		LastLoggedD = PaniniD;
		UE_LOG(LogTemp, Display, TEXT("PBL Vision: TotalFOV %.0f, CSFov %.0f @ aspect %.3f -> Panini d = %.3f"), TotalFOV, S->CSFov, Aspect, PaniniD);
	}
	// Ось проекции привязана к вертикали мира: шейдеру нужен наклон камеры, а боковым камерам -
	// поворот вокруг вертикали системы проекции, не камеры.
	const float CamPitch = FRotator::NormalizeAxis(Camera->GetComponentRotation().Pitch);
	CompositeMID->SetScalarParameterValue(TEXT("CamPitch"), CamPitch);
	CompositeMID->SetScalarParameterValue(TEXT("SideExposureFix"), 1.0f);
	CompositeMID->SetScalarParameterValue(TEXT("SideMip"), S->SideMip);
	const int32 CVM = CVarMode.GetValueOnGameThread();
	const int32 Mode = CVM >= 0 ? CVM : S->Mode;
	CompositeMID->SetScalarParameterValue(TEXT("Mode"), (float)Mode);
	CompositeMID->SetScalarParameterValue(TEXT("PanelCenterFrac"), Pick(CVarPanelFrac, S->PanelCenterFrac));
	CompositeMID->SetScalarParameterValue(TEXT("PanelScale"), Pick(CVarPanelScale, S->PanelScale));
	CompositeMID->SetScalarParameterValue(TEXT("PanelSeparator"), S->PanelSeparator);
	CompositeMID->SetScalarParameterValue(TEXT("CSHalfH"), 0.5f * CSEquivalentHFov(S->CSFov, Aspect));
	// В режиме панелей боковые камеры жёстко в системе камеры (как три монитора), без мировой оси.
	UpdateSideRotation(Mode == 1 ? 0.0f : EffectivePitch(CamPitch));
	CompositeMID->SetScalarParameterValue(TEXT("PitchAlignStart"), S->PitchAlignStart);
	CompositeMID->SetScalarParameterValue(TEXT("PitchAlignEnd"), S->PitchAlignEnd);
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

float UPBLVisionComponent::EffectivePitch(float CamPitchDeg)
{
	const UPBLVisionSettings* S = GetDefault<UPBLVisionSettings>();
	const float T = FMath::Clamp((FMath::Abs(CamPitchDeg) - S->PitchAlignStart) / FMath::Max(S->PitchAlignEnd - S->PitchAlignStart, 0.01f), 0.0f, 1.0f);
	const float Smooth = T * T * (3.0f - 2.0f * T);
	return CamPitchDeg * (1.0f - Smooth);
}

void UPBLVisionComponent::UpdateSideRotation(float EffectivePitchDeg)
{
	if (!Camera) { return; }
	const UPBLVisionSettings* S = GetDefault<UPBLVisionSettings>();
	// F = камера, "разогнутая" на p0e; бок = F -> yaw +-SideYaw -> pitch p0e. Зеркало шейдера.
	const FQuat QF = Camera->GetComponentQuat() * FRotator(-EffectivePitchDeg, 0.0f, 0.0f).Quaternion();
	const FQuat QPitch = FRotator(EffectivePitchDeg, 0.0f, 0.0f).Quaternion();
	const float SideYaw = Pick(CVarSideYaw, S->SideYaw);
	if (CaptureL) { CaptureL->SetWorldRotation(QF * FRotator(0.0f, -SideYaw, 0.0f).Quaternion() * QPitch); }
	if (CaptureR) { CaptureR->SetWorldRotation(QF * FRotator(0.0f,  SideYaw, 0.0f).Quaternion() * QPitch); }
	// Сдвиг боковых камер назад вдоль взгляда (эксперимент пользователя). Даёт параллакс на стыках.
	const FVector Back(-FMath::Max(CVarSideBack.GetValueOnGameThread(), 0.0f), 0.0f, 0.0f);
	if (CaptureL) { CaptureL->SetRelativeLocation(Back); }
	if (CaptureR) { CaptureR->SetRelativeLocation(Back); }
}

float UPBLVisionComponent::CSEquivalentHFov(float CSFov, float Aspect)
{
	// CS задаёт fov для 4:3: вертикаль = 2*atan(tan(fov/2)*3/4), горизонталь при аспекте = 2*atan(tan(v/2)*aspect).
	const float HalfV = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(CSFov) * 0.5f) * 0.75f);
	return FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(HalfV) * Aspect));
}

float UPBLVisionComponent::SolvePaniniD(float TotalFOV, float CSFov, float Aspect)
{
	const float HalfH = FMath::DegreesToRadians(CSEquivalentHFov(CSFov, Aspect)) * 0.5f;
	const float Target = 1.0f / FMath::Tan(HalfH);          // sN: доля полуширины на tan(1 rad) в центре
	const float E = FMath::DegreesToRadians(FMath::Clamp(TotalFOV, 90.0f, 179.0f) * 0.5f);
	const float SinE = FMath::Sin(E), CosE = FMath::Cos(E);
	// sN(d) = (d+cos e)/((d+1) sin e) растёт по d от cos e/sin e к 1/sin e.
	auto SN = [&](float D) { return (D + CosE) / ((D + 1.0f) * SinE); };
	if (Target <= SN(0.01f)) { return 0.01f; }
	if (Target >= SN(1000.0f)) { return 1000.0f; }
	float Lo = 0.01f, Hi = 1000.0f;
	for (int32 i = 0; i < 60; ++i)
	{
		const float Mid = 0.5f * (Lo + Hi);
		if (SN(Mid) < Target) { Lo = Mid; } else { Hi = Mid; }
	}
	return 0.5f * (Lo + Hi);
}

float UPBLVisionComponent::UpdateDynamicFOV(float DeltaTime, float TargetMaxFOV, float Aspect)
{
	const UPBLVisionSettings* S = GetDefault<UPBLVisionSettings>();
	const float RestFOV = S->RestFOV > 0.0f ? S->RestFOV : CSEquivalentHFov(S->CSFov, Aspect);
	if (!Camera) { return RestFOV; }

	const FRotator Rot = Camera->GetComponentRotation();
	float SpeedDeg = 0.0f;
	if (bHasLastCamRot && DeltaTime > 1e-4f)
	{
		const float DYaw = FMath::FindDeltaAngleDegrees(LastCamRot.Yaw, Rot.Yaw);
		const float DPitch = FMath::FindDeltaAngleDegrees(LastCamRot.Pitch, Rot.Pitch);
		SpeedDeg = FMath::Sqrt(DYaw * DYaw + DPitch * DPitch) / DeltaTime;
	}
	LastCamRot = Rot;
	bHasLastCamRot = true;

	const float Open = FMath::Clamp(SpeedDeg / FMath::Max(S->TurnSpeedFull, 1.0f), 0.0f, 1.0f);
	const float Target = FMath::Lerp(RestFOV, FMath::Max(TargetMaxFOV, RestFOV), Open);
	if (DynamicFOV <= 0.0f) { DynamicFOV = RestFOV; }
	const float Speed = Target > DynamicFOV ? S->OpenSpeed : S->CloseSpeed;
	DynamicFOV = FMath::FInterpTo(DynamicFOV, Target, DeltaTime, Speed);
	return DynamicFOV;
}
