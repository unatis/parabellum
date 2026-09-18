#include "Player/PBLPlayerController.h"

#include "EngineUtils.h"
#include "Collection/PBLCollection.h"
#include "UI/SPBLShopPanel.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Engine/GameInstance.h"
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
		InputComponent->BindKey(EKeys::F, IE_Pressed, this, &APBLPlayerController::BenchFire);
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &APBLPlayerController::BenchSlowMo);
		InputComponent->BindKey(EKeys::C, IE_Pressed, this, &APBLPlayerController::BenchCutaway);
		InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &APBLPlayerController::BenchSpecimenNext);
		InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &APBLPlayerController::BenchSpecimenPrev);
		InputComponent->BindKey(EKeys::F4, IE_Pressed, this, &APBLPlayerController::ToggleShopPanel);
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

void APBLPlayerController::ToggleShopPanel()
{
	if (!GEngine || !GEngine->GameViewport) { return; }
	if (ShopPanel.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ShopPanel.ToSharedRef());
		ShopPanel.Reset();
		// В оружейке курсор нужен и после закрытия магазина, в игре - нет.
		if (bBenchMode) { SetInputMode(FInputModeGameAndUI()); }
		else { SetInputMode(FInputModeGameOnly()); }
		bShowMouseCursor = bBenchMode;
		return;
	}
	TSharedRef<SWidget> Panel = SNew(SConstraintCanvas)
		+ SConstraintCanvas::Slot().Anchors(FAnchors(0.5f, 0.5f)).Alignment(FVector2D(0.5f, 0.5f)).AutoSize(true)
		[ SNew(SPBLShopPanel).Controller(this) ];
	ShopPanel = Panel;
	GEngine->GameViewport->AddViewportWidgetContent(Panel, 110);
	UE_LOG(LogTemp, Display, TEXT("PBL: магазин открыт"));
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

static FAutoConsoleCommandWithWorld CmdShopPanel(TEXT("pbl.Shop.Panel"), TEXT("Toggle the shop window (same as F4)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (APBLPlayerController* PC = World ? Cast<APBLPlayerController>(World->GetFirstPlayerController()) : nullptr)
		{
			PC->ToggleShopPanel();
		}
	}));

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
		// Стенд показывает то, что куплено: список образцов ведёт коллекция, а не уровень.
		BenchSpecimen(0);
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
	DragDistance = 0.0f;
	float X, Y;
	if (GetMousePosition(X, Y)) { LastMouse = FVector2D(X, Y); }
}

void APBLPlayerController::BenchDragStop()
{
	// Потянули - вращали образец; кликнули без движения - снимаем деталь под курсором.
	if (bBenchMode && bDragging && DragDistance < 6.0f && Bench.IsValid())
	{
		const FName Part = Bench->GetHovered();
		FString Reason;
		if (!Part.IsNone() && !Bench->TryTakePart(Part, Reason))
		{
			UE_LOG(LogTemp, Display, TEXT("PBL Bench: %s - %s"), *Bench->GetHoveredName(), *Reason);
		}
	}
	bDragging = false;
}

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
	if (!Bench.IsValid()) { return; }

	// Наведение курсором: подсвечиваем деталь под ним.
	FVector WorldPos, WorldDir;
	if (DeprojectMousePositionToWorld(WorldPos, WorldDir))
	{
		Bench->TraceHover(WorldPos, WorldPos + WorldDir * 100000.0f);
	}
	if (!bDragging) { return; }
	float X, Y;
	if (!GetMousePosition(X, Y)) { return; }
	const FVector2D Now(X, Y);
	const FVector2D D = Now - LastMouse;
	LastMouse = Now;
	// Тянем мышью - образец поворачивается за ней.
	DragDistance += D.Size();
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

// Проверка наведения без мыши: луч из камеры в центр названной детали.
static FAutoConsoleCommandWithWorldAndArgs CmdBenchHover(TEXT("pbl.Bench.Hover"),
	TEXT("Hover (and try to take) a part by name: pbl.Bench.Hover Slide"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APBLPlayerController* PC = World ? Cast<APBLPlayerController>(World->GetFirstPlayerController()) : nullptr;
		APBLWeaponBench* B = PC ? PC->GetBench() : nullptr;
		if (!PC || !B || Args.Num() < 1) { return; }
		FVector Target;
		if (!B->GetPartCenter(FName(*Args[0]), Target)) { UE_LOG(LogTemp, Warning, TEXT("PBL Bench: нет детали %s"), *Args[0]); return; }
		FVector Loc; FRotator Rot;
		PC->GetPlayerViewPoint(Loc, Rot);
		const FVector Dir = (Target - Loc).GetSafeNormal();
		const FName P = B->TraceHover(Loc, Loc + Dir * 100000.0f);
		UE_LOG(LogTemp, Display, TEXT("PBL Bench hover: [%s] %s"), *B->GetHoveredName(), *B->GetHoveredDescription());
		FString Reason;
		if (!P.IsNone()) { B->TryTakePart(P, Reason); UE_LOG(LogTemp, Display, TEXT("PBL Bench take: %s"), *B->GetMessage()); }
	}));

void APBLPlayerController::BenchFire()
{
	if (bBenchMode && Bench.IsValid())
	{
		Bench->FireCycle();
		UE_LOG(LogTemp, Display, TEXT("PBL Bench: %s"), *Bench->GetMessage());
	}
}

void APBLPlayerController::BenchSlowMo()
{
	if (bBenchMode && Bench.IsValid()) { Bench->CycleSlowMotion(); }
}

static FAutoConsoleCommandWithWorld CmdBenchFire(
	TEXT("pbl.Bench.Fire"),
	TEXT("Run the action cycle on the armory bench"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (APBLPlayerController* PC = W ? Cast<APBLPlayerController>(W->GetFirstPlayerController()) : nullptr) { PC->BenchFire(); }
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdBenchFreeze(
	TEXT("pbl.Bench.Freeze"),
	TEXT("Hold the action cycle at a given millisecond: pbl.Bench.Freeze <ms>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
	{
		APBLPlayerController* PC = W ? Cast<APBLPlayerController>(W->GetFirstPlayerController()) : nullptr;
		APBLWeaponBench* B = PC ? PC->GetBench() : nullptr;
		if (!B) { UE_LOG(LogTemp, Warning, TEXT("pbl.Bench.Freeze: сначала F3")); return; }
		B->FreezeCycle(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f);
		UE_LOG(LogTemp, Display, TEXT("PBL Bench: %s"), *B->GetMessage());
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdBenchSlow(
	TEXT("pbl.Bench.Slow"),
	TEXT("Playback slow motion factor for the action cycle: pbl.Bench.Slow <scale>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
	{
		APBLPlayerController* PC = W ? Cast<APBLPlayerController>(W->GetFirstPlayerController()) : nullptr;
		if (APBLWeaponBench* B = PC ? PC->GetBench() : nullptr) { B->SetSlowMotion(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.1f); }
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdBenchRotate(
	TEXT("pbl.Bench.Rotate"),
	TEXT("Turn the specimen on the bench: pbl.Bench.Rotate <yaw> [pitch]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
	{
		APBLPlayerController* PC = W ? Cast<APBLPlayerController>(W->GetFirstPlayerController()) : nullptr;
		if (APBLWeaponBench* B = PC ? PC->GetBench() : nullptr)
		{
			B->AddRotation(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f,
			               Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f);
		}
	}));

void APBLPlayerController::BenchCutaway()
{
	if (bBenchMode && Bench.IsValid()) { Bench->ToggleCutaway(); }
}

static FAutoConsoleCommandWithWorld CmdBenchCutaway(
	TEXT("pbl.Bench.Cutaway"),
	TEXT("Hide the outer shells so the mechanism and ammunition are visible"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (APBLPlayerController* PC = W ? Cast<APBLPlayerController>(W->GetFirstPlayerController()) : nullptr) { PC->BenchCutaway(); }
	}));

// ---------------------------------------------------------------------------------------------
// Коллекция: какой образец стоит на стенде, и магазин, где образцы покупаются.

void APBLPlayerController::BenchSpecimen(int32 Direction)
{
	UPBLCollectionSubsystem* Coll = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLCollectionSubsystem>() : nullptr;
	UPBLWeaponDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	APBLWeaponBench* B = Bench.Get();
	if (!Coll || !Data || !B) { return; }

	const FName Want = (Direction == 0) ? Coll->Active() : Coll->NextOwned(Direction);
	if (Want.IsNone())
	{
		B->SetSpecimen(NAME_None, NAME_None, FString());
		return;
	}
	Coll->SetActive(Want);
	const FPBLCatalogueEntry* E = Data->FindCatalogue(Want);
	if (!E) { return; }
	B->SetSpecimen(E->Weapon, E->Generation, E->PartsPath);
	UE_LOG(LogTemp, Display, TEXT("PBL Bench: %s (%d из %d в коллекции)"),
		*E->DisplayName, Coll->Owned().IndexOfByKey(Want) + 1, Coll->Owned().Num());
}

static FAutoConsoleCommandWithWorld CmdShop(
	TEXT("pbl.Shop"),
	TEXT("List the catalogue: what can be bought into the collection"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
		UPBLWeaponDataSubsystem* Data = GI ? GI->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
		UPBLCollectionSubsystem* Coll = GI ? GI->GetSubsystem<UPBLCollectionSubsystem>() : nullptr;
		if (!Data || !Coll) { return; }
		UE_LOG(LogTemp, Display, TEXT("SHOP: счёт %d, в коллекции %d"), Coll->Balance(), Coll->Owned().Num());
		for (const FPBLCatalogueEntry& E : Data->Catalogue())
		{
			const TCHAR* State = Coll->IsOwned(E.Weapon) ? TEXT("КУПЛЕН")
				: (E.IsAvailable() ? TEXT("в продаже") : TEXT("модели нет"));
			UE_LOG(LogTemp, Display, TEXT("SHOP  %-18s %-22s %4d г, %-8s %-14s %6d  %s"),
				*E.Weapon.ToString(), *E.DisplayName, E.Year, *E.Country, *E.Cartridge.ToString(), E.Price, State);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdShopBuy(
	TEXT("pbl.Shop.Buy"),
	TEXT("Buy a specimen into the collection: pbl.Shop.Buy <weapon>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
	{
		UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
		UPBLCollectionSubsystem* Coll = GI ? GI->GetSubsystem<UPBLCollectionSubsystem>() : nullptr;
		if (!Coll || Args.Num() < 1) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Shop.Buy <weapon>")); return; }
		FString Reason;
		const bool bOk = Coll->Buy(FName(*Args[0]), Reason);
		UE_LOG(LogTemp, Display, TEXT("SHOP %s: %s"), bOk ? TEXT("OK") : TEXT("отказ"), *Reason);
	}));

static FAutoConsoleCommandWithWorld CmdCollection(
	TEXT("pbl.Collection"),
	TEXT("List the specimens owned by the player"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
		UPBLCollectionSubsystem* Coll = GI ? GI->GetSubsystem<UPBLCollectionSubsystem>() : nullptr;
		if (!Coll) { return; }
		UE_LOG(LogTemp, Display, TEXT("COLLECTION: %d образцов, счёт %d, на стенде %s"),
			Coll->Owned().Num(), Coll->Balance(), *Coll->Active().ToString());
		for (const FName& N : Coll->Owned()) { UE_LOG(LogTemp, Display, TEXT("COLLECTION  %s"), *N.ToString()); }
	}));
