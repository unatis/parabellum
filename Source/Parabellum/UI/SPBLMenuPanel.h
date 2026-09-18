#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APBLPlayerController;

/**
 * Меню по Escape: настройки управления и выход.
 *
 * Значения лежат в UPBLMovementSettings (Config=Game) и применяются сразу, без кнопки
 * «применить»: скорости уходят в CharacterMovementComponent, чувствительность читается
 * при каждом движении мыши. SaveConfig() пишет их в конфиг, поэтому настройка переживает
 * перезапуск. Slate на C++, без ассетов.
 */
class SPBLMenuPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPBLMenuPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APBLPlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Строка настройки: подпись, крутилка, единицы и пояснение. */
	/**
	 * Строка настройки. Scale переводит хранимое значение в то, что видит человек:
	 * скорости движок держит в см/с, а показываем километры в час. Min/Max/Step - уже
	 * в показанных единицах.
	 */
	TSharedRef<SWidget> MakeRow(const FText& Label, float& Value, float Scale, float Min, float Max,
		float Step, const FText& Units, const FText& Hint);
	void OnChanged(float NewValue, float* Target, float Scale);
	FReply OnReset();
	FReply OnQuit();
	FReply OnClose();
	FText GetSpeedHint() const;

	TWeakObjectPtr<APBLPlayerController> Controller;
};
