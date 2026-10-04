#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoidAgent.generated.h"

class AFlockManager;
class UStaticMeshComponent;

// One boid. It doesn't follow a path, it steers off the boids around it.
UCLASS()
class ABoidAgent : public AActor
{
	GENERATED_BODY()

public:

	ABoidAgent();

	// Called by the manager when this boid is spawned.
	void Initialize(AFlockManager* InManager, int32 InIndex, const FVector& StartPosition);

	// One movement step.
	void FlockUpdate(float DeltaTime);

	// Back to this boid's own start position. Used when R is pressed.
	void ResetToStart();

	FVector GetBoidVelocity() const { return Velocity; }

	const FVector& GetStartPosition() const { return StartPosition; }

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<AFlockManager> Manager;

	// Current heading and speed.
	FVector Velocity = FVector::ZeroVector;

	// Where this boid was placed at the start.
	FVector StartPosition = FVector::ZeroVector;

	float WanderAngle = 0.f;
	int32 AgentIndex = 0;

	// Don't get too close to other boids.
	FVector ComputeSeparation() const;

	// Head the same way as nearby boids.
	FVector ComputeAlignment() const;

	// Stay with the group.
	FVector ComputeCohesion() const;

	// Move toward whatever goal the manager is using right now.
	FVector ComputeSeek(const FVector& Goal) const;

	// Small random turn so they don't fly dead straight.
	FVector ComputeWander();

	// Difference between the velocity we want and the one we have.
	FVector SteerToward(const FVector& DesiredVelocity) const;
};
