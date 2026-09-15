#include "Player/PBLHUD.h"

#include "Character/PBLCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Weapons/PBLWeapon.h"
#include "Player/PBLPlayerState.h"

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

	// Хит-маркер: X из четырёх диагональных штрихов
	if (GetWorld() && GetWorld()->GetTimeSeconds() < HitMarkerUntil)
	{
		const FLinearColor HC = bHitMarkerHead ? HitMarkerHeadColor : HitMarkerColor;
		const float S0 = G + 3.0f, S1 = G + 3.0f + HitMarkerSize;
		DrawLine(CX - S0, CY - S0, CX - S1, CY - S1, HC, T);
		DrawLine(CX + S0, CY - S0, CX + S1, CY - S1, HC, T);
		DrawLine(CX - S0, CY + S0, CX - S1, CY + S1, HC, T);
		DrawLine(CX + S0, CY + S0, CX + S1, CY + S1, HC, T);
	}

	const APBLCharacter* C = Cast<APBLCharacter>(GetOwningPawn());
	const APBLWeapon* W = C ? C->GetWeapon() : nullptr;
	if (W && GEngine && GEngine->GetLargeFont())
	{
		const FString Ammo = W->IsReloading() ? TEXT("RELOAD") : FString::Printf(TEXT("%d / %d"), W->GetAmmoInMag(), W->GetMagSize());
		DrawText(Ammo, FLinearColor::White, Canvas->ClipX - 140.0f, Canvas->ClipY - 60.0f, GEngine->GetLargeFont(), 1.5f);
	}
	if (const APBLPlayerState* PS = GetOwningPlayerController() ? GetOwningPlayerController()->GetPlayerState<APBLPlayerState>() : nullptr)
	{
		const FString Score = FString::Printf(TEXT("HITS %d   HS %d   KILLS %d"), PS->Hits, PS->Headshots, PS->Kills);
		DrawText(Score, FLinearColor(0.9f, 0.9f, 0.9f, 0.8f), 30.0f, Canvas->ClipY - 60.0f, GEngine->GetLargeFont(), 1.2f);
	}
}

void APBLHUD::ShowHitMarker(bool bHead, bool bKill)
{
	HitMarkerUntil = GetWorld()->GetTimeSeconds() + (bKill ? HitMarkerTime * 2.0f : HitMarkerTime);
	bHitMarkerHead = bHead || bKill;
}
