#include "Player/PBLPlayerController.h"

#include "EngineUtils.h"
#include "Range/PBLWeaponBench.h"

#include "Camera/CameraActor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "UI/SPBLTuningPanel.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Camera/CameraComponent.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Character/PBLCharacter.h"
#include "Weapons/PBLWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "InputMappingContext.h"

APBLPlayerController::APBLPlayerController()
{
}

void APBLPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Ввод существует только там, где есть живой игрок за экраном.
	if (!IsLocalController())
	{
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("PBL: PlayerController BeginPlay, LocalPlayer=%s, DefaultMappingContext='%s'"),
		GetLocalPlayer() ? TEXT("yes") : TEXT("NO"), *DefaultMappingContext.ToString());

	SetupAutoScreenshot();

	if (DefaultMappingContext.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("PBL: DefaultMappingContext не задан — управления не будет. Ожидается на Э2.1."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("PBL: нет EnhancedInputLocalPlayerSubsystem — плагин EnhancedInput не поднялся?"));
		return;
	}

	UInputMappingContext* Context = DefaultMappingContext.LoadSynchronous();
	if (!Context)
	{
		UE_LOG(LogTemp, Error, TEXT("PBL: не удалось загрузить '%s' — ассет отсутствует? Запусти Tools/make_input_assets.bat"),
			*DefaultMappingContext.ToString());
		return;
	}

	Subsystem->AddMappingContext(Context, DefaultMappingPriority);
	UE_LOG(LogTemp, Display, TEXT("PBL: MappingContext '%s' добавлен (priority %d), маппингов: %d; PlayerInput=%s"),
		*Context->GetName(), DefaultMappingPriority, Context->GetMappings().Num(),
		PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("null"));

	// Пересборка маппингов идёт на следующем тике - проверяем результат с задержкой.
	GetWorldTimerManager().SetTimer(RebuildCheckHandle, this, &APBLPlayerController::LogRebuiltMappings, 1.0f, false);
}

void APBLPlayerController::LogRebuiltMappings()
{
	const UEnhancedPlayerInput* EPI = Cast<UEnhancedPlayerInput>(PlayerInput);
	if (!EPI)
	{
		UE_LOG(LogTemp, Error, TEXT("PBL: PlayerInput класса %s не Enhanced - Enhanced Input не соберёт ни одного маппинга. Проверь DefaultInput.ini"),
			PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("null"));
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("PBL: через 1 c собрано действующих маппингов: %d"), EPI->GetEnhancedActionMappingsView().Num());

	const APawn* P = GetPawn();
	UE_LOG(LogTemp, Display, TEXT("PBL: state paused=%d moveIgnored=%d lookIgnored=%d inputEnabled=%d pawn=%s pawnLoc=%s viewTarget=%s"),
		UGameplayStatics::IsGamePaused(GetWorld()) ? 1 : 0,
		IsMoveInputIgnored() ? 1 : 0, IsLookInputIgnored() ? 1 : 0,
		InputEnabled() ? 1 : 0,
		P ? *P->GetName() : TEXT("NONE"),
		P ? *P->GetActorLocation().ToCompactString() : TEXT("-"),
		GetViewTarget() ? *GetViewTarget()->GetName() : TEXT("NONE"));

}

void APBLPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Движение/стрельба - Enhanced Input в персонаже. F2 - окно испытателя (legacy-привязка работает параллельно).
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &APBLPlayerController::ToggleTuningPanel);
		InputComponent->BindKey(EKeys::F3, IE_Pressed, this, &APBLPlayerController::ToggleBench);
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APBLPlayerController::BenchNext);
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &APBLPlayerController::BenchPrev);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &APBLPlayerController::BenchNext);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APBLPlayerController::BenchPrev);
		InputComponent->BindKey(EKeys::T, IE_Pressed, this, &APBLPlayerController::BenchStage);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &APBLPlayerController::BenchReset);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &APBLPlayerController::BenchDragStart);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &APBLPlayerController::BenchDragStop);
	}
}

void APBLPlayerController::SetupAutoScreenshot()
{
	float AfterSeconds = 0.0f;
	if (!FParse::Value(FCommandLine::Get(), TEXT("PBLScreenshotAfter="), AfterSeconds) || AfterSeconds <= 0.0f)
	{
		return;
	}

	FString Name = TEXT("pbl_auto");
	FParse::Value(FCommandLine::Get(), TEXT("PBLScreenshotName="), Name);

	// Опционально задать взгляд перед снимком (абсолютно, в мире): -PBLLookYaw=<град> -PBLLookPitch=<град>
	float Yaw = 0.0f, Pitch = 0.0f;
	const bool bHasYaw = FParse::Value(FCommandLine::Get(), TEXT("PBLLookYaw="), Yaw);
	const bool bHasPitch = FParse::Value(FCommandLine::Get(), TEXT("PBLLookPitch="), Pitch);

	UE_LOG(LogTemp, Display, TEXT("PBL: auto screenshot '%s' in %.1f s"), *Name, AfterSeconds);

	// Взгляд выставляем сразу (через 1 с, когда пешка захвачена), чтобы автострельба шла в нужную сторону;
	// перед снимком повторим - отдача могла увести.
	auto ApplyLook = [this, Yaw, Pitch, bHasYaw, bHasPitch]()
	{
		if (!bHasYaw && !bHasPitch) { return; }
		FRotator R = GetControlRotation();
		if (bHasYaw) { R.Yaw = Yaw; }
		if (bHasPitch) { R.Pitch = Pitch; }
		SetControlRotation(R);
	};
	FTimerHandle LookEarly;
	GetWorldTimerManager().SetTimer(LookEarly, FTimerDelegate::CreateWeakLambda(this, ApplyLook), 1.0f, false);
	// -PBLAim: выйти в прицел через 1.2 с (проверка позы прицеливания скриншотом).
	if (FParse::Param(FCommandLine::Get(), TEXT("PBLAim")))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (APBLCharacter* C = Cast<APBLCharacter>(GetPawn())) { if (C->GetWeapon()) { C->GetWeapon()->SetAiming(true); } }
		}), 1.2f, false);
	}
	// -PBLExecDelayed="cmd1,cmd2": консольные команды через 2 с (когда мир и игрок готовы) - для проверок UI и стрельбища.
	FString Delayed;
	if (FParse::Value(FCommandLine::Get(), TEXT("PBLExecDelayed="), Delayed, false))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Delayed]()
		{
			TArray<FString> Cmds; Delayed.ParseIntoArray(Cmds, TEXT(","));
			for (const FString& Cmd : Cmds) { ConsoleCommand(Cmd.TrimQuotes()); }
		}), 2.0f, false);
	}

	// -PBLAutoFire=N: N выстрелов по одному в секунду, начиная за 6 с до снимка (самопроверка стрельбы).
	int32 AutoFire = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("PBLAutoFire="), AutoFire) && AutoFire > 0)
	{
		const float Start = FMath::Max(AfterSeconds - 6.0f, 1.5f);   // после применения взгляда (1.0 с) и выхода в прицел (1.2 с)
		for (int32 i = 0; i < AutoFire; ++i)
		{
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, ApplyLook]()
			{
				// Перед каждым выстрелом возвращаем прицел (отдача не копится), если не задан -PBLAutoFireNoReaim (проверка отдачи).
				if (!FParse::Param(FCommandLine::Get(), TEXT("PBLAutoFireNoReaim"))) { ApplyLook(); }
				if (APBLCharacter* C = Cast<APBLCharacter>(GetPawn())) { if (C->GetWeapon()) { C->GetWeapon()->StartFire(); } }
			}), Start + i * 1.0f, false);
		}
	}
	GetWorldTimerManager().SetTimer(ScreenshotHandle, FTimerDelegate::CreateWeakLambda(this, [this, Name, Yaw, Pitch, bHasYaw, bHasPitch]()
	{
		if (bHasYaw || bHasPitch)
		{
			FRotator R = GetControlRotation();
			if (bHasYaw) { R.Yaw = Yaw; }
			if (bHasPitch) { R.Pitch = Pitch; }
			SetControlRotation(R);
		}
		// -PBLScreenshotCamera=X,Y,Z,Pitch,Yaw: снимок с отдельной камеры-наблюдателя (например, сбоку блока геля).
		FString CamSpec;
		if (FParse::Value(FCommandLine::Get(), TEXT("PBLScreenshotCamera="), CamSpec, false))
		{
			TArray<FString> Parts;
			CamSpec.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 5)
			{
				const FVector Loc(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
				const FRotator Rot(FCString::Atof(*Parts[3]), FCString::Atof(*Parts[4]), 0.0f);
				if (ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(Loc, Rot))
				{
					if (Parts.Num() >= 6) { Cam->GetCameraComponent()->SetFieldOfView(FCString::Atof(*Parts[5])); }
					SetViewTargetWithBlend(Cam, 0.0f);
					UE_LOG(LogTemp, Display, TEXT("PBL: screenshot camera at %s rot %s"), *Loc.ToCompactString(), *Rot.ToCompactString());
				}
			}
		}
		// Снимок делается в конце следующего кадра - даём кадру пройти с новым поворотом.
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Name]()
		{
			FScreenshotRequest::RequestScreenshot(Name, true, false);   // bShowUI: Slate-окна (F2) тоже в кадр
			UE_LOG(LogTemp, Display, TEXT("PBL: screenshot requested"));
			FTimerHandle Q;
			GetWorldTimerManager().SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				ConsoleCommand(TEXT("quit"));
			}), 2.0f, false);
		}), 0.3f, false);
	}), AfterSeconds, false);
}

void APBLPlayerController::ToggleTuningPanel()
{
	if (!GEngine || !GEngine->GameViewport) { return; }
	if (TuningPanel.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(TuningPanel.ToSharedRef());
		TuningPanel.Reset();
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		return;
	}
	TSharedRef<SWidget> Panel = SNew(SConstraintCanvas)
		+ SConstraintCanvas::Slot().Anchors(FAnchors(1.0f, 0.0f)).Alignment(FVector2D(1.0f, 0.0f)).AutoSize(true).Offset(FMargin(-20.0f, 20.0f, 0.0f, 0.0f))
		[ SNew(SPBLTuningPanel).Controller(this) ];
	TuningPanel = Panel;
	GEngine->GameViewport->AddViewportWidgetContent(Panel, 100);
	UE_LOG(LogTemp, Display, TEXT("PBL: tuning panel opened"));
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

static FAutoConsoleCommandWithWorld CmdTuning(TEXT("pbl.Tuning"), TEXT("Toggle the weapon tuning panel (same as F2)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (APBLPlayerController* PC = World ? Cast<APBLPlayerController>(World->GetFirstPlayerController()) : nullptr) { PC->ToggleTuningPanel(); }
	}));

// ---------------- Оружейная комната ----------------

void APBLPlayerController::ToggleBench()
{
	if (!GetWorld()) { return; }
	if (!Bench.IsValid())
	{
		for (TActorIterator<APBLWeaponBench> It(GetWorld()); It; ++It) { Bench = *It; break; }
	}
	APBLWeaponBench* B = Bench.Get();
	if (!B) { UE_LOG(LogTemp, Warning, TEXT("PBL: стенд не найден на уровне")); return; }

	bBenchMode = !bBenchMode;
	if (bBenchMode)
	{
		if (!BenchCamera.IsValid())
		{
			BenchCamera = GetWorld()->SpawnActor<ACameraActor>(B->GetActorLocation(), FRotator::ZeroRotator);
		}
		UpdateBenchCamera(1.0f);
		SetViewTargetWithBlend(BenchCamera.Get(), 0.5f);
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		bShowMouseCursor = true;
		UE_LOG(LogTemp, Display, TEXT("PBL: оружейная комната - %s"), *B->GetStatusLine());
	}
	else
	{
		SetViewTargetWithBlend(GetPawn(), 0.4f);
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		bDragging = false;
	}
}

void APBLPlayerController::BenchDragStart()
{
	if (!bBenchMode) { return; }
	bDragging = true;
	float X, Y;
	if (GetMousePosition(X, Y)) { LastMouse = FVector2D(X, Y); }
}

void APBLPlayerController::BenchDragStop() { bDragging = false; }

void APBLPlayerController::BenchNext()
{
	if (bBenchMode && Bench.IsValid()) { Bench->StepForward(); UE_LOG(LogTemp, Display, TEXT("PBL: %s"), *Bench->GetStatusLine()); }
}

void APBLPlayerController::BenchPrev()
{
	if (bBenchMode && Bench.IsValid()) { Bench->StepBack(); UE_LOG(LogTemp, Display, TEXT("PBL: %s"), *Bench->GetStatusLine()); }
}

void APBLPlayerController::BenchStage()
{
	if (bBenchMode && Bench.IsValid()) { Bench->ToggleStage(); UE_LOG(LogTemp, Display, TEXT("PBL: %s"), *Bench->GetStatusLine()); }
}

void APBLPlayerController::BenchReset()
{
	if (bBenchMode && Bench.IsValid()) { Bench->ResetView(); }
}

static FAutoConsoleCommandWithWorld CmdBench(TEXT("pbl.Bench"), TEXT("Toggle the armory bench (same as F3)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (APBLPlayerController* PC = World ? Cast<APBLPlayerController>(World->GetFirstPlayerController()) : nullptr) { PC->ToggleBench(); }
	}));

void APBLPlayerController::UpdateBenchCamera(float Blend)
{
	APBLWeaponBench* B = Bench.Get();
	ACameraActor* Cam = BenchCamera.Get();
	if (!B || !Cam) { return; }
	FVector Center; float Radius;
	B->GetViewFocus(Center, Radius);
	const float FOV = 55.0f;
	const float Dist = Radius / FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) * 1.35f;
	const FVector Goal = Center + FVector(0.0f, -Dist, Radius * 0.12f);
	const FVector Loc = FMath::Lerp(Cam->GetActorLocation(), Goal, FMath::Clamp(Blend, 0.0f, 1.0f));
	Cam->SetActorLocation(Loc);
	Cam->SetActorRotation((Center - Loc).Rotation());
	Cam->GetCameraComponent()->SetFieldOfView(FOV);
}

void APBLPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!bBenchMode) { return; }
	// Камера плавно подстраивается: при разборке детали расходятся, кадр расширяется.
	UpdateBenchCamera(FMath::Clamp(DeltaTime * 3.0f, 0.0f, 1.0f));
	if (!bDragging || !Bench.IsValid()) { return; }
	float X, Y;
	if (!GetMousePosition(X, Y)) { return; }
	const FVector2D Now(X, Y);
	const FVector2D D = Now - LastMouse;
	LastMouse = Now;
	// Тянем мышью - образец поворачивается за ней.
	Bench->AddRotation(-D.X * 0.4f, D.Y * 0.4f);
}

// Консольные команды для проверки без клавиатуры: pbl.Bench.Next / Prev / Stage / Reset
static void BenchCmd(UWorld* World, void (APBLPlayerController::*Fn)())
{
	if (APBLPlayerController* PC = World ? Cast<APBLPlayerController>(World->GetFirstPlayerController()) : nullptr) { (PC->*Fn)(); }
}
static FAutoConsoleCommandWithWorld CmdBenchNext(TEXT("pbl.Bench.Next"), TEXT("Next disassembly step"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { BenchCmd(W, &APBLPlayerController::BenchNext); }));
static FAutoConsoleCommandWithWorld CmdBenchPrev(TEXT("pbl.Bench.Prev"), TEXT("Previous step (assemble)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { BenchCmd(W, &APBLPlayerController::BenchPrev); }));
static FAutoConsoleCommandWithWorld CmdBenchStage(TEXT("pbl.Bench.Stage"), TEXT("Toggle field/full strip"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { BenchCmd(W, &APBLPlayerController::BenchStage); }));
