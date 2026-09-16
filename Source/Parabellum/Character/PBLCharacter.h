#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PBLCharacter.generated.h"

class UCameraComponent;
class UPBLVisionComponent;
class APBLWeapon;
class UInputAction;
struct FInputActionValue;

/**
 * Базовый персонаж Parabellum.
 *
 * Ответственность: тело в мире и его состояние - движение, здоровье, оружие в руках.
 * Всё это авторитетно на сервере и реплицируется владельцу и остальным.
 *
 * Ввод: действия Enhanced Input привязываются здесь (SetupPlayerInputComponent) -
 * это конвенция UE, InputComponent пешки активен только у локально управляемой.
 * Но какие клавиши что означают (Mapping Context), чувствительность, HUD -
 * это APBLPlayerController. Правила матча - APBLGameMode.
 *
 * Персонаж не знает, живой игрок им управляет или бот - иначе сеть и AI разъедутся.
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APBLCharacter();

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	APBLWeapon* GetWeapon() const { return Weapon; }

	/** Отдача: подброс контроллера. Вызывает оружие локально у владельца. */
	void ApplyRecoil(float PitchUp, float YawDelta);

	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
	virtual void BeginPlay() override;

	// --- Обработчики ввода ---
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_CrouchStart();
	void Input_CrouchStop();
	void Input_FireStart();
	void Input_FireStop();
	void Input_AimStart(const struct FInputActionValue& Value);
	void Input_AimStop(const struct FInputActionValue& Value);
	void Input_Reload();

	/** Сервер выдаёт оружие при появлении. */
	void SpawnDefaultWeapon();

	UFUNCTION()
	void OnRep_Weapon();

	/** Переносит UPBLMovementSettings в капсулу и CharacterMovementComponent. */
	void ApplyMovementSettings();

	/** Пересчитывает целевую высоту камеры относительно текущего размера капсулы. */
	void UpdateCameraHeight();

	/** Целевая и текущая Z камеры относительно центра капсулы; текущая догоняет целевую в Tick. */
	float CameraTargetRelZ = 0.0f;
	float CameraCurrentRelZ = 0.0f;

	/**
	 * Камера от первого лица. Живёт на пешке, а не на контроллере: она следует
	 * за телом, и на Э6 к ней прицепится VisionComponent с боковыми захватами.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parabellum|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Фовеальное зрение (Э6-Э7). Клиентская часть, работает только у локального игрока. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parabellum|Camera")
	TObjectPtr<UPBLVisionComponent> Vision;

	/**
	 * FOV центрального рендера. По спеке зрения - 100-110 градусов.
	 * До Э6 это просто широкий обзор, после станет центральной зоной композита.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Camera")
	float CenterFieldOfView = 103.0f;
	/** FOV для оружия от первого лица (viewmodel, CS: 68). Рендер первого лица UE 5.5+: оружие не растягивается широким FOV мира. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Camera")
	float ViewmodelFieldOfView = 70.0f;
	/** Масштаб геометрии первого лица к камере (0..1): оружие можно поставить на реальные ~60 см от глаз, а рендер подтянет его ближе - не врезается в стены. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Camera")
	float ViewmodelScale = 0.4f;

	// --- Действия ввода. Это .uasset: создаются Tools/make_input_assets.py,
	//     пути задаются в DefaultGame.ini. Мягкие ссылки: нет ассета - нет
	//     действия, но игра не падает. ---

	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputAction> AimAction;

	/** Класс стартового оружия (Config, чтобы не плодить Blueprint). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Weapon")
	TSoftClassPtr<APBLWeapon> DefaultWeaponClass;

	/** Текущее оружие. Реплицируется, чтобы у чужих клиентов оно тоже существовало (Э5+: показать в руках). */
	UPROPERTY(ReplicatedUsing = OnRep_Weapon, VisibleInstanceOnly, Category = "Parabellum|Weapon")
	TObjectPtr<APBLWeapon> Weapon;
};
