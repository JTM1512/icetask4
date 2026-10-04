#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "FlockManager.generated.h"

class ABoidAgent;
class IInputProcessor;
class UCameraComponent;
class UStaticMeshComponent;

// Going out, coming back, or already home.
UENUM(BlueprintType)
enum class EFlockPhase : uint8
{
	Exploring UMETA(DisplayName="Exploring Target"),
	Returning UMETA(DisplayName="Returning Home"),
	Home UMETA(DisplayName="Back at Start")
};

// Spawns the boids, holds the weights, and switches the goal from target back to start.
UCLASS()
class AFlockManager : public AActor
{
	GENERATED_BODY()

public:

	AFlockManager();
	virtual ~AFlockManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	void TryStartFlock();

	// 1 separation, 2 alignment, 3 cohesion, R restart. Shift lowers 1/2/3.
	void ApplyTuneKey(const FKey& Key, bool bShiftDown);

	const TArray<TObjectPtr<ABoidAgent>>& GetAgents() const { return Agents; }

	// Orange target while exploring, green start after that.
	FVector GetCurrentGoal() const;

	// Pull toward the goal is weaker once they're home.
	float GetSeekScale() const;

	// Place the flock in front of the player on play.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	bool bAnchorToPlayer = false;

	// Task wants 5 to 10.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="5", ClampMax="10"))
	int32 AgentCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="100.0"))
	float ExploreDistance = 1800.f;

	// Only used if the manager is dropped in the level by hand.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	FVector TargetOffset = FVector(1800.f, 400.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="50.0"))
	float SpawnRadius = 220.f;

	// Inside this distance counts as arrived.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="50.0"))
	float ArriveRadius = 420.f;

	// They have to stay in the area for this long before the phase changes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="0.0"))
	float ArriveHoldTime = 1.25f;

	// Key 1. Higher spreads them out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="0.0", ClampMax="5.0"))
	float SeparationWeight = 1.6f;

	// Key 2. Higher makes them head the same way.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="0.0", ClampMax="5.0"))
	float AlignmentWeight = 1.1f;

	// Key 3. Higher pulls them into a clump.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="0.0", ClampMax="5.0"))
	float CohesionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="0.0", ClampMax="5.0"))
	float SeekWeight = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="0.0", ClampMax="2.0"))
	float WanderWeight = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="50.0"))
	float MaxSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="50.0"))
	float MaxForce = 900.f;

	// How far a boid looks for neighbours (alignment and cohesion).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="50.0"))
	float NeighborRadius = 520.f;

	// How close is "too close" for separation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boids", meta=(ClampMin="20.0"))
	float SeparationRadius = 170.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Flock")
	EFlockPhase Phase = EFlockPhase::Exploring;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> StartMarker;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> TargetMarker;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> OverviewCamera;

	UPROPERTY()
	TArray<TObjectPtr<ABoidAgent>> Agents;

	FVector StartArea = FVector::ZeroVector;
	FVector TargetArea = FVector::ZeroVector;
	float ArriveHold = 0.f;
	bool bFlockStarted = false;
	FTimerHandle StartTimer;

	TSharedPtr<IInputProcessor> KeyProcessor;

	void SpawnAgents();
	void FrameOverviewCamera();
	void RegisterKeyProcessor();
	void UnregisterKeyProcessor();
	void UpdatePhase(float DeltaTime);
	void DrawFlockDebug() const;
	void ApplyMarkerColor(UStaticMeshComponent* Marker, const FLinearColor& Color) const;

	int32 CountAgentsInside(const FVector& Area) const;
	FVector GetCentroid() const;

	void AdjustWeight(float& Weight, float Delta, float MaxValue);
};
