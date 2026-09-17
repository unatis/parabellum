#include "Ballistics/PBLCycle.h"

#include "Ballistics/PBLBallistics.h"
#include "Ballistics/PBLRecoil.h"
#include "Ballistics/PBLRecoilSettings.h"
#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	constexpr float MaxSubstep = 0.0002f;   // 0.2 мс: цикл длится десятки миллисекунд
}

float PBLCycle::InitialSlideVelocity(const FPBLFirearmData& F, float Impulse_Ns)
{
	// В запертом положении импульс получает всё оружие целиком, и подвижные части уходят назад
	// с этой скоростью относительно руки. Отсюда же берётся свободная скорость отдачи.
	return F.Mass_kg > 0.0f ? Impulse_Ns / F.Mass_kg : 0.0f;
}

void PBLCycle::Fire(FPBLCycleState& S, const FPBLFirearmData& F, float Impulse_Ns)
{
	// Модель верна для отдачи ствола и свободного затвора. Газоотвод приводится давлением через газовый
	// канал, а не свободной отдачей: его схема здесь не считается (см. бэклог).
	if (F.SlideTravel_m <= 0.0f || !F.IsRecoilOperated()) { return; }
	S.Phase = EPBLCyclePhase::Recoiling;
	S.X = 0.0f;
	S.V = InitialSlideVelocity(F, Impulse_Ns);
	S.T = 0.0f;
	S.PeakV = S.V;
	S.MaxX = 0.0f;
	S.CycleTime = 0.0f;
	S.BarrelTiltAlpha = 0.0f;
}

void PBLCycle::Step(FPBLCycleState& S, const FPBLFirearmData& F, float Dt)
{
	if (!S.IsRunning() || F.SlideMass_kg <= 0.0f) { return; }
	const int32 N = FMath::Max(1, FMath::CeilToInt(Dt / MaxSubstep));
	const float h = Dt / N;
	for (int32 i = 0; i < N && S.IsRunning(); ++i)
	{
		const float Spring = F.SpringPreload_N + F.SpringRate_Npm * S.X;
		const float FrictionN = F.SlideResist_N;
		if (S.Phase == EPBLCyclePhase::Recoiling)
		{
			S.V += -(Spring + FrictionN) / F.SlideMass_kg * h;
			S.X += S.V * h;
			S.MaxX = FMath::Max(S.MaxX, S.X);
			if (S.X >= F.SlideTravel_m) { S.X = F.SlideTravel_m; S.V = 0.0f; S.Phase = EPBLCyclePhase::AtRear; }
			else if (S.V <= 0.0f) { S.Phase = EPBLCyclePhase::Returning; S.V = 0.0f; }
		}
		else if (S.Phase == EPBLCyclePhase::AtRear)
		{
			S.Phase = EPBLCyclePhase::Returning;
		}
		else if (S.Phase == EPBLCyclePhase::Returning)
		{
			// Накат: пружина разгоняет затвор вперёд; мешают трение и подача патрона из магазина.
			S.V += -(Spring - FrictionN - F.FeedResist_N) / F.SlideMass_kg * h;
			S.X += S.V * h;
			if (S.X <= 0.0f) { S.X = 0.0f; S.V = 0.0f; S.Phase = EPBLCyclePhase::Ready; S.CycleTime = S.T + h; }
		}
		S.T += h;
		// Ствол опущен ровно на участке после отпирания и до возврата в переднее положение.
		S.BarrelTiltAlpha = (F.UnlockTravel_m > 0.0f)
			? FMath::Clamp((S.X - F.UnlockTravel_m) / FMath::Max(F.UnlockTravel_m, 1e-4f), 0.0f, 1.0f)
			: 0.0f;
	}
}

void PBLCycle::Predict(const FPBLFirearmData& F, float Impulse_Ns, float& OutTravel_m,
	float& OutRecoilTime_s, float& OutCycleTime_s, float& OutMaxRPM)
{
	FPBLCycleState S;
	Fire(S, F, Impulse_Ns);
	OutRecoilTime_s = 0.0f;
	bool bRecoilDone = false;
	for (int32 i = 0; i < 200000 && S.IsRunning(); ++i)
	{
		Step(S, F, MaxSubstep);
		if (!bRecoilDone && S.Phase != EPBLCyclePhase::Recoiling) { OutRecoilTime_s = S.T; bRecoilDone = true; }
	}
	OutTravel_m = S.MaxX;
	OutCycleTime_s = S.CycleTime;
	OutMaxRPM = (S.CycleTime > 0.0f) ? 60.0f / S.CycleTime : 0.0f;
}

// ---------------------------------------------------------------------------------------------------------------------
// pbl.Ballistics.Cycle <firearm> - ход затвора, время цикла и предельный темп из импульса выстрела

static FAutoConsoleCommandWithWorldAndArgs CmdCycle(
	TEXT("pbl.Ballistics.Cycle"),
	TEXT("Action cycle derived from recoil impulse: pbl.Ballistics.Cycle <firearm>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !World || !World->GetGameInstance()) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Ballistics.Cycle <firearm>")); return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		const FPBLFirearmData* F = Data ? Data->FindFirearm(FName(*Args[0])) : nullptr;
		const FPBLCartridgeData* C = F ? Data->FindCartridge(F->Cartridge) : nullptr;
		if (!F || !C) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Cycle: не найдено")); return; }
		if (F->SlideTravel_m <= 0.0f) { UE_LOG(LogTemp, Display, TEXT("CYCLE %s: неавтоматическое оружие"), *F->Name.ToString()); return; }
		if (!F->IsRecoilOperated())
		{
			UE_LOG(LogTemp, Display, TEXT("CYCLE %s: схема %s приводится газом, а не свободной отдачей - не смоделирована"),
				*F->Name.ToString(), *F->ActionScheme.ToString());
			return;
		}

		const float V0 = PBLBallistics::MuzzleVelocity(*C, F->Barrel_m);
		const FPBLRecoilInfo R = PBLRecoil::Compute(*C, *F, V0, UPBLRecoilSettings::Get().Hold(F->Hold));
		float Travel, RecoilT, CycleT, MaxRPM;
		PBLCycle::Predict(*F, R.Impulse_Ns, Travel, RecoilT, CycleT, MaxRPM);
		UE_LOG(LogTemp, Display, TEXT("CYCLE %s (%s): импульс %.2f N*s -> скорость затвора %.2f м/с; масса подвижных частей %.0f г, пружина %.0f Н + %.0f Н/м"),
			*F->Name.ToString(), *F->ActionScheme.ToString(), R.Impulse_Ns, PBLCycle::InitialSlideVelocity(*F, R.Impulse_Ns),
			F->SlideMass_kg * 1000.0f, F->SpringPreload_N, F->SpringRate_Npm);
		UE_LOG(LogTemp, Display, TEXT("CYCLE   ход %.1f мм из %.0f возможных, откат %.1f мс, цикл %.1f мс -> предельный темп %.0f выстр/мин (в данных %d)"),
			Travel * 1000.0f, F->SlideTravel_m * 1000.0f, RecoilT * 1000.0f, CycleT * 1000.0f, MaxRPM, F->RPM);
	}));

float PBLCycle::VelocityAtTravel(const FPBLFirearmData& F, float Impulse_Ns, float Travel_m)
{
	FPBLCycleState S;
	Fire(S, F, Impulse_Ns);
	for (int32 i = 0; i < 100000 && S.Phase == EPBLCyclePhase::Recoiling && S.X < Travel_m; ++i)
	{
		Step(S, F, MaxSubstep);
	}
	return S.Phase == EPBLCyclePhase::Recoiling ? S.V : 0.0f;
}
