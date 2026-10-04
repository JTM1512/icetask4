#include "FlockWorldSubsystem.h"
#include "FlockManager.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

bool UFlockWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Don't spawn anything while editing the map.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void UFlockWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	TArray<AActor*> Existing;
	UGameplayStatics::GetAllActorsOfClass(&InWorld, AFlockManager::StaticClass(), Existing);
	if (Existing.Num() > 0)
	{
		return;
	}

	// Position gets moved in front of the player once the pawn exists.
	const FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 200.f));
	AFlockManager* Manager = InWorld.SpawnActorDeferred<AFlockManager>(AFlockManager::StaticClass(), SpawnTransform);
	if (!Manager)
	{
		return;
	}

	Manager->bAnchorToPlayer = true;
	UGameplayStatics::FinishSpawningActor(Manager, SpawnTransform);
}
