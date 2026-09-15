#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PBLGelBlock.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * Блок испытательной среды на стрельбище (гель 10% по умолчанию). Пуля, попав в него, проходит модель
 * PBLPenetration; сервер рассылает результат как канал (шейка до кувырка/раскрытия, основная часть,
 * пуля в конце) - клиенты только рисуют. Размер и материал - из DefaultGame.ini.
 * Консоль: pbl.Range.ClearGel - убрать каналы.
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLGelBlock : public AActor
{
	GENERATED_BODY()

public:
	APBLGelBlock();

	FName GetMaterialName() const { return MaterialName; }
	UStaticMeshComponent* GetBlock() const { return Block; }

	/** Сервер: добавить канал и разослать. Глубины в см, диаметры в мм. */
	void AddChannel(const FVector& Entry, const FVector& Dir, float Depth_cm, float NeckDepth_cm, float EntryDia_mm, float FinalDia_mm, bool bExit);
	void ClearChannels();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_AddChannel(FVector_NetQuantize Entry, FVector_NetQuantizeNormal Dir, float Depth_cm, float NeckDepth_cm, float EntryDia_mm, float FinalDia_mm, bool bExit);
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Clear();

	UStaticMeshComponent* AddCylinder(const FVector& From, const FVector& To, float Dia_cm);
	UStaticMeshComponent* AddSphere(const FVector& At, float Dia_cm);

	UPROPERTY(VisibleAnywhere, Category = "Parabellum|Range")
	TObjectPtr<UStaticMeshComponent> Block;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ChannelParts;

	int32 ChannelCount = 0;

	// --- Config ---
	/** Имя материала из Content/Data/Materials.csv. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") FName MaterialName = TEXT("Gel10");
	/** Размер блока, см (X - вдоль выстрела). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") FVector Size = FVector(100.0f, 15.0f, 15.0f);
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") TSoftObjectPtr<UMaterialInterface> GelMaterial;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") TSoftObjectPtr<UMaterialInterface> ChannelMaterial;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") int32 MaxChannels = 32;
	/** Во сколько раз канал рисуется шире реального диаметра пули (чтобы был виден). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") float ChannelVisualScale = 1.5f;
};
