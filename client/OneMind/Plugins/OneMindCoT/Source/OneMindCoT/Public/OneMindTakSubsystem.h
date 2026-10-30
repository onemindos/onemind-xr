// The client as a TAK contact itself (C3) with contacts, chat and channels (C8).
//
// Owns om::TakStreamClient over FOneMindUeTlsTransport (8089, mutual TLS with the enrolled
// certificate), om::TakContacts for the contact list and conversations, and om::TakEnrollment
// for /Marti/api/tls/signClient/v2 (8446). Everything received here is shown locally and is
// never republished to the bus; the bus stays the authority for entities (TakStream.h).
// Not compiled on the build machine (no Unreal); the same core is built and tested on the host
// (HostTests: unit_tak.cpp, scripts/e2e_c3_tak.py).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "OneMindCore/TakContacts.h"
#include "OneMindCore/TakEnroll.h"
#include "OneMindCore/TakStream.h"

#include <memory>

#include "OneMindTakSubsystem.generated.h"

class FOneMindTakEnrollPlatform;  // Private/OneMindTakSubsystem.cpp

USTRUCT(BlueprintType)
struct FOneMindTakContact
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Uid;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Callsign;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Team;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Role;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Platform;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") double Lat = 0;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") double Lon = 0;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Via;
};

USTRUCT(BlueprintType)
struct FOneMindChatLine
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Conversation;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Sender;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") bool bOutgoing = false;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|TAK") FDateTime Time;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOneMindTakState, bool, bConnected, const FString&, Detail);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOneMindChat, const FOneMindChatLine&, Line);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOneMindEnrolled, bool, bOk, const FString&, Error);

UCLASS()
class ONEMINDCOT_API UOneMindTakSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Enrolls a certificate for this client with a TAK user. Requires the TAK Server CA in the
	 *  keychain item "server-ca" (owner-provisioned); without it nothing is sent and OnEnrolled
	 *  reports NoCaError. The password is used once and not stored; key and certificate go to
	 *  the keychain; the CA chain in the server's reply is never stored. One enrollment at a
	 *  time: a second call while one runs is refused. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|TAK")
	void Enroll(const FString& TakUser, const FString& TakPassword);

	UFUNCTION(BlueprintPure, Category = "OneMind|TAK")
	bool HasIdentity() const;

	UFUNCTION(BlueprintCallable, Category = "OneMind|TAK")
	bool Connect();

	UFUNCTION(BlueprintCallable, Category = "OneMind|TAK")
	void Disconnect();

	UFUNCTION(BlueprintPure, Category = "OneMind|TAK")
	bool IsConnected() const;

	/** "connected v1 to 5.8-RELEASE-69", "connecting", "not enrolled", last error... */
	UFUNCTION(BlueprintPure, Category = "OneMind|TAK")
	FString GetStatusText() const;

	/** Own position for the SA (the camera focus or the operator's location). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|TAK")
	void SetSelfPosition(double Lat, double Lon, double HaeMeters);

	UFUNCTION(BlueprintPure, Category = "OneMind|TAK")
	TArray<FOneMindTakContact> GetContacts() const;

	/** Every team heard plus the active ones. */
	UFUNCTION(BlueprintPure, Category = "OneMind|TAK")
	TArray<FString> GetChannels() const;

	/** Empty = everything visible. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|TAK")
	void SetActiveChannels(const TArray<FString>& Channels);

	/** Conversation: "All Chat Rooms", a contact uid (direct) or a room id. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|TAK")
	FString SendChat(const FString& Conversation, const FString& Text);

	UFUNCTION(BlueprintPure, Category = "OneMind|TAK")
	TArray<FOneMindChatLine> GetConversation(const FString& Conversation) const;

	/** Sends a CoT over the client's own TAK stream. Under proposal UE-P5 the own stream carries
	 *  only the client's SA and direct/group chat; drawn shapes go through svc.cot.send (the
	 *  projector's gates) unless UOneMindTakSettings::bSendShapesOnOwnStream is on. */
	FString SendCot(const om::CotEvent& Ev);

	static const char* const NoCaError;

	UPROPERTY(BlueprintAssignable, Category = "OneMind|TAK") FOnOneMindTakState OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "OneMind|TAK") FOnOneMindChat OnChat;
	UPROPERTY(BlueprintAssignable, Category = "OneMind|TAK") FOnOneMindEnrolled OnEnrolled;

private:
	std::string ClientUidStd() const;

	std::unique_ptr<om::TakStreamClient> Stream;
	std::unique_ptr<om::TakContacts> Contacts;
	std::shared_ptr<FOneMindTakEnrollPlatform> EnrollPlatform;  // shared_ptr: complete only in the .cpp
	std::unique_ptr<om::TakEnrollment> Enrollment;
	std::unique_ptr<om::TakEnrollment> RetiredEnrollment;
	om::TakSelf Self;
	float ExpireTimer = 0.f;
	bool bEnrolling = false;
};
