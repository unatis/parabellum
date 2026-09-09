#include "Player/PBLPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

APBLPlayerController::APBLPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
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
	UE_LOG(LogTemp, Display, TEXT("PBL: MappingContext '%s' добавлен (priority %d), маппингов: %d"),
		*Context->GetName(), DefaultMappingPriority, Context->GetMappings().Num());
}

void APBLPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Привязки появятся на Э2.1 (движение) и Э4 (стрельба).
}
