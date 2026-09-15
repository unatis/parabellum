#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PBLPlayerState.generated.h"

/** Счёт игрока. Живёт на сервере, реплицируется всем - так HUD и табло читают одно и то же. */
UCLASS()
class PARABELLUM_API APBLPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void AddHit(bool bHead) { ++Hits; if (bHead) { ++Headshots; } }
	void AddKill() { ++Kills; }

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Parabellum|Score") int32 Hits = 0;
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Parabellum|Score") int32 Headshots = 0;
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Parabellum|Score") int32 Kills = 0;
};
