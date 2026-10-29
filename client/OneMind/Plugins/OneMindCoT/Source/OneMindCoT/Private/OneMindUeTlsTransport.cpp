// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindUeTlsTransport.h"

#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace OneMindTlsPrivate
{
enum : int32 { KOpen = 0, KData = 1, KClosed = 2 };

bool SplitUrl(const std::string& Url, FString& OutScheme, FString& OutHost, int32& OutPort)
{
	const FString U = UTF8_TO_TCHAR(Url.c_str());
	FString Rest;
	if (!U.Split(TEXT("://"), &OutScheme, &Rest)) return false;
	FString HostPart, PortPart;
	if (!Rest.Split(TEXT(":"), &HostPart, &PortPart, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) return false;
	OutHost = HostPart;
	OutPort = FCString::Atoi(*PortPart);
	return !OutHost.IsEmpty() && OutPort > 0 && OutPort < 65536;
}

bool SendAll(FSocket* S, const std::string& Data, double TimeoutSec)
{
	int32 Off = 0;
	const double End = FPlatformTime::Seconds() + TimeoutSec;
	while (Off < static_cast<int32>(Data.size()))
	{
		int32 Sent = 0;
		if (S->Send(reinterpret_cast<const uint8*>(Data.data()) + Off, static_cast<int32>(Data.size()) - Off, Sent) && Sent > 0)
		{
			Off += Sent;
			continue;
		}
		if (FPlatformTime::Seconds() > End) return false;
		FPlatformProcess::Sleep(0.005f);
	}
	return true;
}
}  // namespace OneMindTlsPrivate

FOneMindUeTlsTransport::~FOneMindUeTlsTransport()
{
	SetListener(nullptr);
	Close();
}

void FOneMindUeTlsTransport::SetListener(Listener* InListener)
{
	FScopeLock Lock(&Mutex);
	TransportListener = InListener;
}

bool FOneMindUeTlsTransport::Open(const std::string& Url)
{
	FString Scheme;
	if (!OneMindTlsPrivate::SplitUrl(Url, Scheme, Host, Port) || (Scheme != TEXT("tls") && Scheme != TEXT("tcp"))) return false;
	Close();
	bUseTls = Scheme == TEXT("tls");
	bCloseRequested = false;
	{
		FScopeLock Lock(&Mutex);
		OutBuf.clear();
	}
	RunGeneration = ++Generation;
	Thread = FRunnableThread::Create(this, TEXT("OneMindTakTls"), 0, TPri_Normal);
	return Thread != nullptr;
}

void FOneMindUeTlsTransport::Send(const std::string& Bytes)
{
	FScopeLock Lock(&Mutex);
	OutBuf += Bytes;
}

void FOneMindUeTlsTransport::Close()
{
	bCloseRequested = true;
	if (Thread)
	{
		Thread->WaitForCompletion();  // the thread flushes queued bytes, then exits
		delete Thread;
		Thread = nullptr;
	}
	++Generation;
}

void FOneMindUeTlsTransport::Deliver(uint64 Gen, int32 Kind, const std::string& Data)
{
	FScopeLock Lock(&Mutex);
	if (!TransportListener || Gen != Generation.load() || bCloseRequested) return;
	if (Kind == OneMindTlsPrivate::KOpen) TransportListener->OnTransportOpen();
	else if (Kind == OneMindTlsPrivate::KData) TransportListener->OnTransportData(Data.data(), Data.size());
	else TransportListener->OnTransportClosed(Data);
}

uint32 FOneMindUeTlsTransport::Run()
{
	using namespace OneMindTlsPrivate;
	const uint64 Gen = RunGeneration;
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FAddressInfoResult Resolved = Subsystem->GetAddressInfo(*Host, *FString::FromInt(Port), EAddressInfoFlags::Default, NAME_None);
	if (Resolved.ReturnCode != SE_NO_ERROR || Resolved.Results.Num() == 0)
	{
		Deliver(Gen, KClosed, "resolve failed");
		return 0;
	}
	TSharedRef<FInternetAddr> Addr = Resolved.Results[0].Address;
	FSocket* Socket = Subsystem->CreateSocket(NAME_Stream, TEXT("OneMindTak"), Addr->GetProtocolType());
	if (!Socket || !Socket->Connect(*Addr))
	{
		if (Socket) Subsystem->DestroySocket(Socket);
		Deliver(Gen, KClosed, "connect failed");
		return 0;
	}
	Socket->SetNonBlocking(true);
	Socket->SetNoDelay(true);

	omtls::TlsSession Session;
	bool bOpened = false;
	std::string Err;
	if (bUseTls)
	{
		omtls::TlsConfig Cfg = Tls;
		if (Cfg.server_name.empty()) Cfg.server_name = TCHAR_TO_UTF8(*Host);
		if (!Session.Init(Cfg, Err))
		{
			Subsystem->DestroySocket(Socket);
			Deliver(Gen, KClosed, "tls: " + Err);
			return 0;
		}
	}
	else
	{
		bOpened = true;
		Deliver(Gen, KOpen, std::string());
	}

	std::string NetOut, Reason;
	uint8 Buf[16384];
	while (!bCloseRequested)
	{
		std::string Plain;
		{
			FScopeLock Lock(&Mutex);
			Plain.swap(OutBuf);
		}
		if (!Plain.empty())
		{
			if (bUseTls) Session.Write(Plain);
			else NetOut += Plain;
		}
		bool bEof = false;
		uint32 Pending = 0;
		while (Socket->HasPendingData(Pending) && Pending > 0)
		{
			int32 Read = 0;
			if (!Socket->Recv(Buf, sizeof(Buf), Read) || Read <= 0) break;
			if (bUseTls) Session.FeedNetwork(reinterpret_cast<const char*>(Buf), Read);
			else Deliver(Gen, KData, std::string(reinterpret_cast<const char*>(Buf), Read));
		}
		if (Socket->GetConnectionState() != SCS_Connected) bEof = true;
		if (bUseTls)
		{
			std::string In;
			const omtls::TlsSession::State St = Session.Pump(In);
			NetOut += Session.TakeNetwork();
			if (St == omtls::TlsSession::State::Open && !bOpened)
			{
				bOpened = true;
				Deliver(Gen, KOpen, std::string());
			}
			if (!In.empty()) Deliver(Gen, KData, In);
			if (St == omtls::TlsSession::State::Failed) { Reason = Session.error(); SendAll(Socket, NetOut, 0.2); break; }
			if (St == omtls::TlsSession::State::Closed) { Reason = "tls closed by peer"; break; }
		}
		if (!NetOut.empty())
		{
			int32 Sent = 0;
			if (Socket->Send(reinterpret_cast<const uint8*>(NetOut.data()), static_cast<int32>(NetOut.size()), Sent) && Sent > 0)
			{
				NetOut.erase(0, Sent);
			}
		}
		if (bEof) { Reason = bOpened ? "connection closed" : "connection closed during handshake"; break; }
		Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(20));
	}
	if (bCloseRequested)
	{
		std::string Rest;
		{
			FScopeLock Lock(&Mutex);
			Rest.swap(OutBuf);
		}
		if (bUseTls)
		{
			if (!Rest.empty()) Session.Write(Rest);
			std::string Ignore;
			Session.Pump(Ignore);
			Session.Shutdown();
			NetOut += Session.TakeNetwork();
		}
		else
		{
			NetOut += Rest;
		}
		SendAll(Socket, NetOut, 2.0);
		Socket->Shutdown(ESocketShutdownMode::Write);
	}
	Socket->Close();
	Subsystem->DestroySocket(Socket);
	if (!bCloseRequested) Deliver(Gen, KClosed, Reason);
	return 0;
}
