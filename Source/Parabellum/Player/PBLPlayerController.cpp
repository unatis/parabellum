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

	if (DefaultMappingContext.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PBLPlayerController: DefaultMappingContext не задан — управления не будет. "
				 "Ожидается на Э2.1."));
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (UInputMappingContext* Context = DefaultMappingContext.LoadSynchronous())
		{
			Subsystem->AddMappingContext(Context, DefaultMappingPriority);
		}
	}
}

void APBLPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Привязки появятся на Э2.1 (движение) и Э4 (стрельба).
}
