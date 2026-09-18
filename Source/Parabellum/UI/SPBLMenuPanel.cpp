#include "UI/SPBLMenuPanel.h"

#include "Character/PBLCharacter.h"
#include "Character/PBLMovementSettings.h"
#include "Player/PBLPlayerController.h"
#include "Player/PBLUserSettings.h"
#include "Engine/GameInstance.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "UI/PBLSlateStyle.h"

#define LOCTEXT_NAMESPACE "PBLMenu"

namespace
{
	UPBLMovementSettings* Settings() { return GetMutableDefault<UPBLMovementSettings>(); }
	/** Движок держит скорость в см/с, человек мыслит километрами в час. */
	constexpr float KmH = 0.036f;
}

void SPBLMenuPanel::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	UPBLMovementSettings* S = Settings();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(PBLSlate::Solid())
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.97f))
		.Padding(FMargin(20.0f))
		[
			SNew(SBox).WidthOverride(560.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(STextBlock).Font(PBLSlate::BoldFont(16)).Text(LOCTEXT("Title", "Настройки управления")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 12)
				[
					SNew(STextBlock).Font(PBLSlate::Font(9)).ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
					.AutoWrapText(true)
					.Text(LOCTEXT("Sub", "Скорости пока отладочные: они нужны, чтобы подобрать ощущение движения. "
						"Применяются сразу и сохраняются между запусками."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Walk", "Скорость ходьбы"), S->MaxWalkSpeed, KmH, 1.0f, 45.0f, 0.1f,
					LOCTEXT("KmH", "км/ч"), LOCTEXT("WalkHint", "W A S D")) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Sprint", "Скорость бега"), S->MaxSprintSpeed, KmH, 1.0f, 60.0f, 0.1f,
					LOCTEXT("KmH2", "км/ч"), LOCTEXT("SprintHint", "удерживать Shift; в прицеле бег не работает")) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Crouch", "Скорость приседа"), S->MaxCrouchSpeed, KmH, 0.5f, 25.0f, 0.1f,
					LOCTEXT("KmH3", "км/ч"), LOCTEXT("CrouchHint", "Ctrl")) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ MakeRow(LOCTEXT("Sens", "Чувствительность мыши"), S->MouseSensitivity, 1.0f, 0.1f, 15.0f, 0.05f,
					LOCTEXT("None", ""), LOCTEXT("SensHint", "градусов на единицу перемещения мыши")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 14)
				[
					SNew(STextBlock).Font(PBLSlate::Font(9)).ColorAndOpacity(FLinearColor(0.55f, 0.6f, 0.65f))
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

TSharedRef<SWidget> SPBLMenuPanel::MakeRow(const FText& Label, float& Value, float Scale, float Min, float Max,
	float Step, const FText& Units, const FText& Hint)
{
	float* Target = &Value;
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0, 4)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Font(PBLSlate::Font(12)).Text(Label) ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Font(PBLSlate::Font(9)).ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f)).Text(Hint) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(130.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(Min).MaxValue(Max).Delta(Step)
				.MinFractionalDigits(1).MaxFractionalDigits(2)
				.Value_Lambda([Target, Scale]() { return *Target * Scale; })
				.OnValueChanged(this, &SPBLMenuPanel::OnChanged, Target, Scale)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
		[ SNew(SBox).WidthOverride(40.0f)[ SNew(STextBlock).Font(PBLSlate::Font(10)).Text(Units) ] ];
}

void SPBLMenuPanel::OnChanged(float NewValue, float* Target, float Scale)
{
	if (!Target || Scale <= 0.0f) { return; }
	*Target = NewValue / Scale;
	// Пишем в пользовательский сейв: проектный DefaultGame.ini в игре не записывается.
	if (APBLPlayerController* PC = Controller.Get())
	{
		if (UGameInstance* GI = PC->GetGameInstance())
		{
			if (UPBLUserSettingsSubsystem* U = GI->GetSubsystem<UPBLUserSettingsSubsystem>()) { U->Capture(); }
		}
		// Скорости попадают в движение только через персонажа; чувствительность читается сама.
		if (APBLCharacter* C = Cast<APBLCharacter>(PC->GetPawn())) { C->ApplyMoveSpeed(); }
	}
}

FReply SPBLMenuPanel::OnReset()
{
	// Сброс - это забыть пользовательские правки, а не вписать сюда копию проектных чисел:
	// иначе они разъедутся с DefaultGame.ini при первой же правке конфига.
	if (APBLPlayerController* PC = Controller.Get())
	{
		if (UGameInstance* GI = PC->GetGameInstance())
		{
			if (UPBLUserSettingsSubsystem* U = GI->GetSubsystem<UPBLUserSettingsSubsystem>()) { U->ResetToProjectDefaults(); }
		}
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
	// Ориентиры, чтобы было с чем сверяться. Шаг человека ростом 180 см - около 75 см,
	// при обычных 110 шагах в минуту это и даёт 5 км/ч.
	return FText::FromString(FString::Printf(
		TEXT("Для сверки: пешком 5 км/ч, быстрым шагом 6.5, бег трусцой 10, бег 15, спринт 25 (предел человека около 37). ")
		TEXT("Сейчас ходьба %.1f км/ч - это %.0f шагов в минуту при шаге 75 см (человек идёт примерно 110)."),
		S->MaxWalkSpeed * KmH, S->MaxWalkSpeed / 75.0f * 60.0f));
}

#undef LOCTEXT_NAMESPACE
