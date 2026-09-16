#include "UI/SPBLTuningPanel.h"

#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Character/PBLCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/PBLPlayerController.h"
#include "Range/PBLMaterialBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PBLTuning"

APBLWeapon* SPBLTuningPanel::GetWeapon() const
{
	const APBLCharacter* C = Controller.IsValid() ? Cast<APBLCharacter>(Controller->GetPawn()) : nullptr;
	return C ? C->GetWeapon() : nullptr;
}

void SPBLTuningPanel::PullFromWeapon()
{
	if (APBLWeapon* W = GetWeapon())
	{
		Tuning = W->GetTuning();
		// Пустые переопределения показываем как текущие значения данных - удобнее править от реальных цифр.
		if (Tuning.V0_mps <= 0.0f) { Tuning.V0_mps = W->GetMuzzleVelocity(); }
		if (Tuning.BulletMass_g <= 0.0f) { Tuning.BulletMass_g = W->GetCartridgeData().BulletMass_kg * 1000.0f; }
		if (Tuning.BC <= 0.0f) { Tuning.BC = W->GetCartridgeData().BC; }
		if (Tuning.RPM <= 0.0f) { Tuning.RPM = W->GetFirearmData().RPM > 0 ? W->GetFirearmData().RPM : 400.0f; }
		if (Tuning.Dispersion_MOA < 0.0f) { Tuning.Dispersion_MOA = W->GetFirearmData().Dispersion_MOA; }
		if (Tuning.ZeroRange_m <= 0.0f) { Tuning.ZeroRange_m = W->GetFirearmData().ZeroRange_m; }
		if (Tuning.SightHeight_mm <= 0.0f) { Tuning.SightHeight_mm = W->GetFirearmData().SightHeight_m * 1000.0f; }
		if (Tuning.TrailLifetime_s < 0.0f) { Tuning.TrailLifetime_s = 30.0f; }
		SelectedCartridge = nullptr;
		for (const auto& It : CartridgeItems) { if (*It == W->GetCartridgeData().Name) { SelectedCartridge = It; } }
	}
}

void SPBLTuningPanel::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	if (const UWorld* World = Controller.IsValid() ? Controller->GetWorld() : nullptr)
	{
		if (UPBLWeaponDataSubsystem* Data = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr)
		{
			TArray<FName> Names; Data->AllCartridges().GetKeys(Names); Names.Sort(FNameLexicalLess());
			for (const FName& N : Names) { CartridgeItems.Add(MakeShared<FName>(N)); }
		}
	}
	PullFromWeapon();

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[ SNew(STextBlock).Text(LOCTEXT("Title", "Испытания оружия (F2 - закрыть)")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14)) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Cart", "Патрон"),
		SAssignNew(CartridgeCombo, SComboBox<TSharedPtr<FName>>).OptionsSource(&CartridgeItems).InitiallySelectedItem(SelectedCartridge)
			.OnGenerateWidget(this, &SPBLTuningPanel::MakeCartridgeItem)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FName> Item, ESelectInfo::Type) { SelectedCartridge = Item; if (Item.IsValid()) { Tuning.Cartridge = *Item; Tuning.V0_mps = 0.0f; Tuning.BulletMass_g = 0.0f; Tuning.BC = 0.0f; if (CartridgeLabel.IsValid()) { CartridgeLabel->SetText(FText::FromName(*Item)); } } })
			[ SAssignNew(CartridgeLabel, STextBlock).Text(SelectedCartridge.IsValid() ? FText::FromName(*SelectedCartridge) : LOCTEXT("None", "-")) ],
		FText::GetEmpty()) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("V0", "Начальная скорость"), MakeSpin(Tuning.V0_mps, 50.0f, 2000.0f, 1.0f), LOCTEXT("mps", "м/с")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Mass", "Масса пули"), MakeSpin(Tuning.BulletMass_g, 0.5f, 60.0f, 0.1f), LOCTEXT("g", "г")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("BC", "Баллистический коэффициент"), MakeSpin(Tuning.BC, 0.02f, 1.5f, 0.005f), FText::GetEmpty()) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("RPM", "Темп"), MakeSpin(Tuning.RPM, 30.0f, 1500.0f, 10.0f), LOCTEXT("rpm", "выстр/мин")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("MOA", "Рассеивание"), MakeSpin(Tuning.Dispersion_MOA, 0.0f, 20.0f, 0.1f), LOCTEXT("moa", "MOA")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Zero", "Ноль"), MakeSpin(Tuning.ZeroRange_m, 5.0f, 1000.0f, 5.0f), LOCTEXT("m", "м")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Sight", "Высота прицела"), MakeSpin(Tuning.SightHeight_mm, 0.0f, 120.0f, 1.0f), LOCTEXT("mm", "мм")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Recoil", "Отдача"), MakeSpin(Tuning.RecoilScale, 0.0f, 5.0f, 0.05f), LOCTEXT("x", "x")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Trail", "След траектории"), MakeSpin(Tuning.TrailLifetime_s, 0.0f, 300.0f, 1.0f), LOCTEXT("s", "с")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Temp", "Температура"), MakeSpin(Tuning.Temperature_C, -40.0f, 50.0f, 1.0f), LOCTEXT("C", "°C")) ];
	Rows->AddSlot().AutoHeight()[ MakeRow(LOCTEXT("Press", "Давление"), MakeSpin(Tuning.Pressure_hPa, 700.0f, 1100.0f, 1.0f), LOCTEXT("hPa", "гПа")) ];
	Rows->AddSlot().AutoHeight().Padding(0, 8, 0, 0)[ SNew(STextBlock).Text(this, &SPBLTuningPanel::GetStatusText).AutoWrapText(true) ];
	Rows->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[ SNew(SButton).Text(LOCTEXT("Apply", "Применить")).OnClicked(this, &SPBLTuningPanel::OnApply) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[ SNew(SButton).Text(LOCTEXT("Reset", "Сброс к данным")).OnClicked(this, &SPBLTuningPanel::OnReset) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[ SNew(SButton).Text(LOCTEXT("Clear", "Очистить полигон")).OnClicked(this, &SPBLTuningPanel::OnClearRange) ]
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).Text(LOCTEXT("Close", "Закрыть")).OnClicked(this, &SPBLTuningPanel::OnClose) ]
	];

	ChildSlot
	[
		SNew(SBox).WidthOverride(520.0f)
		[
			SNew(SBorder).BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f)).Padding(12.0f)[ Rows ]
		]
	];
}

TSharedRef<SWidget> SPBLTuningPanel::MakeCartridgeItem(TSharedPtr<FName> Item)
{
	return SNew(STextBlock).Text(FText::FromName(*Item));
}

TSharedRef<SWidget> SPBLTuningPanel::MakeSpin(float& Value, float Min, float Max, float Delta)
{
	float* Ptr = &Value;
	return SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max).Delta(Delta).MinDesiredWidth(110.0f)
		.Value_Lambda([Ptr]() { return *Ptr; })
		.OnValueChanged_Lambda([Ptr](float V) { *Ptr = V; })
		.OnValueCommitted_Lambda([Ptr](float V, ETextCommit::Type) { *Ptr = V; });
}

TSharedRef<SWidget> SPBLTuningPanel::MakeRow(const FText& Label, TSharedRef<SWidget> Editor, const FText& Units)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0, 2)[ SNew(STextBlock).Text(Label) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(6, 2)[ Editor ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2)[ SNew(SBox).WidthOverride(70.0f)[ SNew(STextBlock).Text(Units) ] ];
}

FText SPBLTuningPanel::GetStatusText() const
{
	const APBLWeapon* W = GetWeapon();
	if (!W) { return LOCTEXT("NoWeapon", "Оружие не найдено"); }
	return FText::FromString(FString::Printf(TEXT("%s / %s: V0 %.1f м/с, ноль %.0f м -> угол %.2f MOA, подброс %.2f°, свободная отдача %.2f Дж"),
		*W->GetFirearmData().Name.ToString(), *W->GetCartridgeData().Name.ToString(), W->GetMuzzleVelocity(), W->GetFirearmData().ZeroRange_m,
		FMath::RadiansToDegrees(W->GetZeroAngleRad()) * 60.0f, FMath::RadiansToDegrees(W->GetRecoilInfo().PeakAngle_rad) * W->GetTuning().RecoilScale, W->GetRecoilInfo().FreeEnergy_J));
}

FReply SPBLTuningPanel::OnApply()
{
	if (APBLWeapon* W = GetWeapon()) { W->ApplyTuning(Tuning); PullFromWeapon(); }
	return FReply::Handled();
}

FReply SPBLTuningPanel::OnReset()
{
	if (APBLWeapon* W = GetWeapon()) { W->ApplyTuning(FPBLWeaponTuning()); PullFromWeapon(); if (CartridgeLabel.IsValid()) { CartridgeLabel->SetText(FText::FromName(W->GetCartridgeData().Name)); } }
	return FReply::Handled();
}

FReply SPBLTuningPanel::OnClearRange()
{
	if (UWorld* World = Controller.IsValid() ? Controller->GetWorld() : nullptr)
	{
		for (TActorIterator<APBLMaterialBlock> It(World); It; ++It) { It->ClearChannels(); }
		for (TActorIterator<AActor> It(World); It; ++It) { if (It->ActorHasTag(TEXT("PBLTrail"))) { It->Destroy(); } }
	}
	return FReply::Handled();
}

FReply SPBLTuningPanel::OnClose()
{
	if (Controller.IsValid()) { Controller->ToggleTuningPanel(); }
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
