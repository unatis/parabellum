#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"

/**
 * Попадание пули: к обычному точечному урону добавлена энергия, отданная в тканях (Дж), и признак головы.
 * Нужно целям, чтобы реакция зависела от физики удара, а не от очков урона (шатание / сбивание с ног).
 */
struct FPBLBulletDamageEvent : public FPointDamageEvent
{
	/** Свой идентификатор типа: 1 - FPointDamageEvent, 2 - FRadialDamageEvent. */
	static const int32 ClassID = 10;

	virtual int32 GetTypeID() const override { return FPBLBulletDamageEvent::ClassID; }
	virtual bool IsOfType(int32 InID) const override { return (FPBLBulletDamageEvent::ClassID == InID) || FPointDamageEvent::IsOfType(InID); }

	/** Энергия, отданная пулей в цели, Дж. */
	float Energy_J = 0.0f;
	/** Скорость на входе в цель, м/с (0 если не по живой цели). */
	float ImpactVelocity_mps = 0.0f;
	/** Пуля осталась в теле (не сквозное). */
	bool bStoppedInTarget = false;
};
