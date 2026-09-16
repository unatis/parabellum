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

	/** Хит-маркер: четыре диагональных штриха вокруг прицела на HitMarkerTime секунд. */
	void ShowHitMarker(bool bHead, bool bKill);

protected:
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") float CrosshairGap = 4.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") float CrosshairLength = 7.0f;
	/** Крестик. По умолчанию выключен: от бедра целимся по стволу, при прицеливании - по мушке. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|HUD") bool bShowCrosshair = false;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") float CrosshairThickness = 2.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Crosshair") FLinearColor CrosshairColor = FLinearColor(0.2f, 1.0f, 0.2f, 0.9f);

	UPROPERTY(Config, EditDefaultsOnly, Category = "HitMarker") float HitMarkerTime = 0.12f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "HitMarker") float HitMarkerSize = 9.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "HitMarker") FLinearColor HitMarkerColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.95f);
	UPROPERTY(Config, EditDefaultsOnly, Category = "HitMarker") FLinearColor HitMarkerHeadColor = FLinearColor(1.0f, 0.25f, 0.2f, 1.0f);

	float HitMarkerUntil = -1.0f;
	bool bHitMarkerHead = false;
};
