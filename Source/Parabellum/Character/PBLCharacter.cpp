#include "Character/PBLCharacter.h"

#include "Camera/CameraComponent.h"
#include "Character/PBLMovementSettings.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Vision/PBLVisionComponent.h"
#include "Weapons/PBLWeapon.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

// Живой подбор чувствительности из консоли (~): pbl.Sens 1.5 ; 0 = брать из настроек.
static TAutoConsoleVariable<float> CVarPBLSens(TEXT("pbl.Sens"), 0.0f, TEXT("Override mouse sensitivity, 0 = use PBLMovementSettings"));

APBLCharacter::APBLCharacter()
{

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->bUsePawnControlRotation = true;

	Vision = CreateDefaultSubobject<UPBLVisionComponent>(TEXT("Vision"));

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
	// FOV ставим до BeginPlay компонентов: UPBLVisionComponent запоминает его как FOV обычной камеры.
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(CenterFieldOfView);
	}
}

void APBLCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APBLCharacter, Weapon);
}

void APBLCharacter::SpawnDefaultWeapon()
{
	if (!HasAuthority() || Weapon) { return; }
	UClass* Cls = DefaultWeaponClass.LoadSynchronous();
	if (!Cls) { UE_LOG(LogTemp, Warning, TEXT("PBL: DefaultWeaponClass не задан")); return; }
	FActorSpawnParameters P;
	P.Owner = this;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Weapon = GetWorld()->SpawnActor<APBLWeapon>(Cls, GetActorTransform(), P);
	if (Weapon) { Weapon->AttachToOwnerCamera(this); }
}

void APBLCharacter::OnRep_Weapon()
{
	if (Weapon) { Weapon->AttachToOwnerCamera(this); }
}

void APBLCharacter::ApplyRecoil(float PitchUp, float YawDelta)
{
	AddControllerPitchInput(PitchUp);
	AddControllerYawInput(YawDelta);
}

void APBLCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	SpawnDefaultWeapon();
	UE_LOG(LogTemp, Display, TEXT("PBL: Character %s possessed by %s at %s"),
		*GetName(), NewController ? *NewController->GetName() : TEXT("null"), *GetActorLocation().ToCompactString());
}

void APBLCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateCameraHeight();
	CameraCurrentRelZ = CameraTargetRelZ;
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraCurrentRelZ));
	}
}

void APBLCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FirstPersonCamera && !FMath::IsNearlyEqual(CameraCurrentRelZ, CameraTargetRelZ, 0.01f))
	{
		const float Speed = GetDefault<UPBLMovementSettings>()->CrouchCameraInterpSpeed;
		CameraCurrentRelZ = FMath::FInterpTo(CameraCurrentRelZ, CameraTargetRelZ, DeltaSeconds, Speed);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraCurrentRelZ));
	}
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
	if (!GetCapsuleComponent())
	{
		return;
	}
	const UPBLMovementSettings* S = GetDefault<UPBLMovementSettings>();
	const float EyeFromBottom = bIsCrouched ? S->CrouchedEyeHeight : S->EyeHeight;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	CameraTargetRelZ = EyeFromBottom - HalfHeight;
}

void APBLCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// Капсула сжалась и её центр упал на ScaledHalfHeightAdjust. Компенсируем,
	// чтобы камера осталась на месте в мире, и дальше плавно едем к цели в Tick.
	CameraCurrentRelZ += ScaledHalfHeightAdjust;
	UpdateCameraHeight();
}

void APBLCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CameraCurrentRelZ -= ScaledHalfHeightAdjust;
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
	if (UInputAction* IA = FireAction.LoadSynchronous())
	{
		Input->BindAction(IA, ETriggerEvent::Started, this, &APBLCharacter::Input_FireStart);
		Input->BindAction(IA, ETriggerEvent::Completed, this, &APBLCharacter::Input_FireStop);
	}
	if (UInputAction* IA = ReloadAction.LoadSynchronous())
	{
		Input->BindAction(IA, ETriggerEvent::Started, this, &APBLCharacter::Input_Reload);
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
	const float CVarSens = CVarPBLSens.GetValueOnGameThread();
	const float Sens = CVarSens > 0.0f ? CVarSens : S->MouseSensitivity;
	const float DegPerCount = S->MouseDegreesPerCount * S->MouseCountsScale * Sens;

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

void APBLCharacter::Input_FireStart()
{
	if (Weapon) { Weapon->StartFire(); }
}

void APBLCharacter::Input_FireStop()
{
	if (Weapon) { Weapon->StopFire(); }
}

void APBLCharacter::Input_Reload()
{
	if (Weapon) { Weapon->StartReload(); }
}
