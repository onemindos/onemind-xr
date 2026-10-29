// Owns the OneMind client core (om::ClientCore) for the game instance and ticks it every
// frame on the game thread. Other OneMind plugins reach the bus through this subsystem.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "OneMindCore/ClientCore.h"
#include "OneMindCore/Entity.h"

#include <memory>

#include "OneMindBusSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOneMindBus, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOneMindBusConnection, bool, bConnected);

UCLASS()
class ONEMINDBUS_API UOneMindBusSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Core.get() != nullptr; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** The engine-independent client core (C++ only). Null before Initialize. */
	om::ClientCore* GetCore() const { return Core.get(); }

	/** Markers from the S5 KV probe (empty unless ProbeBucket is set). */
	const om::EntityStore& GetProbeStore() const { return ProbeStore; }
	om::EntityStore& GetProbeStoreMutable() { return ProbeStore; }

	UFUNCTION(BlueprintPure, Category = "OneMind|Bus")
	bool IsConnected() const;

	/** One-line status for the HUD: state, server, reconnects, last error. */
	UFUNCTION(BlueprintPure, Category = "OneMind|Bus")
	FString GetStatusText() const;

	/** Drops and re-opens the connection (keeps subscriptions). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Bus")
	void Reconnect();

	UPROPERTY(BlueprintAssignable, Category = "OneMind|Bus")
	FOnOneMindBusConnection OnConnectionChanged;

private:
	void StartProbe();

	std::unique_ptr<om::ClientCore> Core;
	om::EntityStore ProbeStore;
	bool bWasConnected = false;
	bool bProbeStarted = false;
	uint64 LastReconnects = 0;
};
