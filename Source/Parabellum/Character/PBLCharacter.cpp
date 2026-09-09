#include "Character/PBLCharacter.h"

#include "Camera/CameraComponent.h"
#include "Character/PBLMovementSettings.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

APBLCharacter::APBLCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Тело поворачивается за взглядом только по йау: в шутере корпус смотрит
	// туда же, куда прицел, а наклон головы тело не трогает.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void APBLCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyMovementSettings();
}

void APBLCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UE_LOG(LogTemp, Display, TEXT("PBL: Character %s possessed by %s at %s"),
		*GetName(), NewController ? *NewController->GetName() : TEXT("null"), *GetActorLocation().ToCompactString());
}

void APBLCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(CenterFieldOfView);
	}
	UpdateCameraHeight();
}

void APBLCharacter::ApplyMovementSettings()
{
	const UPBLMovementSettings* S = GetDefault<UPBLMovementSettings>();

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(S->CapsuleRadius, S->CapsuleHalfHeight);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = S->MaxWalkSpeed;
		Move->MaxWalkSpeedCrouched = S->MaxCrouchSpeed;
		Move->MaxAcceleration = S->MaxAcceleration;
		Move->BrakingDecelerationWalking = S->BrakingDeceleration;
		Move->JumpZVelocity = S->JumpVelocity;
		Move->AirControl = S->AirControl;
		Move->SetCrouchedHalfHeight(S->CrouchedCapsuleHalfHeight);
		Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	}
}

void APBLCharacter::UpdateCameraHeight()
{
	if (!FirstPersonCamera || !GetCapsuleComponent())
	{
		return;
	}
	const UPBLMovementSettings* S = GetDefault<UPBLMovementSettings>();
	const float EyeFromBottom = bIsCrouched ? S->CrouchedEyeHeight : S->EyeHeight;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, EyeFromBottom - HalfHeight));
}

void APBLCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	UpdateCameraHeight();
}

void APBLCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	UpdateCameraHeight();
}

void APBLCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("PBLCharacter: InputComponent is not Enhanced - check DefaultInput.ini"));
		return;
	}

	int32 Bound = 0;
	if (UInputAction* IA = MoveAction.LoadSynchronous())
	{
		Input->BindAction(IA, ETriggerEvent::Triggered, this, &APBLCharacter::Input_Move);
		++Bound;
	}
	if (UInputAction* IA = LookAction.LoadSynchronous())
	{
		Input->BindAction(IA, ETriggerEvent::Triggered, this, &APBLCharacter::Input_Look);
		++Bound;
	}
	if (UInputAction* IA = JumpAction.LoadSynchronous())
	{
		Input->BindAction(IA, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(IA, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		++Bound;
	}
	if (UInputAction* IA = CrouchAction.LoadSynchronous())
	{
		Input->BindAction(IA, ETriggerEvent::Started, this, &APBLCharacter::Input_CrouchStart);
		Input->BindAction(IA, ETriggerEvent::Completed, this, &APBLCharacter::Input_CrouchStop);
		++Bound;
	}

	UE_LOG(LogTemp, Display, TEXT("PBL: input bound %d/4 actions (Move='%s' Look='%s' Jump='%s' Crouch='%s')"),
		Bound, *MoveAction.ToString(), *LookAction.ToString(), *JumpAction.ToString(), *CrouchAction.ToString());
	if (Bound < 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("PBL: часть действий не загрузилась — проверь пути в DefaultGame.ini и Content/Input"));
	}
}

void APBLCharacter::Input_Move(const FInputActionValue& Value)
{
	// X = вправо, Y = вперёд - так собран IMC_Default (см. make_input_assets.py).
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}
	const FRotator YawOnly(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Axis.X);
}

void APBLCharacter::Input_Look(const FInputActionValue& Value)
{
	// Сырые counts мыши. bEnableLegacyInputScales=False в DefaultInput.ini,
	// поэтому AddController*Input принимает градусы один к одному.
	const FVector2D Delta = Value.Get<FVector2D>();
	const UPBLMovementSettings* S = GetDefault<UPBLMovementSettings>();
	const float DegPerCount = S->MouseDegreesPerCount * S->MouseSensitivity;

	AddControllerYawInput(Delta.X * DegPerCount);
	AddControllerPitchInput(Delta.Y * DegPerCount * (S->bInvertMouseY ? -1.0f : 1.0f));
}

void APBLCharacter::Input_CrouchStart()
{
	Crouch();
}

void APBLCharacter::Input_CrouchStop()
{
	UnCrouch();
}
