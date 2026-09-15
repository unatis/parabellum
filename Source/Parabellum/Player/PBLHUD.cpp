#include "Player/PBLHUD.h"

#include "Character/PBLCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Weapons/PBLWeapon.h"

void APBLHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float T = CrosshairThickness;
	const float G = CrosshairGap;
	const float L = CrosshairLength;
	// Четыре штриха, как в CS: без центральной точки.
	DrawRect(CrosshairColor, CX - G - L, CY - T * 0.5f, L, T);
	DrawRect(CrosshairColor, CX + G,     CY - T * 0.5f, L, T);
	DrawRect(CrosshairColor, CX - T * 0.5f, CY - G - L, T, L);
	DrawRect(CrosshairColor, CX - T * 0.5f, CY + G,     T, L);

	const APBLCharacter* C = Cast<APBLCharacter>(GetOwningPawn());
	const APBLWeapon* W = C ? C->GetWeapon() : nullptr;
	if (W && GEngine && GEngine->GetLargeFont())
	{
		const FString Ammo = W->IsReloading() ? TEXT("RELOAD") : FString::Printf(TEXT("%d / %d"), W->GetAmmoInMag(), W->GetMagSize());
		DrawText(Ammo, FLinearColor::White, Canvas->ClipX - 140.0f, Canvas->ClipY - 60.0f, GEngine->GetLargeFont(), 1.5f);
	}
}
