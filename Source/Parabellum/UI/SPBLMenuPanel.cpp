#include "UI/SPBLMenuPanel.h"

#include "Character/PBLCharacter.h"
#include "Character/PBLMovementSettings.h"
#include "Player/PBLPlayerController.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "PBLMenu"

namespace
{
	FSlateFontInfo Font(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
	FSlateFontInfo BoldFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Bold", Size); }
	UPBLMovementSettings* Settings() { return GetMutableDefault<UPBLMovementSettings>(); }
}

void SPBLMenuPanel::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	UPBLMovementSettings* S = Settings();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.97f))
		.Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(560.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(STextBlock).Font(BoldFont(16)).Text(LOCTEXT("Title", "Настройки управления")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 12)
				[
					SNew(STextBlock).Font(Font(9)).ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
					.AutoWrapText(true)
					.Text(LOCTEXT("Sub", "Скорости пока отладочные: они нужны, чтобы подобрать ощущение движения. "
						"Применяются сразу и сохраняются между запусками."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Walk", "Скорость ходьбы"), S->MaxWalkSpeed, 80.0f, 1200.0f, 10.0f,
					LOCTEXT("CmS", "см/с"), LOCTEXT("WalkHint", "W A S D")) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Sprint", "Скорость бега"), S->MaxSprintSpeed, 80.0f, 1600.0f, 10.0f,
					LOCTEXT("CmS2", "см/с"), LOCTEXT("SprintHint", "удерживать Shift; в прицеле бег не работает")) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Crouch", "Скорость приседа"), S->MaxCrouchSpeed, 40.0f, 600.0f, 5.0f,
					LOCTEXT("CmS3", "см/с"), LOCTEXT("CrouchHint", "Ctrl")) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Sens", "Чувствительность мыши"), S->MouseSensitivity, 0.1f, 15.0f, 0.05f,
					LOCTEXT("None", ""), LOCTEXT("SensHint", "градусов на единицу перемещения мыши")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 14)
				[
					SNew(STextBlock).Font(Font(9)).ColorAndOpacity(FLinearColor(0.55f, 0.6f, 0.65f))
					.AutoWrapText(true).Text(this, &SPBLMenuPanel::GetSpeedHint)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[ SNew(SButton).Text(LOCTEXT("Resume", "Продолжить (Esc)")).OnClicked(this, &SPBLMenuPanel::OnClose) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[ SNew(SButton).Text(LOCTEXT("Reset", "Сбросить")).OnClicked(this, &SPBLMenuPanel::OnReset) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[ SNew(SSpacer) ]
					+ SHorizontalBox::Slot().AutoWidth()
					[ SNew(SButton).Text(LOCTEXT("Quit", "Выход из игры")).OnClicked(this, &SPBLMenuPanel::OnQuit) ]
				]
			]
		]
	];
}

TSharedRef<SWidget> SPBLMenuPanel::MakeRow(const FText& Label, float& Value, float Min, float Max, float Step,
	const FText& Units, const FText& Hint)
{
	float* Target = &Value;
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0, 4)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Font(Font(12)).Text(Label) ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Font(Font(9)).ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f)).Text(Hint) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(130.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(Min).MaxValue(Max).Delta(Step)
				.Value_Lambda([Target]() { return *Target; })
				.OnValueChanged(this, &SPBLMenuPanel::OnChanged, Target)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
		[ SNew(SBox).WidthOverride(40.0f)[ SNew(STextBlock).Font(Font(10)).Text(Units) ] ];
}

void SPBLMenuPanel::OnChanged(float NewValue, float* Target)
{
	if (!Target) { return; }
	*Target = NewValue;
	Settings()->SaveConfig();
	// Скорости попадают в движение только через персонажа; чувствительность читается сама.
	if (APBLPlayerController* PC = Controller.Get())
	{
		if (APBLCharacter* C = Cast<APBLCharacter>(PC->GetPawn())) { C->ApplyMoveSpeed(); }
	}
}

FReply SPBLMenuPanel::OnReset()
{
	UPBLMovementSettings* S = Settings();
	const UPBLMovementSettings* Def = GetDefault<UPBLMovementSettings>()->GetClass()->GetDefaultObject<UPBLMovementSettings>();
	// Значения по умолчанию лежат в заголовке; сбрасываем только то, что меняется этим окном.
	S->MaxWalkSpeed = 480.0f;
	S->MaxSprintSpeed = 700.0f;
	S->MaxCrouchSpeed = 160.0f;
	S->MouseSensitivity = 2.0f;
	(void)Def;
	S->SaveConfig();
	if (APBLPlayerController* PC = Controller.Get())
	{
		if (APBLCharacter* C = Cast<APBLCharacter>(PC->GetPawn())) { C->ApplyMoveSpeed(); }
	}
	return FReply::Handled();
}

FReply SPBLMenuPanel::OnQuit()
{
	if (APBLPlayerController* PC = Controller.Get()) { PC->ConsoleCommand(TEXT("quit")); }
	return FReply::Handled();
}

FReply SPBLMenuPanel::OnClose()
{
	if (APBLPlayerController* PC = Controller.Get()) { PC->ToggleMenuPanel(); }
	return FReply::Handled();
}

FText SPBLMenuPanel::GetSpeedHint() const
{
	const UPBLMovementSettings* S = GetDefault<UPBLMovementSettings>();
	// В метрах в секунду понятнее: обычный шаг ~1.4 м/с, бег трусцой ~3, спринт ~7.
	return FText::FromString(FString::Printf(
		TEXT("в метрах в секунду: ходьба %.1f, бег %.1f, присед %.1f  (для сравнения: шаг 1.4, бег трусцой 3, спринт 7)"),
		S->MaxWalkSpeed / 100.0f, S->MaxSprintSpeed / 100.0f, S->MaxCrouchSpeed / 100.0f));
}

#undef LOCTEXT_NAMESPACE
