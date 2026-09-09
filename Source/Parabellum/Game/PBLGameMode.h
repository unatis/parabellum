#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PBLGameMode.generated.h"

/**
 * Режим игры.
 *
 * Ответственность: правила и всё, что решается авторитетно — кто где спавнится,
 * что считается попаданием, когда раунд закончен. Существует ТОЛЬКО на сервере;
 * на клиентах его нет вообще. Поэтому здесь не должно быть ничего, что нужно
 * клиенту для отрисовки — такое живёт в GameState.
 */
UCLASS()
class PARABELLUM_API APBLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APBLGameMode();

	/** -PBLPlayerStart=<PlayerStartTag> выбирает точку спавна детерминированно (для скриншотов и A/B). */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
