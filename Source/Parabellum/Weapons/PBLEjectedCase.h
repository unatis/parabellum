#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PBLEjectedCase.generated.h"

class UStaticMeshComponent;

namespace PBLAmmo
{
	/** Меш боеприпаса по имени патрона: 9x19_124_FMJ + "Case" -> /Game/Weapons/Ammo/Case9x19.
	    nullptr, если модели для этого калибра ещё нет. */
	PARABELLUM_API UStaticMesh* MeshFor(FName Cartridge, const TCHAR* Kind);
}

/**
 * Стреляная гильза: физическое тело, вылетающее из окна выброса.
 *
 * Скорость вылета не выдумана - она берётся из расчёта цикла автоматики: гильзу уносит назад
 * вместе с затвором, а отражатель разворачивает её вбок. Поэтому назад гильза летит ровно с той
 * скоростью, какую имеет затвор в момент встречи с отражателем, а боковая составляющая и закрутка
 * заданы данными по съёмке высокоскоростной камерой (Firearms.csv: EjectRight/Up/Spin).
 */
UCLASS()
class PARABELLUM_API APBLEjectedCase : public AActor
{
	GENERATED_BODY()

public:
	APBLEjectedCase();

	virtual void Tick(float DeltaSeconds) override;

	/** Меш гильзы для патрона: 9x19_124_FMJ -> /Game/Weapons/Ammo/Case9x19. nullptr, если модели нет. */
	static UStaticMesh* CaseMeshFor(FName Cartridge);

	/**
	 * Выбросить гильзу. Velocity в см/с (мир), Mass_kg - масса гильзы, Scale - масштаб показа
	 * (на стенде образец увеличен, гильза должна быть увеличена так же).
	 */
	static APBLEjectedCase* Eject(UWorld* World, const FTransform& At, const FVector& Velocity,
		const FVector& AngularVel_deg, float Mass_kg, UStaticMesh* Mesh, float Scale = 1.0f,
		float Life = 20.0f, float TimeScale = 1.0f);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Ammo")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Замедление показа: скорость масштабируется на k, тяжесть - на k*k, и полёт выходит
	    тем же самым, только растянутым во времени. Нужно, чтобы гильзу было видно в замедлении. */
	float TimeScale = 1.0f;
};
