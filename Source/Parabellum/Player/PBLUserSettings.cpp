#include "Player/PBLUserSettings.h"

#include "Character/PBLMovementSettings.h"
#include "Kismet/GameplayStatics.h"

void UPBLUserSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	{
		// Снимок проектных значений до того, как их накроют пользовательские.
		const UPBLMovementSettings* S = GetDefault<UPBLMovementSettings>();
		ProjectWalk = S->MaxWalkSpeed;
		ProjectSprint = S->MaxSprintSpeed;
		ProjectCrouch = S->MaxCrouchSpeed;
		ProjectSens = S->MouseSensitivity;
	}
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Save = Cast<UPBLUserSettingsSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
	if (!Save)
	{
		Save = Cast<UPBLUserSettingsSave>(UGameplayStatics::CreateSaveGameObject(UPBLUserSettingsSave::StaticClass()));
		UE_LOG(LogTemp, Display, TEXT("PBL Settings: пользовательских настроек нет, берём проектные"));
		return;
	}
	ApplyToMovementSettings();
}

void UPBLUserSettingsSubsystem::ApplyToMovementSettings()
{
	if (!Save) { return; }
	UPBLMovementSettings* S = GetMutableDefault<UPBLMovementSettings>();
	// Ноль - «не задано»: так старый сейв без новых полей не обнулит скорости.
	if (Save->WalkSpeed > 0.0f) { S->MaxWalkSpeed = Save->WalkSpeed; }
	if (Save->SprintSpeed > 0.0f) { S->MaxSprintSpeed = Save->SprintSpeed; }
	if (Save->CrouchSpeed > 0.0f) { S->MaxCrouchSpeed = Save->CrouchSpeed; }
	if (Save->MouseSensitivity > 0.0f) { S->MouseSensitivity = Save->MouseSensitivity; }
	UE_LOG(LogTemp, Display, TEXT("PBL Settings: ходьба %.0f см/с, бег %.0f, присед %.0f, чувствительность %.2f"),
		S->MaxWalkSpeed, S->MaxSprintSpeed, S->MaxCrouchSpeed, S->MouseSensitivity);
}

void UPBLUserSettingsSubsystem::Capture()
{
	if (!Save) { return; }
	const UPBLMovementSettings* S = GetDefault<UPBLMovementSettings>();
	Save->WalkSpeed = S->MaxWalkSpeed;
	Save->SprintSpeed = S->MaxSprintSpeed;
	Save->CrouchSpeed = S->MaxCrouchSpeed;
	Save->MouseSensitivity = S->MouseSensitivity;
	UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

void UPBLUserSettingsSubsystem::ResetToProjectDefaults()
{
	if (Save)
	{
		Save->WalkSpeed = Save->SprintSpeed = Save->CrouchSpeed = Save->MouseSensitivity = 0.0f;
		UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
	}
	UPBLMovementSettings* S = GetMutableDefault<UPBLMovementSettings>();
	S->MaxWalkSpeed = ProjectWalk;
	S->MaxSprintSpeed = ProjectSprint;
	S->MaxCrouchSpeed = ProjectCrouch;
	S->MouseSensitivity = ProjectSens;
	UE_LOG(LogTemp, Display, TEXT("PBL Settings: сброшено к проектным - ходьба %.0f см/с"), S->MaxWalkSpeed);
}
