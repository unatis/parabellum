#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PBLMaterialBlock.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * Блок испытательного материала на стрельбище: гель, гипсокартон, фанера, доска, сталь... Материал (строка Materials.csv),
 * размер и внешний вид задаются на экземпляре (генератор уровня) или по умолчанию из DefaultGame.ini (гель 10%).
 * Пуля, попав в блок, проходит PBLPenetration::PassLayer по толщине вдоль своего пути; сервер рассылает канал
 * (шейка до кувырка/раскрытия, основная часть, пуля в конце) - клиенты только рисуют. Консоль: pbl.Range.ClearGel.
 */
UCLASS(Config=Game)
class PARABELLUM_API APBLMaterialBlock : public AActor
{
	GENERATED_BODY()

public:
	APBLMaterialBlock();

	FName GetMaterialName() const { return MaterialName; }
	FName GetBodyPart() const { return BodyPart; }
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
	/** Часть тела из Content/Data/BodyLayers.csv (пусто = однородный блок MaterialName). Пуля идёт по стеку слоёв. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Range") FName BodyPart;
	/** Имя материала из Content/Data/Materials.csv. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Range") FName MaterialName = TEXT("Gel10");
	/** Своя форма вместо куба (например, часть куклы из Blender): Size не применяется, коллизия меша должна быть complex-as-simple. */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Range") TSoftObjectPtr<UStaticMesh> BlockMesh;
	/** Размер блока, см (X - вдоль выстрела = толщина слоя). */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Range") FVector Size = FVector(100.0f, 15.0f, 15.0f);
	/** Внешний вид блока (по умолчанию - гель). */
	UPROPERTY(Config, EditAnywhere, Category = "Parabellum|Range") TSoftObjectPtr<UMaterialInterface> GelMaterial;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") TSoftObjectPtr<UMaterialInterface> ChannelMaterial;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") int32 MaxChannels = 32;
	/** Во сколько раз канал рисуется шире реального диаметра пули (чтобы был виден). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Parabellum|Range") float ChannelVisualScale = 1.5f;
};
