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
	if (bShowCrosshair)
	{
		// Четыре штриха, как в CS: без центральной точки.
		DrawRect(CrosshairColor, CX - G - L, CY - T * 0.5f, L, T);
		DrawRect(CrosshairColor, CX + G,     CY - T * 0.5f, L, T);
		DrawRect(CrosshairColor, CX - T * 0.5f, CY - G - L, T, L);
		DrawRect(CrosshairColor, CX - T * 0.5f, CY + G,     T, L);
	}

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
	// Хронограф: отчёт о последнем выстреле, 4 с.
	if (W && GetWorld() && GetWorld()->GetTimeSeconds() - W->GetLastReportTime() < 4.0f)
	{
		const FPBLShotReport& R = W->GetLastReport();
		const FString Line1 = FString::Printf(TEXT("V0 %.0f m/s   dist %.1f m   Vimp %.0f m/s   E %.0f J"), R.V0_mps, R.Distance_m, R.ImpactVelocity_mps, R.ImpactEnergy_J);
		const FString Line2 = FString::Printf(TEXT("t %.3f s   drop %+.1f cm   %s"), R.TimeOfFlight_s, R.DropFromLOS_m * 100.0f, R.bHit ? (R.bHitTarget ? TEXT("TARGET") : TEXT("hit")) : TEXT("no hit"));
		DrawText(Line1, FLinearColor(1.0f, 0.9f, 0.5f, 0.9f), 30.0f, 30.0f, GEngine->GetLargeFont(), 1.1f);
		DrawText(Line2, FLinearColor(1.0f, 0.9f, 0.5f, 0.9f), 30.0f, 55.0f, GEngine->GetLargeFont(), 1.1f);
		if (!R.PenetrationMaterial.IsNone())
		{
			FString Line3 = FString::Printf(TEXT("%s: %.1f cm (%.1f in)   %s   dia %.1f mm%s"), *R.PenetrationMaterial.ToString(),
				R.Penetration_m * 100.0f, R.Penetration_m / 0.0254f,
				R.bStoppedInMedium ? TEXT("STOPPED") : *FString::Printf(TEXT("EXIT %.0f m/s"), R.MediumExitVelocity_mps), R.FinalDiameter_mm,
				R.Ricochets > 0 ? *FString::Printf(TEXT("   RICOCHET x%d"), R.Ricochets) : TEXT(""));
			if (R.WoundDamage > 0.0f) { Line3 += FString::Printf(TEXT("   DMG %.0f (%.0f J)"), R.WoundDamage, R.WoundEnergy_J); }
			DrawText(Line3, FLinearColor(1.0f, 0.6f, 0.4f, 0.95f), 30.0f, 80.0f, GEngine->GetLargeFont(), 1.1f);
		}
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
