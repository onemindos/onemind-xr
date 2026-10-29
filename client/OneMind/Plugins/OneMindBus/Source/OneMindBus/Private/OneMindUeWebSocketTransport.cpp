// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindUeWebSocketTransport.h"

#include "IWebSocket.h"
#include "Modules/ModuleManager.h"
#include "WebSocketsModule.h"

FOneMindUeWebSocketTransport::~FOneMindUeWebSocketTransport()
{
	SetListener(nullptr);
	Close();
}

void FOneMindUeWebSocketTransport::SetListener(Listener* InListener)
{
	FScopeLock Lock(&Mutex);
	TransportListener = InListener;
}

bool FOneMindUeWebSocketTransport::Open(const std::string& Url)
{
	Close();
	const FString UrlStr = UTF8_TO_TCHAR(Url.c_str());
	if (!UrlStr.StartsWith(TEXT("ws://")) && !UrlStr.StartsWith(TEXT("wss://")))
	{
		return false;
	}
	FWebSocketsModule& Module = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	Socket = Module.CreateWebSocket(UrlStr, FString());

	const uint64_t Gen = ++(*Generation);
	std::shared_ptr<std::atomic<uint64_t>> GenRef = Generation;
	auto Live = [GenRef, Gen]() { return GenRef->load() == Gen; };

	Socket->OnConnected().AddLambda([this, Live]()
	{
		if (!Live()) return;
		FScopeLock Lock(&Mutex);
		if (TransportListener) TransportListener->OnTransportOpen();
	});
	Socket->OnConnectionError().AddLambda([this, Live](const FString& Error)
	{
		if (!Live()) return;
		FScopeLock Lock(&Mutex);
		if (TransportListener) TransportListener->OnTransportClosed(std::string("websocket error: ") + TCHAR_TO_UTF8(*Error));
	});
	Socket->OnClosed().AddLambda([this, Live](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		if (!Live()) return;
		FScopeLock Lock(&Mutex);
		if (TransportListener)
		{
			TransportListener->OnTransportClosed(std::string("websocket closed ") + std::to_string(StatusCode) + " " + TCHAR_TO_UTF8(*Reason));
		}
	});
	// nats-server sends binary frames; each raw message (or fragment) is appended to the
	// NATS byte stream in order.
	Socket->OnRawMessage().AddLambda([this, Live](const void* Data, SIZE_T Size, SIZE_T /*BytesRemaining*/)
	{
		if (!Live()) return;
		FScopeLock Lock(&Mutex);
		if (TransportListener) TransportListener->OnTransportData(static_cast<const char*>(Data), static_cast<size_t>(Size));
	});
	Socket->Connect();
	return true;
}

void FOneMindUeWebSocketTransport::Send(const std::string& Bytes)
{
	if (Socket.IsValid() && Socket->IsConnected())
	{
		Socket->Send(Bytes.data(), static_cast<SIZE_T>(Bytes.size()), /*bIsBinary=*/true);
	}
}

void FOneMindUeWebSocketTransport::Close()
{
	++(*Generation);  // late callbacks from the old socket are ignored
	if (Socket.IsValid())
	{
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnRawMessage().Clear();
		if (Socket->IsConnected())
		{
			Socket->Close();
		}
		Socket.Reset();
	}
}
