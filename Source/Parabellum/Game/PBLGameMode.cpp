#include "Game/PBLGameMode.h"

#include "Character/PBLCharacter.h"
#include "Player/PBLPlayerController.h"

APBLGameMode::APBLGameMode()
{
	DefaultPawnClass = APBLCharacter::StaticClass();
	PlayerControllerClass = APBLPlayerController::StaticClass();
}
