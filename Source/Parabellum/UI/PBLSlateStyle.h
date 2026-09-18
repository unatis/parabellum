#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"

/**
 * Общие мелочи для панелей на Slate. Вынесены сюда не ради красоты: у каждой панели были
 * свои Font/BoldFont в безымянном пространстве имён, и при unity-сборке файлы склеиваются
 * в одну единицу трансляции - имена сталкивались.
 */
namespace PBLSlate
{
	inline FSlateFontInfo Font(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
	inline FSlateFontInfo BoldFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Bold", Size); }
	/** Сплошная подложка: без неё цвет рамки красить нечего и окно выходит прозрачным. */
	inline const FSlateBrush* Solid() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
}
