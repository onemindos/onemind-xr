// om::ITransport over the engine's WebSockets module (IWebSocket). The engine does the
// RFC 6455 framing; NATS protocol bytes go out as binary messages and incoming binary
// messages are fed to the core parser as a byte stream (fragments are fine: NATS framing
// does not depend on WebSocket message boundaries).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "OneMindCore/NatsClient.h"

#include <atomic>
#include <memory>

class IWebSocket;

class FOneMindUeWebSocketTransport : public om::ITransport
{
public:
	virtual ~FOneMindUeWebSocketTransport() override;
	virtual void SetListener(Listener* InListener) override;
	virtual bool Open(const std::string& Url) override;
	virtual void Send(const std::string& Bytes) override;
	virtual void Close() override;

private:
	FCriticalSection Mutex;
	Listener* TransportListener = nullptr;
	TSharedPtr<IWebSocket> Socket;
	std::shared_ptr<std::atomic<uint64_t>> Generation = std::make_shared<std::atomic<uint64_t>>(0);
};
