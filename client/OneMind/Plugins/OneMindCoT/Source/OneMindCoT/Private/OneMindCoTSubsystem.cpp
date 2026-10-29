// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCoTSubsystem.h"

#include "Engine/GameInstance.h"
#include "OneMindBusSubsystem.h"
#include "OneMindCore/ClientCore.h"
#include "OneMindCore/CoT.h"
#include "OneMindCore/Drawing.h"
#include "OneMindTakSubsystem.h"

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
std::vector<om::GeoPoint> Points(const TArray<FVector2D>& LatLon)
{
	std::vector<om::GeoPoint> Out;
	for (const FVector2D& P : LatLon) Out.push_back(om::GeoPoint{P.X, P.Y, 9999999.0});
	return Out;
}

om::ShapeStyle Style(int64 Stroke, int64 Fill)
{
	om::ShapeStyle S;
	S.stroke_argb = static_cast<uint32_t>(Stroke);
	S.fill_argb = static_cast<uint32_t>(Fill);
	return S;
}

// svc.cot.send (bus, projector gates, mesh and server targets) plus the client's own TAK
// stream when connected; the projector and TAK Server both deduplicate by uid.
FString SendShape(UOneMindCoTSubsystem* Self, const om::CotEvent& Ev)
{
	if (UGameInstance* GI = Self->GetGameInstance())
	{
		if (UOneMindTakSubsystem* Tak = GI->GetSubsystem<UOneMindTakSubsystem>())
		{
			if (Tak->IsConnected()) Tak->SendCot(Ev);
		}
	}
	return Send(Self, "shape", Ev);
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

FString UOneMindCoTSubsystem::SendPoint(double Lat, double Lon, const FString& Label)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildPoint(Build(CoreOf(this), Label), om::GeoPoint{Lat, Lon, 9999999.0}, om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendLine(const TArray<FVector2D>& LatLonPoints, const FString& Label, int64 StrokeArgb)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildPolyline(Build(CoreOf(this), Label), Points(LatLonPoints), Style(StrokeArgb, 0), om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendPolygon(const TArray<FVector2D>& LatLonRing, const FString& Label, int64 StrokeArgb, int64 FillArgb)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildPolygon(Build(CoreOf(this), Label), Points(LatLonRing), Style(StrokeArgb, FillArgb), om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendRectangle(FVector2D C1, FVector2D C2, const FString& Label, int64 StrokeArgb, int64 FillArgb)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildRectangle(Build(CoreOf(this), Label), om::GeoPoint{C1.X, C1.Y, 0}, om::GeoPoint{C2.X, C2.Y, 0},
		Style(StrokeArgb, FillArgb), om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendCircle(double Lat, double Lon, double RadiusMeters, const FString& Label, int64 StrokeArgb, int64 FillArgb)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildCircle(Build(CoreOf(this), Label), om::GeoPoint{Lat, Lon, 9999999.0}, RadiusMeters,
		Style(StrokeArgb, FillArgb), om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendSector(double Lat, double Lon, double RadiusMeters, double FromBearingDeg, double ToBearingDeg, const FString& Label)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildSector(Build(CoreOf(this), Label), om::GeoPoint{Lat, Lon, 9999999.0}, RadiusMeters, FromBearingDeg,
		ToBearingDeg, Style(0xffffff00, 0x40ffff00), om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendRangeRings(double Lat, double Lon, const TArray<double>& RadiiMeters, const FString& Label)
{
	using namespace OneMindCotPrivate;
	std::vector<double> Radii(RadiiMeters.GetData(), RadiiMeters.GetData() + RadiiMeters.Num());
	FString First;
	for (const om::CotEvent& Ev : om::BuildRangeRings(Build(CoreOf(this), Label), om::GeoPoint{Lat, Lon, 9999999.0}, Radii,
		Style(0xffff0000, 0), om::WallClockMs()))
	{
		const FString Uid = SendShape(this, Ev);
		if (First.IsEmpty()) First = Uid;
	}
	return First;
}

FString UOneMindCoTSubsystem::SendRangeBearing(FVector2D From, FVector2D To, const FString& Label)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildRangeBearing(Build(CoreOf(this), Label), om::GeoPoint{From.X, From.Y, 0}, om::GeoPoint{To.X, To.Y, 0},
		Style(0xffff0000, 0), om::WallClockMs()));
}

FString UOneMindCoTSubsystem::SendBuffer(const TArray<FVector2D>& LatLonPoints, double DistanceMeters, const FString& Label)
{
	using namespace OneMindCotPrivate;
	return SendShape(this, om::BuildBuffer(Build(CoreOf(this), Label), Points(LatLonPoints), DistanceMeters,
		Style(0xffffa500, 0x40ffa500), om::WallClockMs()));
}

TArray<FString> UOneMindCoTSubsystem::LassoSelect(const TArray<FVector2D>& LatLonRing, const TArray<FString>& Ids, const TArray<FVector2D>& LatLons) const
{
	std::vector<om::LassoCandidate> C;
	for (int32 i = 0; i < Ids.Num() && i < LatLons.Num(); ++i) C.push_back({TCHAR_TO_UTF8(*Ids[i]), LatLons[i].X, LatLons[i].Y});
	TArray<FString> Out;
	for (const std::string& Id : om::LassoSelect(OneMindCotPrivate::Points(LatLonRing), C)) Out.Add(UTF8_TO_TCHAR(Id.c_str()));
	return Out;
}
