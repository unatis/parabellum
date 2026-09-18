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
	TSharedRef<SWidget> MakeRow(const FText& Label, float& Value, float Min, float Max, float Step,
		const FText& Units, const FText& Hint);
	void OnChanged(float NewValue, float* Target);
	FReply OnReset();
	FReply OnQuit();
	FReply OnClose();
	FText GetSpeedHint() const;

	TWeakObjectPtr<APBLPlayerController> Controller;
};
