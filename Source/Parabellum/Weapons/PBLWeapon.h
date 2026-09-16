#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ballistics/PBLBallisticsTypes.h"
#include "Weapons/PBLProjectile.h"
#include "Ballistics/PBLRecoil.h"
#include "PBLWeapon.generated.h"

class USkeletalMeshComponent;
class UAnimSequence;
class USoundBase;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class APBLCharacter;

/** Переопределения для испытаний (окно F2). 0 / пусто = взять из данных. Хранится на оружии, применяется в LoadWeaponData. */
USTRUCT()
struct FPBLWeaponTuning
{
	GENERATED_BODY()
	UPROPERTY() FName Cartridge;
	UPROPERTY() float V0_mps = 0.0f;
	UPROPERTY() float BulletMass_g = 0.0f;
	UPROPERTY() float BC = 0.0f;
	UPROPERTY() float RPM = 0.0f;
	UPROPERTY() float Dispersion_MOA = -1.0f;
	UPROPERTY() float ZeroRange_m = 0.0f;
	UPROPERTY() float SightHeight_mm = 0.0f;
	UPROPERTY() float RecoilScale = 1.0f;
	UPROPERTY() float TrailLifetime_s = -1.0f;
	UPROPERTY() float Temperature_C = 15.0f;
	UPROPERTY() float Pressure_hPa = 1013.25f;
};

/**
 * Оружие (пока - один пистолет, hitscan).
 *
 * Сетевая схема с первого дня: клиент-владелец нажимает "огонь" -> локально сразу играет анимацию/звук
 * (отзывчивость как в CS) и шлёт Server_Fire с исходной точкой и направлением. Сервер валидирует
 * (кулдаун, патроны, длина луча), делает трассировку, наносит урон и рассылает Multicast_HitFX всем.
 * В сингле (listen server) обе роли на одной машине - код тот же.
 *
 * Все числа - Config (DefaultGame.ini, секция [/Script/Parabellum.PBLWeapon]).
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLWeapon : public AActor
{
	GENERATED_BODY()

public:
	APBLWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Привязать к владельцу и его камере (вид от первого лица). */
	void AttachToOwnerCamera(APBLCharacter* NewOwner);

	// --- Ввод владельца (клиент) ---
	void StartFire();
	void StopFire();
	void StartReload();

	int32 GetAmmoInMag() const { return AmmoInMag; }
	int32 GetMagSize() const { return MagSize; }
	bool IsReloading() const { return bReloading; }
	const FPBLShotReport& GetLastReport() const { return LastReport; }
	// --- Испытательные переопределения (окно F2) ---
	const FPBLWeaponTuning& GetTuning() const { return Tuning; }
	void ApplyTuning(const FPBLWeaponTuning& NewTuning);
	const FPBLFirearmData& GetFirearmData() const { return Firearm; }
	const FPBLCartridgeData& GetCartridgeData() const { return Cartridge; }
	float GetZeroAngleRad() const { return ZeroAngle_rad; }
	const FPBLRecoilInfo& GetRecoilInfo() const { return RecoilInfo; }
	float GetMuzzleVelocity() const;
	float GetLastReportTime() const { return LastReportTime; }

	FVector GetMuzzleLocation() const;
	/** Косметический след пули за кадр (вызывает пуля на всех машинах). */
	void DrawTracerSegment(const FVector& From, const FVector& To) { SpawnTracer(From, To); }

	// --- Обратные вызовы пули (сервер) ---
	void OnProjectileImpact(const FHitResult& Hit, const FVector& ImpactVelocity_mps, float Damage);
	void OnProjectileFinished(const FPBLShotReport& Report);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Отдача из физики (E10.5): кик пружины хвата при выстреле. */
	void ApplyPhysicsRecoil();

	/** Локальный выстрел владельца: проверка, эффекты, RPC на сервер. */
	void FireOnce();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);

	/** Отчёт о выстреле стрелку (HUD-хронограф). */
	UFUNCTION(Client, Unreliable)
	void Client_ShotReport(FPBLShotReport Report);

	/** Запуск пули (сервер - авторитетной, клиенты - косметической). Origin в см, Velocity м/с. */
	void LaunchProjectile(const FVector& Origin, const FVector& Velocity, const FVector& LOSOrigin, const FVector& LOSDir, bool bAuthoritative);
	/** Точка и направление вылета с учётом прицельной линии и нуля. */
	void ComputeLaunch(const FVector& LOSOrigin, const FVector& LOSDir, FVector& OutOrigin, FVector& OutDir) const;
	void LoadWeaponData();
	/** Масштаб меша под габаритную длину образца (OverallLength_mm из Firearms.csv). */
	void ApplyRealSize();
	void AppendShotCsv(const FPBLShotReport& R) const;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_HitFX(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal, bool bHitCharacter);

	/** Сервер подтверждает владельцу попадание по мишени - хит-маркер и звук только у стрелявшего. */
	UFUNCTION(Client, Unreliable)
	void Client_HitConfirmed(bool bHead, bool bKill);

	/** Вспышка и косметическая пуля у других клиентов (владелец уже запустил свою). */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShotFX(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);
	void PlayMuzzleFX();
	void SpawnTracer(const FVector& From, const FVector& To);
	/** Цилиндр-отрезок без коллизии с временем жизни; общий для трассера и следа. */
	AActor* SpawnSegment(const FVector& From, const FVector& To, float Thickness_cm, UMaterialInterface* M, float Lifetime, FName Tag);
	void HideMuzzleFlash();

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	UFUNCTION()
	void OnRep_AmmoInMag();

	void FinishReload();
	void PlayLocalFireFX();
	void PlayIdle();
	FTimerHandle IdleTimer;
	void SpawnImpact(const FVector& Location, const FVector& Normal, bool bHitCharacter);

	/** Разброс: направление с учётом текущего рассеивания (Э4.3 - паттерн отдачи придёт сюда). */
	FVector ApplySpread(const FVector& Dir) const;

	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Weapon")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Вспышка: маленький излучающий меш + точечный свет на 1-2 кадра. */
	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Weapon")
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;
	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Weapon")
	TObjectPtr<UPointLightComponent> MuzzleLight;
	FTimerHandle MuzzleTimer;

	UPROPERTY(Transient)
	TObjectPtr<APBLCharacter> OwnerCharacter;

	// --- Состояние (реплицируется владельцу для HUD) ---
	UPROPERTY(ReplicatedUsing = OnRep_AmmoInMag)
	int32 AmmoInMag = 0;

	UPROPERTY(Replicated)
	bool bReloading = false;

	float LastFireTime = -1000.0f;
	float ServerLastFireTime = -1000.0f;
	FTimerHandle ReloadTimer;

	// --- Данные из CSV (E10.1) ---
	FPBLFirearmData Firearm;
	FPBLCartridgeData Cartridge;
	bool bHasData = false;
	float ZeroAngle_rad = 0.0f;
	/** Отдача (E10.5): статические величины выстрела и состояние пружины хвата (только у владельца). */
	FPBLRecoilInfo RecoilInfo;
	FPBLRecoilState Recoil;
	FPBLWeaponTuning Tuning;
	FPBLAtmosphere Atmosphere;
	bool bRecoilTickActive = false;
	FPBLShotReport LastReport;
	float LastReportTime = -1000.0f;

	// --- Параметры (Config) ---
	/** Имя образца в Content/Data/Firearms.csv - оттуда патрон, ствол, магазин, темп, рассеивание, ноль. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FName FirearmName = TEXT("Glock17");
	/** Патрон вместо штатного из Firearms.csv (пусто = штатный). Для испытаний разных типов пуль в одном оружии. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FName CartridgeName;
	/** Класс пули. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftClassPtr<APBLProjectile> ProjectileClass;
	/** Писать каждый выстрел в Saved/Ballistics/shots.csv (сервер). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") bool bLogShotsCsv = true;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USkeletalMesh> WeaponMesh;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UAnimSequence> FireAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UAnimSequence> ReloadAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UAnimSequence> ReloadEmptyAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UAnimSequence> IdleAnim;
	/** Масштабировать меш под OverallLength_mm (для одиночных моделей оружия; для рига «руки+оружие» - выключить). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") bool bScaleMeshToLength = true;
	/** Сокет/кость дула на меше; если нет - MuzzleOffset. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FName MuzzleSocket;
	/** Печатать кости меша при старте (калибровка нового рига). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") bool bLogBones = false;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> FireSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> DryFireSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> ReloadSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UMaterialInterface> ImpactDecal;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UMaterialInterface> TracerMaterial;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UMaterialInterface> MuzzleFlashMaterial;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> ImpactSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> BodyHitSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> HitMarkerSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> KillSound;
	/** Дуло в локальных координатах меша оружия (у SK_Pistol сокетов нет). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FVector MuzzleOffset = FVector(0.0f, 19.0f, 9.0f);
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float TracerLifetime = 0.06f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float TracerThickness = 1.2f;
	/** След траектории: тонкая линия по пути пули, живёт долго (полигон: видно падение). 0 = выключено. pbl.Range.ClearTrails. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float TrajectoryTrailLifetime = 30.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float TrajectoryTrailThickness = 0.3f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UMaterialInterface> TrailMaterial;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float MuzzleFlashTime = 0.04f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float MuzzleFlashSize = 6.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float MuzzleLightIntensity = 4000.0f;

	/** Положение вида от первого лица относительно камеры (см): вперёд, вправо, вверх. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FVector ViewOffset = FVector(28.0f, 12.0f, -14.0f);
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FRotator ViewRotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") int32 MagSize = 17;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float FireInterval = 0.15f;   // Glock ~400 rpm
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float ReloadTime = 2.2f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float Damage = 28.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float Range = 8192.0f;
	/** Базовый разброс, градусы (полуугол). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float SpreadDeg = 0.6f;
	/** Отдача-заглушка без данных CSV (hitscan): подброс камеры за выстрел, градусы. С данными работает PBLRecoil. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float RecoilPitch = 1.4f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float RecoilYawRandom = 0.35f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float DecalSize = 10.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float DecalLifetime = 60.0f;
};
