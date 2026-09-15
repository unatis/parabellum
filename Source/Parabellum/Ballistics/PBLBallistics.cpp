#include "Ballistics/PBLBallistics.h"

#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	struct FCdPoint { float Mach; float Cd; };

	// Стандартная пуля G1 (таблица McCoy). Mach -> Cd.
	const FCdPoint G1Table[] = {
		{0.00f,0.2629f},{0.05f,0.2558f},{0.10f,0.2487f},{0.15f,0.2413f},{0.20f,0.2344f},{0.25f,0.2278f},{0.30f,0.2214f},{0.35f,0.2155f},
		{0.40f,0.2104f},{0.45f,0.2061f},{0.50f,0.2032f},{0.55f,0.2020f},{0.60f,0.2034f},{0.70f,0.2165f},{0.725f,0.2230f},{0.75f,0.2313f},
		{0.775f,0.2417f},{0.80f,0.2546f},{0.825f,0.2706f},{0.85f,0.2901f},{0.875f,0.3136f},{0.90f,0.3415f},{0.925f,0.3734f},{0.95f,0.4084f},
		{0.975f,0.4448f},{1.00f,0.4805f},{1.025f,0.5136f},{1.05f,0.5427f},{1.075f,0.5677f},{1.10f,0.5883f},{1.125f,0.6053f},{1.15f,0.6191f},
		{1.20f,0.6393f},{1.25f,0.6518f},{1.30f,0.6589f},{1.35f,0.6621f},{1.40f,0.6625f},{1.45f,0.6607f},{1.50f,0.6573f},{1.55f,0.6528f},
		{1.60f,0.6474f},{1.65f,0.6413f},{1.70f,0.6347f},{1.75f,0.6280f},{1.80f,0.6210f},{1.85f,0.6141f},{1.90f,0.6072f},{1.95f,0.6003f},
		{2.00f,0.5934f},{2.05f,0.5867f},{2.10f,0.5804f},{2.15f,0.5743f},{2.20f,0.5685f},{2.25f,0.5630f},{2.30f,0.5577f},{2.35f,0.5527f},
		{2.40f,0.5481f},{2.45f,0.5438f},{2.50f,0.5397f},{2.60f,0.5325f},{2.70f,0.5264f},{2.80f,0.5211f},{2.90f,0.5168f},{3.00f,0.5133f},
		{3.10f,0.5105f},{3.20f,0.5084f},{3.30f,0.5067f},{3.40f,0.5054f},{3.50f,0.5040f},{3.60f,0.5030f},{3.70f,0.5022f},{3.80f,0.5016f},
		{3.90f,0.5010f},{4.00f,0.5006f},{4.20f,0.4998f},{4.40f,0.4995f},{4.60f,0.4992f},{4.80f,0.4990f},{5.00f,0.4988f}
	};

	// Стандартная пуля G7 (таблица McCoy).
	const FCdPoint G7Table[] = {
		{0.00f,0.1198f},{0.05f,0.1197f},{0.10f,0.1196f},{0.15f,0.1194f},{0.20f,0.1193f},{0.25f,0.1194f},{0.30f,0.1194f},{0.35f,0.1194f},
		{0.40f,0.1193f},{0.45f,0.1193f},{0.50f,0.1194f},{0.55f,0.1193f},{0.60f,0.1194f},{0.65f,0.1197f},{0.70f,0.1202f},{0.725f,0.1207f},
		{0.75f,0.1215f},{0.775f,0.1226f},{0.80f,0.1242f},{0.825f,0.1266f},{0.85f,0.1306f},{0.875f,0.1368f},{0.90f,0.1464f},{0.925f,0.1660f},
		{0.95f,0.2054f},{0.975f,0.2993f},{1.00f,0.3803f},{1.025f,0.4015f},{1.05f,0.4043f},{1.075f,0.4034f},{1.10f,0.4014f},{1.125f,0.3987f},
		{1.15f,0.3955f},{1.20f,0.3884f},{1.25f,0.3810f},{1.30f,0.3732f},{1.35f,0.3657f},{1.40f,0.3580f},{1.50f,0.3440f},{1.55f,0.3376f},
		{1.60f,0.3315f},{1.65f,0.3260f},{1.70f,0.3209f},{1.75f,0.3160f},{1.80f,0.3117f},{1.85f,0.3078f},{1.90f,0.3042f},{1.95f,0.3010f},
		{2.00f,0.2980f},{2.05f,0.2951f},{2.10f,0.2922f},{2.15f,0.2892f},{2.20f,0.2864f},{2.25f,0.2835f},{2.30f,0.2807f},{2.35f,0.2779f},
		{2.40f,0.2752f},{2.45f,0.2725f},{2.50f,0.2697f},{2.55f,0.2670f},{2.60f,0.2643f},{2.65f,0.2615f},{2.70f,0.2588f},{2.75f,0.2561f},
		{2.80f,0.2533f},{2.85f,0.2506f},{2.90f,0.2479f},{2.95f,0.2451f},{3.00f,0.2424f},{3.10f,0.2368f},{3.20f,0.2313f},{3.30f,0.2258f},
		{3.40f,0.2205f},{3.50f,0.2154f},{3.60f,0.2106f},{3.70f,0.2060f},{3.80f,0.2017f},{3.90f,0.1975f},{4.00f,0.1935f},{4.20f,0.1861f},
		{4.40f,0.1793f},{4.60f,0.1730f},{4.80f,0.1672f},{5.00f,0.1618f}
	};

	float Interp(const FCdPoint* T, int32 N, float Mach)
	{
		if (Mach <= T[0].Mach) { return T[0].Cd; }
		if (Mach >= T[N - 1].Mach) { return T[N - 1].Cd; }
		int32 Lo = 0, Hi = N - 1;
		while (Hi - Lo > 1) { const int32 Mid = (Lo + Hi) / 2; if (T[Mid].Mach <= Mach) { Lo = Mid; } else { Hi = Mid; } }
		const float A = (Mach - T[Lo].Mach) / (T[Hi].Mach - T[Lo].Mach);
		return FMath::Lerp(T[Lo].Cd, T[Hi].Cd, A);
	}
}

namespace PBLBallistics
{
	float AirDensity(const FPBLAtmosphere& Atm)
	{
		const float T_K = Atm.Temperature_C + 273.15f;
		const float P = Atm.Pressure_hPa * 100.0f;
		// Давление насыщенного пара (Тетенс), Па; влажный воздух легче сухого.
		const float Psat = 610.78f * FMath::Exp(17.27f * Atm.Temperature_C / (Atm.Temperature_C + 237.3f));
		const float Pv = FMath::Clamp(Atm.Humidity, 0.0f, 1.0f) * Psat;
		return (P - Pv) / (287.058f * T_K) + Pv / (461.495f * T_K);
	}

	float SpeedOfSound(const FPBLAtmosphere& Atm)
	{
		return 331.3f * FMath::Sqrt(1.0f + Atm.Temperature_C / 273.15f);
	}

	float StandardCd(EPBLDragModel Model, float Mach)
	{
		return Model == EPBLDragModel::G7 ? Interp(G7Table, UE_ARRAY_COUNT(G7Table), Mach) : Interp(G1Table, UE_ARRAY_COUNT(G1Table), Mach);
	}

	float MuzzleVelocity(const FPBLCartridgeData& C, float Barrel_m)
	{
		return C.V0_mps + (Barrel_m - C.RefBarrel_m) / 0.025f * C.dV_per_25mm_mps;
	}

	FVector Acceleration(const FPBLCartridgeData& C, const FPBLAtmosphere& Atm, const FVector& Velocity)
	{
		const FVector Vrel = Velocity - Atm.Wind_mps;
		const float V = Vrel.Size();
		if (V < 1e-3f) { return FVector(0, 0, -Gravity); }
		const float Rho = AirDensity(Atm);
		const float Mach = V / SpeedOfSound(Atm);
		const float BC_SI = FMath::Max(C.BC, 0.01f) * BC_LbIn2_to_KgM2;
		const float Decel = (PI / 8.0f) * Rho * StandardCd(C.DragModel, Mach) * V * V / BC_SI;
		return -Decel * (Vrel / V) + FVector(0, 0, -Gravity);
	}

	void Step(const FPBLCartridgeData& C, const FPBLAtmosphere& Atm, FPBLProjectileState& S, float Dt)
	{
		const FVector k1v = Acceleration(C, Atm, S.Velocity);
		const FVector k1p = S.Velocity;
		const FVector k2v = Acceleration(C, Atm, S.Velocity + 0.5f * Dt * k1v);
		const FVector k2p = S.Velocity + 0.5f * Dt * k1v;
		const FVector k3v = Acceleration(C, Atm, S.Velocity + 0.5f * Dt * k2v);
		const FVector k3p = S.Velocity + 0.5f * Dt * k2v;
		const FVector k4v = Acceleration(C, Atm, S.Velocity + Dt * k3v);
		const FVector k4p = S.Velocity + Dt * k3v;
		S.Position += Dt / 6.0f * (k1p + 2.0f * k2p + 2.0f * k3p + k4p);
		S.Velocity += Dt / 6.0f * (k1v + 2.0f * k2v + 2.0f * k3v + k4v);
		S.Time += Dt;
	}
}

namespace PBLBallistics
{
	static float DropAtRange(const FPBLCartridgeData& C, const FPBLAtmosphere& Atm, float V0, float Angle, float SightHeight_m, float Range_m, float Dt)
	{
		FPBLProjectileState S;
		S.Position = FVector(0, 0, -SightHeight_m);           // дуло ниже линии прицеливания
		S.Velocity = FVector(FMath::Cos(Angle), 0, FMath::Sin(Angle)) * V0;
		FVector Prev = S.Position;
		while (S.Position.X < Range_m && S.Time < 20.0f)
		{
			Prev = S.Position;
			Step(C, Atm, S, Dt);
		}
		const float A = (S.Position.X > Prev.X) ? (Range_m - Prev.X) / (S.Position.X - Prev.X) : 0.0f;
		return FMath::Lerp(Prev.Z, S.Position.Z, A);
	}

	float SolveZeroAngle(const FPBLCartridgeData& C, const FPBLAtmosphere& Atm, float V0, float SightHeight_m, float ZeroRange_m)
	{
		// Падение почти линейно по углу - метода секущих хватает.
		float A0 = 0.0f, A1 = FMath::DegreesToRadians(0.5f);
		float D0 = DropAtRange(C, Atm, V0, A0, SightHeight_m, ZeroRange_m, 0.0005f);
		float D1 = DropAtRange(C, Atm, V0, A1, SightHeight_m, ZeroRange_m, 0.0005f);
		for (int32 i = 0; i < 12 && FMath::Abs(D1) > 1e-5f && FMath::Abs(D1 - D0) > 1e-9f; ++i)
		{
			const float A2 = A1 - D1 * (A1 - A0) / (D1 - D0);
			A0 = A1; D0 = D1;
			A1 = A2; D1 = DropAtRange(C, Atm, V0, A1, SightHeight_m, ZeroRange_m, 0.0005f);
		}
		return A1;
	}

	TArray<FPBLTrajectoryRow> BuildTable(const FPBLCartridgeData& C, const FPBLFirearmData& F, const FPBLAtmosphere& Atm, float MaxRange_m, float Step_m, float Dt)
	{
		TArray<FPBLTrajectoryRow> Rows;
		const float V0 = MuzzleVelocity(C, F.Barrel_m);
		const float Angle = SolveZeroAngle(C, Atm, V0, F.SightHeight_m, F.ZeroRange_m);
		FPBLProjectileState S;
		S.Position = FVector(0, 0, -F.SightHeight_m);
		S.Velocity = FVector(FMath::Cos(Angle), 0, FMath::Sin(Angle)) * V0;
		float NextRange = 0.0f;
		FPBLProjectileState Prev = S;
		while (S.Time < 20.0f)
		{
			while (NextRange <= S.Position.X && NextRange <= MaxRange_m)
			{
				const float A = (S.Position.X > Prev.Position.X) ? (NextRange - Prev.Position.X) / (S.Position.X - Prev.Position.X) : 0.0f;
				FPBLTrajectoryRow R;
				R.Range_m = NextRange;
				R.Drop_m = FMath::Lerp(Prev.Position.Z, S.Position.Z, A);
				R.Velocity_mps = FMath::Lerp(Prev.Velocity.Size(), S.Velocity.Size(), A);
				R.Energy_J = 0.5f * C.BulletMass_kg * R.Velocity_mps * R.Velocity_mps;
				R.Time_s = FMath::Lerp(Prev.Time, S.Time, A);
				Rows.Add(R);
				NextRange += Step_m;
			}
			if (NextRange > MaxRange_m) { break; }
			Prev = S;
			Step(C, Atm, S, Dt);
		}
		return Rows;
	}
}

// pbl.Ballistics.Atmosphere: 0 = ICAO (15C, 1013.25 hPa, сухой; JBM по умолчанию), 1 = Army Standard Metro
// (59F, 29.53 inHg = 1000.0 hPa, 78% влажности; таблицы многих производителей).
static TAutoConsoleVariable<int32> CVarAtmosphere(TEXT("pbl.Ballistics.Atmosphere"), 0, TEXT("0 ICAO, 1 Army Standard Metro"));

FPBLAtmosphere PBLBallistics_CurrentAtmosphere()
{
	FPBLAtmosphere A;
	if (CVarAtmosphere.GetValueOnGameThread() == 1)
	{
		A.Temperature_C = 15.0f; A.Pressure_hPa = 1000.0f; A.Humidity = 0.78f;
	}
	return A;
}

// pbl.Ballistics.Table <оружие|патрон> [макс_м=300] [шаг_м=25] [ноль_м] - таблица в лог для сверки с калькуляторами.
static FAutoConsoleCommandWithWorldAndArgs CmdBallisticsTable(
	TEXT("pbl.Ballistics.Table"),
	TEXT("Print trajectory table: pbl.Ballistics.Table <firearm|cartridge> [maxRange_m] [step_m] [zero_m]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !World || !World->GetGameInstance()) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Ballistics.Table <firearm|cartridge> [max] [step] [zero]")); return; }
		UPBLWeaponDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>();
		if (!Data) { return; }
		const float MaxR = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 300.0f;
		const float StepR = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 25.0f;

		FPBLFirearmData F;
		const FPBLCartridgeData* C = nullptr;
		if (const FPBLFirearmData* Found = Data->FindFirearm(FName(*Args[0])))
		{
			F = *Found;
			C = Data->FindCartridge(F.Cartridge);
		}
		else if (const FPBLCartridgeData* FoundC = Data->FindCartridge(FName(*Args[0])))
		{
			C = FoundC;
			F.Name = TEXT("(reference barrel)");
			F.Barrel_m = C->RefBarrel_m;
			F.SightHeight_m = 0.0f;
			F.ZeroRange_m = 100.0f;
		}
		if (!C) { UE_LOG(LogTemp, Warning, TEXT("pbl.Ballistics.Table: '%s' not found"), *Args[0]); return; }
		if (Args.Num() > 3) { F.ZeroRange_m = FCString::Atof(*Args[3]); }

		const FPBLAtmosphere Atm = PBLBallistics_CurrentAtmosphere();
		const float V0 = PBLBallistics::MuzzleVelocity(*C, F.Barrel_m);
		UE_LOG(LogTemp, Display, TEXT("BALLISTICS %s / %s: barrel %.0f mm, V0 %.1f m/s (%.0f fps), %s BC %.3f, mass %.2f g, zero %.0f m, sight %.0f mm, atm %s rho %.4f"),
			*F.Name.ToString(), *C->Name.ToString(), F.Barrel_m * 1000.0f, V0, V0 / 0.3048f,
			C->DragModel == EPBLDragModel::G7 ? TEXT("G7") : TEXT("G1"), C->BC, C->BulletMass_kg * 1000.0f, F.ZeroRange_m, F.SightHeight_m * 1000.0f,
			CVarAtmosphere.GetValueOnGameThread() == 1 ? TEXT("ASM") : TEXT("ICAO"), PBLBallistics::AirDensity(Atm));
		UE_LOG(LogTemp, Display, TEXT("BALLISTICS  range_m   drop_cm   drop_in   vel_mps   vel_fps   energy_J   time_s"));
		for (const FPBLTrajectoryRow& R : PBLBallistics::BuildTable(*C, F, Atm, MaxR, StepR))
		{
			UE_LOG(LogTemp, Display, TEXT("BALLISTICS  %7.0f  %8.1f  %8.2f  %8.1f  %8.0f  %9.0f  %7.3f"),
				R.Range_m, R.Drop_m * 100.0f, R.Drop_m / 0.0254f, R.Velocity_mps, R.Velocity_mps / 0.3048f, R.Energy_J, R.Time_s);
		}
	}));
