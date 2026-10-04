#include "BoidAgent.h"
#include "FlockManager.h"

#include "Components/StaticMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABoidAgent::ABoidAgent()
{
	// The manager updates these. They don't tick themselves.
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ConeMesh.Object);
	}

	Mesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 1.1f));
}

void ABoidAgent::Initialize(AFlockManager* InManager, int32 InIndex, const FVector& InStartPosition)
{
	Manager = InManager;
	AgentIndex = InIndex;
	StartPosition = InStartPosition;

	// Random heading so they don't all leave in the same direction.
	WanderAngle = FMath::FRandRange(0.f, TWO_PI);
	const FVector RandomDir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	const float InitialSpeed = Manager ? Manager->MaxSpeed * 0.45f : 200.f;
	Velocity = RandomDir * InitialSpeed;

	if (Mesh)
	{
		if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this))
			{
				// Different colour per boid so you can tell them apart.
				const uint8 Hue = static_cast<uint8>((AgentIndex * 40) % 255);
				const FLinearColor Color = FLinearColor::MakeFromHSV8(Hue, 200, 255);
				DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
				DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
				Mesh->SetMaterial(0, DynamicMaterial);
			}
		}
	}
}

void ABoidAgent::ResetToStart()
{
	SetActorLocation(StartPosition);
	WanderAngle = FMath::FRandRange(0.f, TWO_PI);
	const FVector RandomDir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	const float InitialSpeed = Manager ? Manager->MaxSpeed * 0.45f : 200.f;
	Velocity = RandomDir * InitialSpeed;
}

void ABoidAgent::FlockUpdate(float DeltaTime)
{
	if (!Manager)
	{
		return;
	}

	// Add the steering forces together. Bigger weight = that rule matters more.
	const float SeekScale = Manager->GetSeekScale();
	FVector Force =
		ComputeSeparation() * Manager->SeparationWeight +
		ComputeAlignment() * Manager->AlignmentWeight +
		ComputeCohesion() * Manager->CohesionWeight +
		ComputeSeek(Manager->GetCurrentGoal()) * Manager->SeekWeight * SeekScale +
		ComputeWander() * Manager->WanderWeight;

	const float WeightSum = Manager->SeparationWeight + Manager->AlignmentWeight + Manager->CohesionWeight + Manager->SeekWeight + Manager->WanderWeight;
	Force = Force.GetClampedToMaxSize(Manager->MaxForce * FMath::Max(WeightSum, 1.f));

	Velocity += Force * DeltaTime;
	Velocity.Z = 0.f;
	Velocity = Velocity.GetClampedToMaxSize(Manager->MaxSpeed);

	// Keep them at the height they spawned at.
	FVector NewLocation = GetActorLocation() + Velocity * DeltaTime;
	NewLocation.Z = StartPosition.Z;
	SetActorLocation(NewLocation);

	// Cone mesh points up (+Z), so aim +Z along the velocity.
	if (Velocity.SizeSquared() > 25.f)
	{
		SetActorRotation(FRotationMatrix::MakeFromZ(Velocity.GetSafeNormal()).Rotator());
	}
}

FVector ABoidAgent::SteerToward(const FVector& DesiredVelocity) const
{
	FVector Steer = DesiredVelocity - Velocity;
	Steer.Z = 0.f;
	if (!Manager)
	{
		return Steer;
	}
	return Steer.GetClampedToMaxSize(Manager->MaxForce);
}

FVector ABoidAgent::ComputeSeparation() const
{
	// Push away from boids inside SeparationRadius. Closer ones push harder.
	FVector Push = FVector::ZeroVector;
	int32 Count = 0;

	for (const ABoidAgent* Other : Manager->GetAgents())
	{
		if (!Other || Other == this)
		{
			continue;
		}

		FVector Away = GetActorLocation() - Other->GetActorLocation();
		Away.Z = 0.f;
		const float Distance = Away.Size();

		// Sitting on the same spot still needs a push or they stick together.
		if (Distance <= KINDA_SMALL_NUMBER)
		{
			Push += FVector(1.f, 0.f, 0.f);
			++Count;
		}
		else if (Distance < Manager->SeparationRadius)
		{
			Push += Away.GetSafeNormal() / Distance;
			++Count;
		}
	}

	if (Count == 0)
	{
		return FVector::ZeroVector;
	}

	const FVector Desired = (Push / Count).GetSafeNormal() * Manager->MaxSpeed;
	return SteerToward(Desired);
}

FVector ABoidAgent::ComputeAlignment() const
{
	// Average velocity of boids inside NeighborRadius, then head that way.
	FVector VelocitySum = FVector::ZeroVector;
	int32 Count = 0;

	for (const ABoidAgent* Other : Manager->GetAgents())
	{
		if (!Other || Other == this)
		{
			continue;
		}

		FVector Offset = Other->GetActorLocation() - GetActorLocation();
		Offset.Z = 0.f;
		if (Offset.Size() < Manager->NeighborRadius && Offset.Size() > KINDA_SMALL_NUMBER)
		{
			VelocitySum += Other->GetBoidVelocity();
			++Count;
		}
	}

	if (Count == 0)
	{
		return FVector::ZeroVector;
	}

	const FVector Desired = (VelocitySum / Count).GetSafeNormal() * Manager->MaxSpeed;
	return SteerToward(Desired);
}

FVector ABoidAgent::ComputeCohesion() const
{
	// Average position of nearby boids, then steer toward that.
	FVector PositionSum = FVector::ZeroVector;
	int32 Count = 0;

	for (const ABoidAgent* Other : Manager->GetAgents())
	{
		if (!Other || Other == this)
		{
			continue;
		}

		FVector Offset = Other->GetActorLocation() - GetActorLocation();
		Offset.Z = 0.f;
		if (Offset.Size() < Manager->NeighborRadius && Offset.Size() > KINDA_SMALL_NUMBER)
		{
			PositionSum += Other->GetActorLocation();
			++Count;
		}
	}

	if (Count == 0)
	{
		return FVector::ZeroVector;
	}

	FVector ToCenter = (PositionSum / Count) - GetActorLocation();
	ToCenter.Z = 0.f;
	const FVector Desired = ToCenter.GetSafeNormal() * Manager->MaxSpeed;
	return SteerToward(Desired);
}

FVector ABoidAgent::ComputeSeek(const FVector& Goal) const
{
	// Head to the goal. Slow down inside ArriveRadius so they don't fly straight through it.
	FVector Offset = Goal - GetActorLocation();
	Offset.Z = 0.f;
	const float Distance = Offset.Size();
	if (Distance < 1.f)
	{
		return FVector::ZeroVector;
	}

	float Speed = Manager->MaxSpeed;
	if (Distance < Manager->ArriveRadius)
	{
		Speed *= Distance / Manager->ArriveRadius;
	}

	const FVector Desired = Offset.GetSafeNormal() * Speed;
	return SteerToward(Desired);
}

FVector ABoidAgent::ComputeWander()
{
	WanderAngle += FMath::FRandRange(-0.6f, 0.6f);
	const FVector Desired = FVector(FMath::Cos(WanderAngle), FMath::Sin(WanderAngle), 0.f) * Manager->MaxSpeed;
	return SteerToward(Desired);
}
