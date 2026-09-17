#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ballistics/PBLBallisticsTypes.h"
#include "PBLWeaponBench.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/**
 * Оружейный стенд: образец висит в воздухе, вращается мышью, разбирается и собирается по шагам.
 * Детали - отдельные меши из /Game/Weapons/<Weapon>Parts, порядок и направление съёма - из
 * Content/Data/WeaponParts.csv (файл генерируется скриптом Blender вместе с самой моделью,
 * так что модель и порядок разборки не могут разойтись).
 */
UCLASS(Config = Game)
class PARABELLUM_API APBLWeaponBench : public AActor
{
	GENERATED_BODY()

public:
	APBLWeaponBench();

	virtual void Tick(float DeltaSeconds) override;

	/** Следующий шаг разборки / предыдущий (сборка). */
	void StepForward();
	void StepBack();
	/** Неполная разборка <-> полная. */
	void ToggleStage();
	/** Вращение образца мышью, градусы. */
	void AddRotation(float Yaw, float Pitch);
	void ResetView();
	/** Центр и радиус собранного образца в мире - чтобы навести камеру. */
	void GetViewFocus(FVector& OutCenter, float& OutRadius) const;

	int32 GetStep() const { return Step; }
	int32 GetStepCount() const { return Steps.Num(); }
	FName GetStage() const { return Stage; }
	/** Подпись текущего состояния для HUD. */
	FString GetStatusLine() const;
	/** Название детали, которая снимется следующей (пусто, если разобрано полностью). */
	FString GetNextPartName() const;

protected:
	virtual void BeginPlay() override;

	void BuildParts();
	void RefreshTargets();

	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Bench")
	TObjectPtr<USceneComponent> Pivot;

	/** Компонент на каждую деталь: ключ - имя детали. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMeshComponent>> PartComps;

	/** Шаги текущей стадии, по порядку. */
	TArray<FPBLWeaponPartStep> Steps;
	/** Куда деталь должна прийти (смещение от сборки, см). */
	TMap<FName, FVector> TargetOffset;

	int32 Step = 0;
	FName Stage = TEXT("Field");

	// --- Config ---
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") FName WeaponName = TEXT("Glock17");
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") FName Generation = TEXT("Gen5");
	/** Путь к папке с деталями: <PartsPath>/<PartName>. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") FString PartsPath = TEXT("/Game/Weapons/Glock17Parts");
	/** Во сколько раз увеличен образец на стенде (реальные 20 см разглядывать неудобно). */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") float DisplayScale = 3.0f;
	/** Скорость перехода детали на место, см/с. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") float MoveSpeed = 90.0f;
};
