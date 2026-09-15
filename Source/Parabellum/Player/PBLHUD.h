#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PBLHUD.generated.h"

/**
 * Минимальный HUD: статичный крест по центру и патроны. Рисуется Canvas'ом - никаких виджетов/ассетов.
 * Крест стоит ровно в центре экрана: при включённой системе зрения центр не пересчитывается.
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") float CrosshairGap = 4.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") float CrosshairLength = 7.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") float CrosshairThickness = 2.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") FLinearColor CrosshairColor = FLinearColor(0.2f, 1.0f, 0.2f, 0.9f);
};
