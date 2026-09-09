#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PBLCharacter.generated.h"

class UCameraComponent;

/**
 * Базовый персонаж Parabellum.
 *
 * Ответственность: тело в мире и его состояние — движение, здоровье, оружие в руках.
 * Всё это авторитетно на сервере и реплицируется владельцу и остальным.
 *
 * Сюда НЕ кладём: обработку клавиш (это APBLPlayerController), правила матча
 * (это APBLGameMode), рисование HUD. Персонаж не знает, живой игрок им управляет
 * или бот — иначе сеть и AI разъедутся.
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APBLCharacter();

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:
	virtual void BeginPlay() override;

	/**
	 * Камера от первого лица. Живёт на пешке, а не на контроллере: она следует
	 * за телом, и на Э6 к ней прицепится VisionComponent с боковыми захватами.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parabellum|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Высота глаз над низом капсулы. Подбирается на Э2 вместе с размером капсулы. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Camera")
	float CameraHeight = 64.0f;

	/**
	 * FOV центрального рендера. По спеке зрения — 100–110°.
	 * До Э6 это просто широкий обзор, после станет центральной зоной композита.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Camera")
	float CenterFieldOfView = 103.0f;
};
