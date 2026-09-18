#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Ballistics/PBLBallisticsTypes.h"

class APBLPlayerController;

/**
 * Магазин (F4): витрина образцов из Content/Data/Catalogue.csv. Купленное уходит в коллекцию
 * игрока и появляется на стенде оружейки. Slate на C++, без ассетов.
 *
 * Покупка идёт только через UPBLCollectionSubsystem::Buy - панель сама список не меняет.
 * Это нужно, чтобы при переезде коллекции на сервер менялся один метод, а не интерфейс.
 */
class SPBLShopPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPBLShopPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APBLPlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeRow(const FPBLCatalogueEntry& Entry);
	FReply OnBuy(FName Weapon);
	FReply OnShow(FName Weapon);
	FReply OnClose();
	FText GetBalanceText() const;
	FText GetStatusText() const;
	class UPBLCollectionSubsystem* Collection() const;

	TWeakObjectPtr<APBLPlayerController> Controller;
	TSharedPtr<class SVerticalBox> Rows;
	FString Status;
	void Rebuild();
};
