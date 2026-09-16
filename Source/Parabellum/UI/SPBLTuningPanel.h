#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Weapons/PBLWeapon.h"

class APBLPlayerController;
class STextBlock;
template <typename T> class SSpinBox;
template <typename T> class SComboBox;

/**
 * Окно испытателя (F2): переопределения параметров оружия и патрона на лету - патрон, V0, масса, BC, темп, рассеивание,
 * ноль, высота прицела, масштаб отдачи, время следа, атмосфера. Slate на C++, без ассетов. Значения уходят в
 * APBLWeapon::ApplyTuning; «Сброс» возвращает данные CSV. Только владелец (сингл/listen).
 */
class SPBLTuningPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPBLTuningPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APBLPlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	APBLWeapon* GetWeapon() const;
	void PullFromWeapon();
	FReply OnApply();
	FReply OnReset();
	FReply OnClearRange();
	FReply OnClose();
	FText GetStatusText() const;
	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Editor, const FText& Units);
	TSharedRef<SWidget> MakeSpin(float& Value, float Min, float Max, float Delta);
	TSharedRef<SWidget> MakeCartridgeItem(TSharedPtr<FName> Item);

	TWeakObjectPtr<APBLPlayerController> Controller;
	FPBLWeaponTuning Tuning;
	TArray<TSharedPtr<FName>> CartridgeItems;
	TSharedPtr<FName> SelectedCartridge;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> CartridgeCombo;
	TSharedPtr<STextBlock> CartridgeLabel;
};
