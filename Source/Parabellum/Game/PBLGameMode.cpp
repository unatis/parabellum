#include "Game/PBLGameMode.h"

#include "Character/PBLCharacter.h"
#include "Player/PBLPlayerController.h"
#include "Player/PBLHUD.h"
#include "Player/PBLPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

APBLGameMode::APBLGameMode()
{
	DefaultPawnClass = APBLCharacter::StaticClass();
	PlayerControllerClass = APBLPlayerController::StaticClass();
	HUDClass = APBLHUD::StaticClass();
	PlayerStateClass = APBLPlayerState::StaticClass();
}

AActor* APBLGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	FString WantedTag;
	if (FParse::Value(FCommandLine::Get(), TEXT("PBLPlayerStart="), WantedTag) && !WantedTag.IsEmpty())
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == FName(*WantedTag))
			{
				return *It;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("PBL: PlayerStart с тегом '%s' не найден, спавн по умолчанию"), *WantedTag);
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
