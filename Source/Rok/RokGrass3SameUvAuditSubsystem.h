#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RokGrass3SameUvAuditSubsystem.generated.h"

class UTexture2D;
struct FRokGrass3SameUvReadback;

/** Opt-in source-atlas GPU sampler observation. Does not edit any world or asset. */
UCLASS()
class ROK_API URokGrass3SameUvAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
	void Start();
	void Poll();
	void Finish(const FString& Failure = FString());
	TStrongObjectPtr<UTexture2D> Atlas;
	TSharedPtr<FRokGrass3SameUvReadback, ESPMode::ThreadSafe> Job;
	FString OutputDirectory;
	double StartSeconds = 0;
	bool bStarted = false, bFinished = false;
};
