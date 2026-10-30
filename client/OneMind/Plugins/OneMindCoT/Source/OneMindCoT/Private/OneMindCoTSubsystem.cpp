// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCoTSubsystem.h"

#include "Engine/GameInstance.h"
#include "OneMindBusSubsystem.h"
#include "OneMindCore/ClientCore.h"
#include "OneMindCore/CoT.h"

DEFINE_LOG_CATEGORY_STATIC(LogOneMindCoT, Log, All);

namespace OneMindCotPrivate
{
om::ClientCore* CoreOf(UGameInstanceSubsystem* Self)
{
	UGameInstance* GI = Self->GetGameInstance();
	UOneMindBusSubsystem* Bus = GI ? GI->GetSubsystem<UOneMindBusSubsystem>() : nullptr;
	return Bus ? Bus->GetCore() : nullptr;
}

FString Send(UOneMindCoTSubsystem* Self, const char* Kind, const om::CotEvent& Ev)
{
	om::ClientCore* Core = CoreOf(Self);
	const FString Uid = UTF8_TO_TCHAR(Ev.GetAttribute("uid").c_str());
	if (!Core)
	{
		Self->OnSent.Broadcast(false, Uid, TEXT("bus not running"));
		return Uid;
	}
	TWeakObjectPtr<UOneMindCoTSubsystem> Weak(Self);
	Core->SendCot(Kind, Ev, [Weak, Uid](const om::CotSendReply& R, const std::string& Err)
	{
		UE_LOG(LogOneMindCoT, Log, TEXT("svc.cot.send %s: %s %s"), *Uid, R.ok ? TEXT("ok") : TEXT("failed"), UTF8_TO_TCHAR(Err.c_str()));
		if (Weak.IsValid()) Weak->OnSent.Broadcast(R.ok, Uid, UTF8_TO_TCHAR(Err.c_str()));
	});
	return Uid;
}

om::CotBuild Build(om::ClientCore* Core, const FString& Callsign)
{
	om::CotBuild B;
	B.uid = Core ? Core->NewCotUid() : std::string("onemind-unbound");
	B.callsign = TCHAR_TO_UTF8(*Callsign);
	B.simulated = true;  // run rule 6 / decision 0004 #13: everything in this run is simulated
	return B;
}
}  // namespace OneMindCotPrivate

FString UOneMindCoTSubsystem::SendMarker(double Lat, double Lon, const FString& Callsign, const FString& CotType, const FString& Remarks)
{
	om::ClientCore* Core = OneMindCotPrivate::CoreOf(this);
	om::CotBuild B = OneMindCotPrivate::Build(Core, Callsign);
	B.remarks = TCHAR_TO_UTF8(*Remarks);
	const om::CotEvent Ev = om::BuildMarker(B, TCHAR_TO_UTF8(*CotType), om::GeoPoint{Lat, Lon, 9999999.0}, om::WallClockMs());
	return OneMindCotPrivate::Send(this, "marker", Ev);
}

FString UOneMindCoTSubsystem::SendSpi(double Lat, double Lon, const FString& Callsign)
{
	om::ClientCore* Core = OneMindCotPrivate::CoreOf(this);
	const om::CotEvent Ev = om::BuildSpi(OneMindCotPrivate::Build(Core, Callsign), om::GeoPoint{Lat, Lon, 9999999.0}, om::WallClockMs());
	return OneMindCotPrivate::Send(this, "spi", Ev);
}

FString UOneMindCoTSubsystem::SendRoute(const TArray<FVector2D>& LatLonPoints, const FString& Callsign)
{
	om::ClientCore* Core = OneMindCotPrivate::CoreOf(this);
	std::vector<om::GeoPoint> Wps;
	for (const FVector2D& P : LatLonPoints) Wps.push_back(om::GeoPoint{P.X, P.Y, 9999999.0});
	return OneMindCotPrivate::Send(this, "route", om::BuildRoute(OneMindCotPrivate::Build(Core, Callsign), Wps, om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendGeofence(const TArray<FVector2D>& LatLonRing, const FString& Callsign, const FString& Kind)
{
	om::ClientCore* Core = OneMindCotPrivate::CoreOf(this);
	std::vector<om::GeoPoint> Ring;
	for (const FVector2D& P : LatLonRing) Ring.push_back(om::GeoPoint{P.X, P.Y, 9999999.0});
	return OneMindCotPrivate::Send(this, "geofence",
		om::BuildGeofence(OneMindCotPrivate::Build(Core, Callsign), Ring, TCHAR_TO_UTF8(*Kind), om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendChat(const FString& Text, const FString& Room)
{
	om::ClientCore* Core = OneMindCotPrivate::CoreOf(this);
	const std::string Sender = Core ? "onemind-" + Core->config().client_id : std::string("onemind");
	om::CotBuild B = OneMindCotPrivate::Build(Core, TEXT("OneMind"));
	const std::string RoomStd = Room.IsEmpty() ? std::string("All Chat Rooms") : std::string(TCHAR_TO_UTF8(*Room));
	B.uid = "GeoChat." + Sender + "." + RoomStd + "." + B.uid;
	return OneMindCotPrivate::Send(this, "chat", om::BuildChat(B, Sender, RoomStd, TCHAR_TO_UTF8(*Text), om::WallClockMs()));
}
