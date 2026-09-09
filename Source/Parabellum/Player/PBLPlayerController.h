#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PBLPlayerController.generated.h"

class UInputMappingContext;

/**
 * Контроллер игрока.
 *
 * Ответственность: какие клавиши что значат (Mapping Context) и всё сугубо
 * локальное - настройки мыши, HUD, меню. Существует на сервере и на владеющем
 * клиенте, но НЕ на чужих клиентах.
 *
 * Сами действия (Move, Jump...) к движению привязывает APBLCharacter - это
 * конвенция UE. Сюда НЕ кладём: состояние персонажа (оно в APBLCharacter,
 * иначе не реплицируется другим игрокам) и правила матча (они в APBLGameMode).
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APBLPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/**
	 * Контекст ввода Enhanced Input. Это .uasset, поэтому агент его не пишет —
	 * ассет создаётся Python-скриптом (Э2.1), а сюда подставляется путём из .ini.
	 * Мягкая ссылка: если ассета ещё нет, игра стартует без ввода, а не падает.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Приоритет контекста. Пригодится, когда появятся режимы (меню, прицел). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	int32 DefaultMappingPriority = 0;
};
