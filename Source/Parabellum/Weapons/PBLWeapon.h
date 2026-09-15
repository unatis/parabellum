#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PBLWeapon.generated.h"

class USkeletalMeshComponent;
class UAnimSequence;
class USoundBase;
class UMaterialInterface;
class APBLCharacter;

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

protected:
	virtual void BeginPlay() override;

	/** Локальный выстрел владельца: проверка, эффекты, RPC на сервер. */
	void FireOnce();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_HitFX(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal, bool bHitCharacter);

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	UFUNCTION()
	void OnRep_AmmoInMag();

	void FinishReload();
	void PlayLocalFireFX();
	void SpawnImpact(const FVector& Location, const FVector& Normal, bool bHitCharacter);

	/** Разброс: направление с учётом текущего рассеивания (Э4.3 - паттерн отдачи придёт сюда). */
	FVector ApplySpread(const FVector& Dir) const;

	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Weapon")
	TObjectPtr<USkeletalMeshComponent> Mesh;

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

	// --- Параметры (Config) ---
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USkeletalMesh> WeaponMesh;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UAnimSequence> FireAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UAnimSequence> ReloadAnim;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> FireSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> DryFireSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<USoundBase> ReloadSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") TSoftObjectPtr<UMaterialInterface> ImpactDecal;

	/** Положение вида от первого лица относительно камеры (см): вперёд, вправо, вверх. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FVector ViewOffset = FVector(28.0f, 12.0f, -14.0f);
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") FRotator ViewRotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") int32 MagSize = 17;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float FireInterval = 0.15f;   // Glock ~400 rpm
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float ReloadTime = 2.2f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float Damage = 28.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float HeadshotMultiplier = 4.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float Range = 8192.0f;
	/** Базовый разброс, градусы (полуугол). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float SpreadDeg = 0.6f;
	/** Отдача: подброс камеры вверх за выстрел, градусы; и скорость возврата. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float RecoilPitch = 1.4f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float RecoilYawRandom = 0.35f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float DecalSize = 10.0f;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Weapon") float DecalLifetime = 60.0f;
};
