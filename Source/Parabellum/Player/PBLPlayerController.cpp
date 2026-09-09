#include "Player/PBLPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
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
