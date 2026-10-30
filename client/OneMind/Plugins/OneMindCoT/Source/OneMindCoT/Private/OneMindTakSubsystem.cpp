// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindTakSubsystem.h"

#include "HAL/PlatformMisc.h"
#include "OneMindCore/Codec.h"
#include "OneMindKeychain.h"
#include "OneMindTakSettings.h"
#include "OneMindUeTlsTransport.h"
#include "Tls/OmTlsSession.h"

#include <set>

DEFINE_LOG_CATEGORY_STATIC(LogOneMindTak, Log, All);

// Enrollment platform for Unreal: HTTPS through om::HttpExchange over the TLS transport (our
// own trust store, so a TAK Server's private CA works), OpenSSL key and CSR, keychain storage.
// Fails closed: without a trusted CA no request is sent, because the TAK user's password goes
// out as HTTP Basic auth and must only reach a server whose certificate chain was verified.
class FOneMindTakEnrollPlatform : public om::IEnrollPlatform
{
public:
	struct FPending
	{
		std::unique_ptr<om::HttpExchange> Exchange;
		HttpCallback Callback;
	};
	TArray<TSharedPtr<FPending>> Pending;
	std::string TrustPem;  // the owner-provisioned TAK Server CA (keychain "server-ca"); required
	std::string KeyPem;

	virtual void Https(const std::string& Method, const std::string& Url,
		const std::vector<std::pair<std::string, std::string>>& Headers, const std::string& Body, HttpCallback Cb) override
	{
		if (TrustPem.empty())
		{
			om::HttpResponse R;
			R.error = UOneMindTakSubsystem::NoCaError;
			Cb(R);
			return;
		}
		omtls::TlsConfig Cfg;
		Cfg.ca_pem = TrustPem;
		Cfg.verify_peer = true;
		TSharedPtr<FPending> P = MakeShared<FPending>();
		P->Exchange = std::make_unique<om::HttpExchange>(std::make_shared<FOneMindUeTlsTransport>(Cfg));
		P->Callback = MoveTemp(Cb);
		if (!P->Exchange->Start(Method, Url, Headers, Body, om::WallClockMs()))
		{
			om::HttpResponse R;
			R.error = "bad url";
			P->Callback(R);
			return;
		}
		Pending.Add(P);
	}

	/** Drops in-flight requests without calling back (their callbacks point at the enrollment). */
	void Reset()
	{
		Pending.Reset();
		KeyPem.clear();
		TrustPem.clear();
	}

	virtual bool MakeKeyAndCsr(const std::vector<std::pair<std::string, std::string>>& Subject, int KeyBits,
		std::string& KeyRef, std::string& CsrPem, std::string& Err) override
	{
		if (!omtls::MakeRsaKeyAndCsr(Subject, KeyBits, KeyPem, CsrPem, Err)) return false;
		KeyRef = "client-key";
		return true;
	}

	void Poll()
	{
		// Collect first, then call back: a callback may start the next request or Reset().
		TArray<TPair<TSharedPtr<FPending>, om::HttpResponse>> Done;
		for (int32 i = Pending.Num() - 1; i >= 0; --i)
		{
			om::HttpResponse R;
			if (Pending[i]->Exchange->Poll(om::WallClockMs(), R))
			{
				Done.Emplace(Pending[i], R);
				Pending.RemoveAt(i);
			}
		}
		for (TPair<TSharedPtr<FPending>, om::HttpResponse>& D : Done) D.Key->Callback(D.Value);
	}
};

std::string UOneMindTakSubsystem::ClientUidStd() const
{
	const UOneMindTakSettings* S = GetDefault<UOneMindTakSettings>();
	FString Uid = S->ClientUid;
	if (Uid.IsEmpty()) Uid = TEXT("ONEMIND-") + FPlatformMisc::GetLoginId().Left(12);
	return TCHAR_TO_UTF8(*Uid);
}

void UOneMindTakSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UOneMindTakSettings* S = GetDefault<UOneMindTakSettings>();
	Self.uid = ClientUidStd();
	Self.callsign = TCHAR_TO_UTF8(*S->Callsign);
	Self.group_name = TCHAR_TO_UTF8(*S->Team);
	Self.group_role = TCHAR_TO_UTF8(*S->Role);
#if PLATFORM_MAC
	Self.os = "macOS";
	Self.device = "Mac";
#elif PLATFORM_ANDROID
	Self.os = "Android";
	Self.device = "Quest";
#else
	Self.os = TCHAR_TO_UTF8(*FPlatformMisc::GetOSVersion());
#endif
	Self.endpoint = "*:-1:stcp";
	Contacts = std::make_unique<om::TakContacts>(Self.uid, Self.callsign);
	EnrollPlatform = std::make_shared<FOneMindTakEnrollPlatform>();
	if (S->bAutoConnect && HasIdentity()) Connect();
}

void UOneMindTakSubsystem::Deinitialize()
{
	Disconnect();
	if (EnrollPlatform) EnrollPlatform->Reset();  // before the enrollment its callbacks reference
	Enrollment.reset();
	RetiredEnrollment.reset();
	bEnrolling = false;
	EnrollPlatform.reset();
	Contacts.reset();
	Super::Deinitialize();
}

TStatId UOneMindTakSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOneMindTakSubsystem, STATGROUP_Tickables);
}

void UOneMindTakSubsystem::Tick(float DeltaTime)
{
	if (EnrollPlatform) EnrollPlatform->Poll();
	RetiredEnrollment.reset();  // a finished enrollment replaced from inside its own callback
	if (Stream) Stream->Poll(om::WallClockMs());
	ExpireTimer += DeltaTime;
	if (Contacts && ExpireTimer > 5.f)
	{
		ExpireTimer = 0.f;
		Contacts->Expire(om::WallClockMs());
	}
}

const char* const UOneMindTakSubsystem::NoCaError = "no TAK Server CA in keychain (server-ca)";

void UOneMindTakSubsystem::Enroll(const FString& TakUser, const FString& TakPassword)
{
	if (bEnrolling)
	{
		// The running enrollment's HTTP callbacks capture it and share the platform's key slot.
		UE_LOG(LogOneMindTak, Warning, TEXT("TAK enrollment already in progress; wait for it to finish"));
		OnEnrolled.Broadcast(false, TEXT("enrollment already in progress"));
		return;
	}
	const UOneMindTakSettings* S = GetDefault<UOneMindTakSettings>();
	// Trust anchor: the TAK Server CA the owner put in the keychain (OWNER-STEPS UE-4). Never
	// learned from the server itself, so a man in the middle cannot get the password or become
	// trusted for 8089.
	std::string Trust;
	if (!OneMindKeychain::Load(TEXT("server-ca"), Trust) || Trust.find("-----BEGIN CERTIFICATE-----") == std::string::npos)
	{
		UE_LOG(LogOneMindTak, Error, TEXT("TAK enrollment refused: %s"), UTF8_TO_TCHAR(NoCaError));
		OnEnrolled.Broadcast(false, UTF8_TO_TCHAR(NoCaError));
		return;
	}
	EnrollPlatform->Reset();
	EnrollPlatform->TrustPem = Trust;
	om::EnrollConfig Cfg;
	Cfg.host = TCHAR_TO_UTF8(*S->ServerHost);
	Cfg.port = S->EnrollPort;
	Cfg.user = TCHAR_TO_UTF8(*TakUser);
	Cfg.password = TCHAR_TO_UTF8(*TakPassword);
	Cfg.client_uid = Self.uid;
	bEnrolling = true;
	RetiredEnrollment = std::move(Enrollment);  // may be the caller (OnEnrolled handler): freed next tick
	Enrollment = std::make_unique<om::TakEnrollment>(*EnrollPlatform, Cfg);
	TWeakObjectPtr<UOneMindTakSubsystem> Weak(this);
	Enrollment->Start([Weak](const om::EnrollResult& R)
	{
		if (!Weak.IsValid()) return;
		Weak->bEnrolling = false;
		bool bOk = R.ok;
		if (bOk)
		{
			// The CA chain in the reply is not stored: "server-ca" stays the owner-provisioned
			// anchor that verified this exchange, and Connect() verifies 8089 against it.
			bOk = OneMindKeychain::Store(TEXT("client-key"), Weak->EnrollPlatform->KeyPem) &&
				OneMindKeychain::Store(TEXT("client-cert"), R.cert_pem);
			UE_LOG(LogOneMindTak, Log, TEXT("TAK enrollment: the server returned %d CA certificate(s); not stored (server-ca is owner-provisioned)"),
				static_cast<int32>(R.ca_pems.size()));
		}
		Weak->EnrollPlatform->KeyPem.clear();
		const FString Err = bOk ? FString() : (R.ok ? FString(TEXT("keychain write failed")) : FString(UTF8_TO_TCHAR(R.error.c_str())));
		UE_LOG(LogOneMindTak, Log, TEXT("TAK enrollment: %s %s"), bOk ? TEXT("ok") : TEXT("failed"), *Err);
		Weak->OnEnrolled.Broadcast(bOk, Err);
	});
}

bool UOneMindTakSubsystem::HasIdentity() const
{
	std::string Cert;
	return OneMindKeychain::Load(TEXT("client-cert"), Cert) && !Cert.empty();
}

bool UOneMindTakSubsystem::Connect()
{
	const UOneMindTakSettings* S = GetDefault<UOneMindTakSettings>();
	omtls::TlsConfig Tls;
	if (!OneMindKeychain::Load(TEXT("client-cert"), Tls.cert_pem) || !OneMindKeychain::Load(TEXT("client-key"), Tls.key_pem) ||
		!OneMindKeychain::Load(TEXT("server-ca"), Tls.ca_pem))
	{
		OnStateChanged.Broadcast(false, TEXT("not enrolled"));
		return false;
	}
	Tls.verify_peer = true;
	Tls.verify_host = S->bVerifyHostName;
	Disconnect();
	om::TakStreamOptions O;
	O.url = "tls://" + std::string(TCHAR_TO_UTF8(*S->ServerHost)) + ":" + std::to_string(S->StreamPort);
	O.protocol = TCHAR_TO_UTF8(*S->Protocol);
	O.sa_interval_ms = FMath::Max(5, S->SaIntervalSeconds) * 1000;
	Stream = std::make_unique<om::TakStreamClient>(std::make_shared<FOneMindUeTlsTransport>(Tls), O, Self);
	TWeakObjectPtr<UOneMindTakSubsystem> Weak(this);
	Stream->OnState([Weak](om::TakStreamClient::State St, const std::string& Reason)
	{
		if (Weak.IsValid()) Weak->OnStateChanged.Broadcast(St == om::TakStreamClient::State::Connected, UTF8_TO_TCHAR(Reason.c_str()));
	});
	Stream->OnEvent([Weak](const std::string& Xml, const om::CotInfo& Info)
	{
		if (!Weak.IsValid() || !Weak->Contacts) return;
		const std::string Kind = Weak->Contacts->Ingest(Xml, om::WallClockMs(), "stream");
		if (Kind == "chat")
		{
			FOneMindChatLine L;
			L.Sender = UTF8_TO_TCHAR(Info.chat_sender.c_str());
			L.Text = UTF8_TO_TCHAR(Info.remarks.c_str());
			L.Time = FDateTime::UtcNow();
			Weak->OnChat.Broadcast(L);
		}
	});
	Stream->Start(om::WallClockMs());
	return true;
}

void UOneMindTakSubsystem::Disconnect()
{
	if (Stream)
	{
		Stream->Stop();
		Stream.reset();
	}
}

bool UOneMindTakSubsystem::IsConnected() const { return Stream && Stream->connected(); }

FString UOneMindTakSubsystem::GetStatusText() const
{
	if (!Stream) return HasIdentity() ? TEXT("TAK: not connected") : TEXT("TAK: not enrolled");
	if (!Stream->connected()) return FString::Printf(TEXT("TAK: connecting (%s)"), UTF8_TO_TCHAR(Stream->last_error().c_str()));
	const TCHAR* Mode = Stream->mode() == om::TakStreamClient::Mode::Proto ? TEXT("v1") : TEXT("XML");
	return FString::Printf(TEXT("TAK: connected %s %s, %d contacts"), Mode, UTF8_TO_TCHAR(Stream->server_version().c_str()),
		Contacts ? static_cast<int32>(Contacts->Visible().size()) : 0);
}

void UOneMindTakSubsystem::SetSelfPosition(double Lat, double Lon, double HaeMeters)
{
	Self.point = om::GeoPoint{Lat, Lon, HaeMeters};
	if (Stream) Stream->UpdateSelf(Self, false);
}

TArray<FOneMindTakContact> UOneMindTakSubsystem::GetContacts() const
{
	TArray<FOneMindTakContact> Out;
	if (!Contacts) return Out;
	for (const om::TakContact* C : Contacts->Visible())
	{
		FOneMindTakContact K;
		K.Uid = UTF8_TO_TCHAR(C->uid.c_str());
		K.Callsign = UTF8_TO_TCHAR(C->callsign.c_str());
		K.Team = UTF8_TO_TCHAR(C->group_name.c_str());
		K.Role = UTF8_TO_TCHAR(C->group_role.c_str());
		K.Platform = UTF8_TO_TCHAR(C->takv_platform.c_str());
		K.Lat = C->point.lat;
		K.Lon = C->point.lon;
		K.Via = UTF8_TO_TCHAR(C->via.c_str());
		Out.Add(K);
	}
	return Out;
}

TArray<FString> UOneMindTakSubsystem::GetChannels() const
{
	TArray<FString> Out;
	if (Contacts)
		for (const std::string& C : Contacts->Channels()) Out.Add(UTF8_TO_TCHAR(C.c_str()));
	return Out;
}

void UOneMindTakSubsystem::SetActiveChannels(const TArray<FString>& Channels)
{
	if (!Contacts) return;
	std::set<std::string> Set;
	for (const FString& C : Channels) Set.insert(TCHAR_TO_UTF8(*C));
	Contacts->SetActiveChannels(Set);
	// On a TAK Server the active groups also change what the server sends: PUT
	// /Marti/api/groups/active with Contacts->ActiveGroupsRequestBody (unverified body shape;
	// owner check UE-6) needs the 8443 client-certificate connector and is not sent yet.
}

FString UOneMindTakSubsystem::SendChat(const FString& Conversation, const FString& Text)
{
	if (!Contacts) return FString();
	const om::CotEvent Ev = Contacts->BuildChatTo(TCHAR_TO_UTF8(*Conversation), TCHAR_TO_UTF8(*Text), om::WallClockMs(),
		/*for_server=*/true, om::NewToken());
	return SendCot(Ev);
}

FString UOneMindTakSubsystem::SendCot(const om::CotEvent& Ev)
{
	if (!Stream) return TEXT("not connected");
	return UTF8_TO_TCHAR(Stream->Send(Ev).c_str());
}

TArray<FOneMindChatLine> UOneMindTakSubsystem::GetConversation(const FString& Conversation) const
{
	TArray<FOneMindChatLine> Out;
	const om::ChatConversation* C = Contacts ? Contacts->Conversation(TCHAR_TO_UTF8(*Conversation)) : nullptr;
	if (!C) return Out;
	for (const om::ChatMessage& M : C->messages)
	{
		FOneMindChatLine L;
		L.Conversation = Conversation;
		L.Sender = UTF8_TO_TCHAR(M.sender_callsign.c_str());
		L.Text = UTF8_TO_TCHAR(M.text.c_str());
		L.bOutgoing = M.outgoing;
		L.Time = FDateTime::FromUnixTimestamp(M.time / 1000);
		Out.Add(L);
	}
	return Out;
}
