#include "FlockManager.h"
#include "BoidAgent.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AFlockManager::AFlockManager()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	StartMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StartMarker"));
	StartMarker->SetupAttachment(Root);
	TargetMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMarker"));
	TargetMarker->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		StartMarker->SetStaticMesh(SphereMesh.Object);
		TargetMarker->SetStaticMesh(SphereMesh.Object);
	}

	StartMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartMarker->SetCastShadow(false);
	TargetMarker->SetCastShadow(false);
	StartMarker->SetRelativeScale3D(FVector(0.55f));
	TargetMarker->SetRelativeScale3D(FVector(0.55f));

	OverviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
	OverviewCamera->SetupAttachment(Root);
	OverviewCamera->SetFieldOfView(70.f);
}

void AFlockManager::BeginPlay()
{
	Super::BeginPlay();
	TryStartFlock();
}

void AFlockManager::TryStartFlock()
{
	if (bFlockStarted)
	{
		return;
	}

	// Player might not exist on the first BeginPlay. Try again shortly.
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (bAnchorToPlayer && !PlayerPawn)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(StartTimer, this, &AFlockManager::TryStartFlock, 0.05f, false);
		}
		return;
	}

	bFlockStarted = true;
	AgentCount = FMath::Clamp(AgentCount, 5, 10);

	if (bAnchorToPlayer && PlayerPawn)
	{
		// Start in front of the player, target further along the same direction.
		FVector Forward = PlayerPawn->GetActorForwardVector();
		Forward.Z = 0.f;
		if (!Forward.Normalize())
		{
			Forward = FVector::ForwardVector;
		}
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		const FVector PlayerLocation = PlayerPawn->GetActorLocation();
		SetActorLocation(PlayerLocation + Forward * 750.f + Right * 180.f + FVector(0.f, 0.f, 160.f));
		StartArea = GetActorLocation();
		TargetArea = StartArea + Forward * ExploreDistance;
	}
	else
	{
		StartArea = GetActorLocation();
		TargetArea = StartArea + TargetOffset;
	}

	StartMarker->SetWorldLocation(StartArea);
	TargetMarker->SetWorldLocation(TargetArea);
	ApplyMarkerColor(StartMarker, FLinearColor(0.15f, 0.85f, 0.25f));
	ApplyMarkerColor(TargetMarker, FLinearColor(1.f, 0.55f, 0.1f));

	SpawnAgents();
	FrameOverviewCamera();
	SetupTuningInput();
}

void AFlockManager::FrameOverviewCamera()
{
	// Camera sits off to the side and up, looking at the middle of the path.
	const FVector Midpoint = (StartArea + TargetArea) * 0.5f;
	FVector Path = TargetArea - StartArea;
	Path.Z = 0.f;
	if (!Path.Normalize())
	{
		Path = FVector::ForwardVector;
	}

	const FVector Side = FVector::CrossProduct(FVector::UpVector, Path);
	const float PathLength = FVector::Dist2D(StartArea, TargetArea);
	const FVector CameraLocation = Midpoint + Side * (PathLength * 0.75f) + FVector(0.f, 0.f, PathLength * 0.55f);

	OverviewCamera->SetWorldLocation(CameraLocation);
	OverviewCamera->SetWorldRotation((Midpoint - CameraLocation).Rotation());

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetViewTargetWithBlend(this, 0.75f);
	}
}

void AFlockManager::ApplyMarkerColor(UStaticMeshComponent* Marker, const FLinearColor& Color) const
{
	if (!Marker)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!BaseMaterial)
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, Marker);
	if (!DynamicMaterial)
	{
		return;
	}

	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Marker->SetMaterial(0, DynamicMaterial);
}

void AFlockManager::SpawnAgents()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Ring around the start so each boid has its own position.
	for (int32 Index = 0; Index < AgentCount; ++Index)
	{
		const float Angle = (TWO_PI * Index) / AgentCount;
		const float Ring = SpawnRadius * FMath::FRandRange(0.35f, 1.f);
		const FVector StartPosition = StartArea + FVector(FMath::Cos(Angle) * Ring, FMath::Sin(Angle) * Ring, 0.f);

		const FTransform SpawnTransform(FRotator::ZeroRotator, StartPosition);
		ABoidAgent* Agent = World->SpawnActorDeferred<ABoidAgent>(ABoidAgent::StaticClass(), SpawnTransform, this);
		if (!Agent)
		{
			continue;
		}

		Agent->Initialize(this, Index, StartPosition);
		UGameplayStatics::FinishSpawningActor(Agent, SpawnTransform);
		Agents.Add(Agent);
	}
}

void AFlockManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bFlockStarted)
	{
		return;
	}

	if (!bTuningBound)
	{
		SetupTuningInput();
	}
	if (!bTuningBound)
	{
		HandleTuningInput();
	}
	UpdatePhase(DeltaTime);

	for (ABoidAgent* Agent : Agents)
	{
		if (Agent)
		{
			Agent->FlockUpdate(DeltaTime);
		}
	}

	DrawFlockDebug();
}

FVector AFlockManager::GetCurrentGoal() const
{
	return Phase == EFlockPhase::Exploring ? TargetArea : StartArea;
}

float AFlockManager::GetSeekScale() const
{
	// Less goal-seeking once they're home, otherwise they pile up on the centre.
	return Phase == EFlockPhase::Home ? 0.35f : 1.f;
}

void AFlockManager::UpdatePhase(float DeltaTime)
{
	if (Phase == EFlockPhase::Home || Agents.Num() == 0)
	{
		return;
	}

	// Need about 2/3 of the flock inside the area, and they have to stay there a bit.
	const FVector Goal = GetCurrentGoal();
	const int32 Needed = FMath::Max(1, (Agents.Num() * 2) / 3);
	if (CountAgentsInside(Goal) >= Needed)
	{
		ArriveHold += DeltaTime;
		if (ArriveHold >= ArriveHoldTime)
		{
			ArriveHold = 0.f;
			Phase = Phase == EFlockPhase::Exploring ? EFlockPhase::Returning : EFlockPhase::Home;
		}
	}
	else
	{
		ArriveHold = 0.f;
	}
}

int32 AFlockManager::CountAgentsInside(const FVector& Area) const
{
	int32 Count = 0;
	for (const ABoidAgent* Agent : Agents)
	{
		if (Agent && FVector::Dist2D(Agent->GetActorLocation(), Area) <= ArriveRadius)
		{
			++Count;
		}
	}
	return Count;
}

FVector AFlockManager::GetCentroid() const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const ABoidAgent* Agent : Agents)
	{
		if (!Agent)
		{
			continue;
		}
		Sum += Agent->GetActorLocation();
		++Count;
	}
	return Count > 0 ? Sum / Count : StartArea;
}

void AFlockManager::AdjustWeight(float& Weight, float Delta, float MaxValue)
{
	Weight = FMath::Clamp(Weight + Delta, 0.f, MaxValue);
}

bool AFlockManager::WasKeyPressed(APlayerController* PlayerController, const FKey& Key, const FKey& AltKey, bool& bWasDown) const
{
	const bool bDown = PlayerController->IsInputKeyDown(Key) || PlayerController->IsInputKeyDown(AltKey);
	const bool bPressed = bDown && !bWasDown;
	bWasDown = bDown;
	return bPressed;
}

void AFlockManager::SetupTuningInput()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController || bTuningBound)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerController->InputComponent);
	if (!Subsystem || !EnhancedInput)
	{
		return;
	}

	TuningContext = NewObject<UInputMappingContext>(this);

	UInputAction* SeparationAction = NewObject<UInputAction>(this);
	SeparationAction->ValueType = EInputActionValueType::Boolean;
	TuningContext->MapKey(SeparationAction, EKeys::One);
	TuningContext->MapKey(SeparationAction, EKeys::NumPadOne);
	EnhancedInput->BindAction(SeparationAction, ETriggerEvent::Started, this, &AFlockManager::TuneSeparation);

	UInputAction* AlignmentAction = NewObject<UInputAction>(this);
	AlignmentAction->ValueType = EInputActionValueType::Boolean;
	TuningContext->MapKey(AlignmentAction, EKeys::Two);
	TuningContext->MapKey(AlignmentAction, EKeys::NumPadTwo);
	EnhancedInput->BindAction(AlignmentAction, ETriggerEvent::Started, this, &AFlockManager::TuneAlignment);

	UInputAction* CohesionAction = NewObject<UInputAction>(this);
	CohesionAction->ValueType = EInputActionValueType::Boolean;
	TuningContext->MapKey(CohesionAction, EKeys::Three);
	TuningContext->MapKey(CohesionAction, EKeys::NumPadThree);
	EnhancedInput->BindAction(CohesionAction, ETriggerEvent::Started, this, &AFlockManager::TuneCohesion);

	UInputAction* RestartAction = NewObject<UInputAction>(this);
	RestartAction->ValueType = EInputActionValueType::Boolean;
	TuningContext->MapKey(RestartAction, EKeys::R);
	EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &AFlockManager::RestartTrip);

	Subsystem->AddMappingContext(TuningContext, 10);
	bTuningBound = true;
}

void AFlockManager::TuneSeparation()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const bool bDecrease = PlayerController && (PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift));
	AdjustWeight(SeparationWeight, bDecrease ? -0.25f : 0.25f, 5.f);
}

void AFlockManager::TuneAlignment()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const bool bDecrease = PlayerController && (PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift));
	AdjustWeight(AlignmentWeight, bDecrease ? -0.25f : 0.25f, 5.f);
}

void AFlockManager::TuneCohesion()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const bool bDecrease = PlayerController && (PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift));
	AdjustWeight(CohesionWeight, bDecrease ? -0.25f : 0.25f, 5.f);
}

void AFlockManager::RestartTrip()
{
	Phase = EFlockPhase::Exploring;
	ArriveHold = 0.f;
	for (ABoidAgent* Agent : Agents)
	{
		if (Agent)
		{
			Agent->ResetToStart();
		}
	}
}

void AFlockManager::HandleTuningInput()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return;
	}

	const bool bDecrease = PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift);
	const float Step = bDecrease ? -0.25f : 0.25f;

	if (WasKeyPressed(PlayerController, EKeys::One, EKeys::NumPadOne, bOneWasDown))
	{
		AdjustWeight(SeparationWeight, Step, 5.f);
	}
	if (WasKeyPressed(PlayerController, EKeys::Two, EKeys::NumPadTwo, bTwoWasDown))
	{
		AdjustWeight(AlignmentWeight, Step, 5.f);
	}
	if (WasKeyPressed(PlayerController, EKeys::Three, EKeys::NumPadThree, bThreeWasDown))
	{
		AdjustWeight(CohesionWeight, Step, 5.f);
	}
	if (WasKeyPressed(PlayerController, EKeys::R, EKeys::R, bRestartWasDown))
	{
		RestartTrip();
	}
}

void AFlockManager::DrawFlockDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Green = start, orange = target, line = where the group is heading.
	DrawDebugSphere(World, StartArea, ArriveRadius, 24, FColor(40, 220, 80), false, 0.f, 0, 2.f);
	DrawDebugSphere(World, TargetArea, ArriveRadius, 24, FColor(255, 140, 30), false, 0.f, 0, 2.f);
	DrawDebugLine(World, GetCentroid(), GetCurrentGoal(), FColor::Cyan, false, 0.f, 0, 3.f);

	const TCHAR* Status = TEXT("They are flying to the orange target.");
	if (Phase == EFlockPhase::Returning)
	{
		Status = TEXT("They are flying back to the green start.");
	}
	else if (Phase == EFlockPhase::Home)
	{
		Status = TEXT("They are back at the green start.");
	}

	if (!GEngine)
	{
		return;
	}

	const FString Text = FString::Printf(
		TEXT("%s\n\nSeparation  %.2f\nAlignment   %.2f\nCohesion    %.2f\n\n1  = more separation (spread out)\n2  = more alignment (same direction)\n3  = more cohesion (stay together)\nShift + 1, 2, or 3  = less of that\nR  = start the trip again\n\nGreen sphere  = start\nOrange sphere = target"),
		Status, SeparationWeight, AlignmentWeight, CohesionWeight);

	GEngine->AddOnScreenDebugMessage(81, 0.f, FColor::White, Text);
}
