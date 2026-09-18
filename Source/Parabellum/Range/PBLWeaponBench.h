#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ballistics/PBLBallisticsTypes.h"
#include "Ballistics/PBLCycle.h"
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
	/** Луч из камеры: подсветить деталь под курсором. Возвращает имя детали или NAME_None. */
	FName TraceHover(const FVector& Start, const FVector& End);
	/** Снять деталь под курсором. false + причина, если её пока держит другая деталь. */
	bool TryTakePart(FName Part, FString& OutReason);
	FName GetHovered() const { return Hovered; }
	FString GetHoveredName() const;
	FString GetHoveredDescription() const;
	FString GetMessage() const { return Message; }
	/** Мировой центр детали - для проверок и подсказок. */
	bool GetPartCenter(FName Part, FVector& Out) const;

	// --- Цикл автоматики: подвижные части движутся по расчёту из импульса выстрела, а не по анимации ---
	/** Выстрел на стенде: запустить цикл. */
	void FireCycle();
	/** Переключить замедление показа по кругу. */
	void CycleSlowMotion();
	/** Разрез: спрятать наружные детали, чтобы видеть механизм и боеприпас внутри. */
	void ToggleCutaway();
	/** Поставить на стенд другой образец из коллекции. Пустое имя - убрать всё со стенда. */
	void SetSpecimen(FName Weapon, FName Gen, const FString& Path);
	FName GetWeaponName() const { return WeaponName; }
	/** Остановить цикл на заданной миллисекунде от выстрела и держать позу. */
	void FreezeCycle(float Milliseconds);
	/** Задать замедление показа напрямую. */
	void SetSlowMotion(float Scale) { TimeScale = FMath::Clamp(Scale, 0.001f, 1.0f); }
	bool IsCycling() const { return Cycle.IsRunning(); }
	/** Строка состояния цикла для HUD. */
	FString GetCycleLine() const;
	/** Патроны: сколько в магазине и есть ли в патроннике. */
	FString GetAmmoLine() const;

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
	/** Патроны в магазине и в патроннике - по раскладке из AmmoLayout.csv. */
	void BuildAmmo();
	/** Разложить патроны: они едут с магазином при разборке и досылаются при накате. */
	void ApplyAmmoPose();
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
	FName Hovered;
	FString Message;
	const FPBLWeaponPartStep* FindStep(FName Part) const;
	/** Деталь едет вместе с затвором? */
	bool RidesWithSlide(FName Part) const;
	/** Разложить подвижные части по текущему состоянию цикла. */
	void ApplyCyclePose();
	/** Выбросить гильзу, когда затвор вытащил её из патронника и донце дошло до отражателя. */
	void EjectCase();

	FPBLCycleState Cycle;
	FPBLFirearmData Firearm;
	FPBLCartridgeData Cartridge;
	/** Ход затвора, на котором гильза покидает патронник: её собственная длина плюс зазор. */
	float EjectTravel_m = 0.0f;
	bool bCaseEjected = false;
	UPROPERTY(Transient) TObjectPtr<class UStaticMesh> CaseMesh;

	FPBLAmmoLayout Ammo;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> MagRounds;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ChamberRound;
	TArray<FVector> MagRoundBase;
	FVector ChamberBase = FVector::ZeroVector;
	/** Сколько патронов осталось в магазине и стоит ли патрон в патроннике. */
	int32 RoundsInMag = 0;
	bool bChambered = true;
	bool bCutaway = false;
	float TimeScale = 0.1f;
	bool bCycleFrozen = false;

	// --- Config ---
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") FName WeaponName = TEXT("Glock17");
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") FName Generation = TEXT("Gen5");
	/** Путь к папке с деталями: <PartsPath>/<PartName>. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") FString PartsPath = TEXT("/Game/Weapons/Glock17Parts");
	/** Во сколько раз увеличен образец на стенде (реальные 20 см разглядывать неудобно). */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") float DisplayScale = 3.0f;
	/** Скорость перехода детали на место, см/с. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") float MoveSpeed = 90.0f;
	/** Материал подсветки детали под курсором. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") TSoftObjectPtr<class UMaterialInterface> HighlightMaterial;
	/** Замедления показа цикла, переключаются по кругу. */
	/** Детали, которые прячутся в разрезе: наружные оболочки. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") TArray<FName> CutawayParts = { TEXT("Frame"), TEXT("Slide"), TEXT("Magazine") };
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Bench") TArray<float> SlowMotionSteps = { 0.1f, 0.03f, 0.01f, 1.0f };
};
