#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PBLPlayerController.generated.h"

class UInputMappingContext;

/**
 * Контроллер игрока.
 *
 * Ответственность: какие клавиши что значат (Mapping Context) и всё сугубо
 * локальное - настройки мыши, HUD, меню. Существует на сервере и на владеющем
 * клиенте, но НЕ на чужих клиентах.
 *
 * Сами действия (Move, Jump...) к движению привязывает APBLCharacter - это
 * конвенция UE. Сюда НЕ кладём: состояние персонажа (оно в APBLCharacter,
 * иначе не реплицируется другим игрокам) и правила матча (они в APBLGameMode).
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APBLPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Диагностика: сколько маппингов реально собрала Enhanced Input после первого тика. */
	void LogRebuiltMappings();
	FTimerHandle RebuildCheckHandle;

	/**
	 * Автоскриншот для проверки картинки без человека: ключ -PBLScreenshotAfter=<сек>
	 * снимает Saved/Screenshots/.../pbl_auto.png и через 2 с закрывает игру.
	 * Запускать с -RenderOffScreen. Основной цикл проверки оптики (Э6-Э9).
	 */
	void SetupAutoScreenshot();

public:
	/** Окно испытателя (F2): переопределения параметров оружия на лету. */
	void ToggleTuningPanel();
	/** Оружейная комната (F3): камера у стенда, мышь вращает образец, колесо разбирает. */
	void ToggleBench();
	virtual void PlayerTick(float DeltaTime) override;
	class APBLWeaponBench* GetBench() const { return Bench.Get(); }
	bool IsBenchMode() const { return bBenchMode; }
	void BenchNext();
	void BenchPrev();
	void BenchStage();
	void BenchReset();
	void BenchFire();
	void BenchSlowMo();
	void BenchCutaway();
	/** Следующий/предыдущий образец коллекции на стенде. */
	void BenchSpecimen(int32 Direction);
	void BenchSpecimenNext() { BenchSpecimen(1); }
	void BenchSpecimenPrev() { BenchSpecimen(-1); }
	void ToggleShopPanel();

private:
	void UpdateBenchCamera(float Blend);
public:
	bool IsTuningPanelOpen() const { return TuningPanel.IsValid(); }

private:
	TSharedPtr<class SWidget> TuningPanel;
	TSharedPtr<class SWidget> ShopPanel;
	TWeakObjectPtr<class APBLWeaponBench> Bench;
	TWeakObjectPtr<class ACameraActor> BenchCamera;
	bool bBenchMode = false;
	FVector2D LastMouse = FVector2D::ZeroVector;
	bool bDragging = false;
	float DragDistance = 0.0f;
	void BenchDragStart();
	void BenchDragStop();
	FTimerHandle ScreenshotHandle;

	/**
	 * Контекст ввода Enhanced Input. Это .uasset, поэтому агент его не пишет —
	 * ассет создаётся Python-скриптом (Э2.1), а сюда подставляется путём из .ini.
	 * Мягкая ссылка: если ассета ещё нет, игра стартует без ввода, а не падает.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Приоритет контекста. Пригодится, когда появятся режимы (меню, прицел). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Parabellum|Input")
	int32 DefaultMappingPriority = 0;
};
