#include "Character/PBLCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

APBLCharacter::APBLCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Поворот тела за камерой отключён: в шутере направление взгляда задаёт
	// контроллер, а йав тела подтягивается отдельно (Э2).
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void APBLCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FirstPersonCamera)
	{
		const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraHeight - HalfHeight));
		FirstPersonCamera->SetFieldOfView(CenterFieldOfView);
	}
}
