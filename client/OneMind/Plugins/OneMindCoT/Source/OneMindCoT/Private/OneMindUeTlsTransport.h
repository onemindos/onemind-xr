// om::ITransport for the TAK streaming connection inside Unreal (feature C3): a TCP FSocket
// on its own FRunnable thread and, for "tls://", omtls::TlsSession (the same OpenSSL code the
// host transport uses and the host tests exercise). "tcp://" is plain TCP, used for TAK
// directed messages to the mesh gateway (TakDirect). Listener calls come from the I/O thread
// and only enqueue into the core, as om::ITransport requires.
// Not compiled on the build machine (no Unreal); the host twin is HostTests/host/PosixTlsTransport.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "OneMindCore/NatsClient.h"
#include "Tls/OmTlsSession.h"

#include <atomic>
#include <string>

class FRunnableThread;
class FSocket;

class FOneMindUeTlsTransport : public om::ITransport, public FRunnable
{
public:
	/** Outbound bytes not yet accepted by the socket; beyond this the connection is failed. */
	static constexpr size_t KMaxPendingBytes = 4u << 20;
	static constexpr double KConnectTimeoutSec = 10.0;

	explicit FOneMindUeTlsTransport(omtls::TlsConfig InTls = omtls::TlsConfig()) : Tls(MoveTemp(InTls)) {}
	virtual ~FOneMindUeTlsTransport() override;

	// om::ITransport
	virtual void SetListener(Listener* InListener) override;
	virtual bool Open(const std::string& Url) override;
	virtual void Send(const std::string& Bytes) override;
	virtual void Close() override;

	// FRunnable
	virtual uint32 Run() override;

private:
	void Deliver(uint64 Gen, int32 Kind, const std::string& Data);

	omtls::TlsConfig Tls;
	FCriticalSection Mutex;  // Listener, OutBuf, bOverflow
	Listener* TransportListener = nullptr;
	std::string OutBuf;
	bool bOverflow = false;
	FString Host;
	int32 Port = 0;
	bool bUseTls = false;
	FRunnableThread* Thread = nullptr;
	std::atomic<uint64> Generation{0};
	std::atomic<bool> bCloseRequested{false};
	uint64 RunGeneration = 0;
};
