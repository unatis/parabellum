#include "Player/PBLPlayerController.h"

#include "Camera/CameraActor.h"
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

	// Привязки появятся на Э2.1 (движение) и Э4 (стрельба).
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

	// -PBLAutoFire=N: N выстрелов по одному в секунду, начиная за 6 с до снимка (самопроверка стрельбы).
	int32 AutoFire = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("PBLAutoFire="), AutoFire) && AutoFire > 0)
	{
		const float Start = FMath::Max(AfterSeconds - 6.0f, 1.0f);
		for (int32 i = 0; i < AutoFire; ++i)
		{
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, ApplyLook]()
			{
				ApplyLook();   // перед каждым выстрелом возвращаем прицел - отдача предыдущего не копится
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
			FScreenshotRequest::RequestScreenshot(Name, false, false);
			UE_LOG(LogTemp, Display, TEXT("PBL: screenshot requested"));
			FTimerHandle Q;
			GetWorldTimerManager().SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				ConsoleCommand(TEXT("quit"));
			}), 2.0f, false);
		}), 0.3f, false);
	}), AfterSeconds, false);
}
