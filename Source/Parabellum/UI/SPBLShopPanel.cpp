#include "UI/SPBLShopPanel.h"

#include "Ballistics/PBLWeaponDataSubsystem.h"
#include "Collection/PBLCollection.h"
#include "Engine/GameInstance.h"
#include "Player/PBLPlayerController.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "PBLShop"

namespace
{
	FSlateFontInfo Font(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
	FSlateFontInfo BoldFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Bold", Size); }
}

UPBLCollectionSubsystem* SPBLShopPanel::Collection() const
{
	APBLPlayerController* PC = Controller.Get();
	UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPBLCollectionSubsystem>() : nullptr;
}

void SPBLShopPanel::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.97f))
		.Padding(FMargin(18.0f))
		[
			SNew(SBox).WidthOverride(760.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[ SNew(STextBlock).Font(BoldFont(16)).Text(LOCTEXT("Title", "Оружейный магазин")) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ SNew(STextBlock).Font(BoldFont(14)).ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.45f))
						.Text(this, &SPBLShopPanel::GetBalanceText) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(STextBlock).Font(Font(9)).ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
					.Text(LOCTEXT("Sub", "Купленное попадает в вашу коллекцию и встаёт на стенд в оружейке (F3). Валюта внутриигровая."))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[ SAssignNew(Rows, SVerticalBox) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[ SNew(STextBlock).Font(Font(10)).ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.4f))
						.Text(this, &SPBLShopPanel::GetStatusText) ]
					+ SHorizontalBox::Slot().AutoWidth()
					[ SNew(SButton).Text(LOCTEXT("Close", "Закрыть (F4)")).OnClicked(this, &SPBLShopPanel::OnClose) ]
				]
			]
		]
	];

	Rebuild();
}

void SPBLShopPanel::Rebuild()
{
	if (!Rows.IsValid()) { return; }
	Rows->ClearChildren();
	APBLPlayerController* PC = Controller.Get();
	UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
	UPBLWeaponDataSubsystem* Data = GI ? GI->GetSubsystem<UPBLWeaponDataSubsystem>() : nullptr;
	if (!Data) { return; }
	for (const FPBLCatalogueEntry& E : Data->Catalogue())
	{
		Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[ MakeRow(E) ];
	}
}

TSharedRef<SWidget> SPBLShopPanel::MakeRow(const FPBLCatalogueEntry& E)
{
	UPBLCollectionSubsystem* Coll = Collection();
	const bool bOwned = Coll && Coll->IsOwned(E.Weapon);
	const bool bCanBuy = Coll && !bOwned && E.IsAvailable() && Coll->Balance() >= E.Price;
	const FName Weapon = E.Weapon;

	// Правая колонка: купленное можно поставить на стенд, доступное - купить,
	// у остального честно написано, почему его нельзя взять.
	TSharedRef<SWidget> Action = SNullWidget::NullWidget;
	if (bOwned)
	{
		Action = SNew(SButton).Text(LOCTEXT("Show", "На стенд"))
			.OnClicked(this, &SPBLShopPanel::OnShow, Weapon);
	}
	else if (E.IsAvailable())
	{
		Action = SNew(SButton)
			.Text(FText::FromString(FString::Printf(TEXT("Купить  %d"), E.Price)))
			.IsEnabled(bCanBuy)
			.OnClicked(this, &SPBLShopPanel::OnBuy, Weapon);
	}
	else
	{
		Action = SNew(STextBlock).Font(Font(10)).ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
			.Text(LOCTEXT("NoModel", "модели пока нет"));
	}

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(bOwned ? FLinearColor(0.10f, 0.14f, 0.10f, 0.9f) : FLinearColor(0.06f, 0.06f, 0.07f, 0.9f))
		.Padding(FMargin(10.0f, 8.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ SNew(STextBlock).Font(BoldFont(13)).Text(FText::FromString(E.DisplayName)) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)
					[ SNew(STextBlock).Font(Font(10)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Text(FText::FromString(FString::Printf(TEXT("%s · %d · %s"),
							*E.Country, E.Year, *E.Cartridge.ToString()))) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)
					[ SNew(STextBlock).Font(BoldFont(10)).ColorAndOpacity(FLinearColor(0.45f, 0.85f, 0.45f))
						.Visibility(bOwned ? EVisibility::Visible : EVisibility::Collapsed)
						.Text(LOCTEXT("Owned", "В КОЛЛЕКЦИИ")) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 12, 0)
				[
					SNew(STextBlock).Font(Font(10)).ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f))
					.AutoWrapText(true).Text(FText::FromString(E.Description))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Action ]
		];
}

FReply SPBLShopPanel::OnBuy(FName Weapon)
{
	if (UPBLCollectionSubsystem* Coll = Collection())
	{
		FString Reason;
		Coll->Buy(Weapon, Reason);
		Status = Reason;
		Rebuild();
	}
	return FReply::Handled();
}

FReply SPBLShopPanel::OnShow(FName Weapon)
{
	APBLPlayerController* PC = Controller.Get();
	UPBLCollectionSubsystem* Coll = Collection();
	if (PC && Coll && Coll->SetActive(Weapon))
	{
		PC->BenchSpecimen(0);
		Status = FString::Printf(TEXT("%s поставлен на стенд - F3, чтобы разглядеть"), *Weapon.ToString());
	}
	return FReply::Handled();
}

FReply SPBLShopPanel::OnClose()
{
	if (APBLPlayerController* PC = Controller.Get()) { PC->ToggleShopPanel(); }
	return FReply::Handled();
}

FText SPBLShopPanel::GetBalanceText() const
{
	UPBLCollectionSubsystem* Coll = Collection();
	return FText::FromString(FString::Printf(TEXT("счёт: %d"), Coll ? Coll->Balance() : 0));
}

FText SPBLShopPanel::GetStatusText() const
{
	return FText::FromString(Status);
}

#undef LOCTEXT_NAMESPACE
