// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindKeychain.h"

#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"

#if PLATFORM_MAC
#include <Security/Security.h>
#endif

namespace OneMindKeychain
{
#if PLATFORM_MAC
namespace
{
const char* KService = "OneMind TAK";

CFMutableDictionaryRef Query(const FString& Item)
{
	CFMutableDictionaryRef Q = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFStringRef Service = CFStringCreateWithCString(nullptr, KService, kCFStringEncodingUTF8);
	CFStringRef Account = CFStringCreateWithCString(nullptr, TCHAR_TO_UTF8(*Item), kCFStringEncodingUTF8);
	CFDictionarySetValue(Q, kSecClass, kSecClassGenericPassword);
	CFDictionarySetValue(Q, kSecAttrService, Service);
	CFDictionarySetValue(Q, kSecAttrAccount, Account);
	CFRelease(Service);
	CFRelease(Account);
	return Q;
}
}  // namespace

bool Store(const FString& Item, const std::string& Pem)
{
	Remove(Item);
	CFMutableDictionaryRef Q = Query(Item);
	CFDataRef Data = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(Pem.data()), static_cast<CFIndex>(Pem.size()));
	CFDictionarySetValue(Q, kSecValueData, Data);
	CFDictionarySetValue(Q, kSecAttrAccessible, kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
	const OSStatus St = SecItemAdd(Q, nullptr);
	CFRelease(Data);
	CFRelease(Q);
	return St == errSecSuccess;
}

bool Load(const FString& Item, std::string& OutPem)
{
	CFMutableDictionaryRef Q = Query(Item);
	CFDictionarySetValue(Q, kSecReturnData, kCFBooleanTrue);
	CFDictionarySetValue(Q, kSecMatchLimit, kSecMatchLimitOne);
	CFTypeRef Result = nullptr;
	const OSStatus St = SecItemCopyMatching(Q, &Result);
	CFRelease(Q);
	if (St != errSecSuccess || !Result) return false;
	CFDataRef Data = static_cast<CFDataRef>(Result);
	OutPem.assign(reinterpret_cast<const char*>(CFDataGetBytePtr(Data)), static_cast<size_t>(CFDataGetLength(Data)));
	CFRelease(Result);
	return true;
}

bool Remove(const FString& Item)
{
	CFMutableDictionaryRef Q = Query(Item);
	const OSStatus St = SecItemDelete(Q);
	CFRelease(Q);
	return St == errSecSuccess || St == errSecItemNotFound;
}
#else
namespace
{
FString EnvFor(const FString& Item)
{
	if (Item == TEXT("client-key")) return FPlatformMisc::GetEnvironmentVariable(TEXT("ONEMIND_TAK_KEY_FILE"));
	if (Item == TEXT("client-cert")) return FPlatformMisc::GetEnvironmentVariable(TEXT("ONEMIND_TAK_CERT_FILE"));
	if (Item == TEXT("server-ca")) return FPlatformMisc::GetEnvironmentVariable(TEXT("ONEMIND_TAK_CA_FILE"));
	return FString();
}
}  // namespace

bool Store(const FString& Item, const std::string& Pem)
{
	return false;  // no secure store wired on this platform yet: enroll on the Mac, or provide files
}

bool Load(const FString& Item, std::string& OutPem)
{
	const FString Path = EnvFor(Item);
	FString Text;
	if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Text, *Path)) return false;
	OutPem = TCHAR_TO_UTF8(*Text);
	return true;
}

bool Remove(const FString& Item)
{
	return false;
}
#endif
}  // namespace OneMindKeychain
