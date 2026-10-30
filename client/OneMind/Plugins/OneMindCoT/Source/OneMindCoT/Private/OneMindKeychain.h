// Storage for the client's TAK identity (feature C3): the private key, the signed certificate
// and the server CA chain from enrollment. On the Mac they go into the login keychain as
// generic passwords (service "OneMind TAK", account = item name) through Security.framework,
// so nothing is written to the project, Saved/ or Git (CLAUDE.md rule 4). Other platforms read
// PEM files named by environment variables (ONEMIND_TAK_CERT_FILE, _KEY_FILE, _CA_FILE) and
// never write.
// Not compiled on the build machine (no Unreal).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"

#include <string>

namespace OneMindKeychain
{
/** Items: "client-key", "client-cert", "server-ca". */
bool Store(const FString& Item, const std::string& Pem);
bool Load(const FString& Item, std::string& OutPem);
bool Remove(const FString& Item);
}  // namespace OneMindKeychain
