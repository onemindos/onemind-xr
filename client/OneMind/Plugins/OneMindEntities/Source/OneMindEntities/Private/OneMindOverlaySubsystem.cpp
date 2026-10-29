// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindOverlaySubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "OneMindBusSubsystem.h"
#include "OneMindCore/ClientCore.h"
#include "OneMindEntityRenderer.h"
#include "OneMindGlobeSettingsBridge.h"
#include "OneMindLayerSubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void UOneMindOverlaySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UGameViewportClient* Viewport = InWorld.GetGameViewport();
	if (!Viewport) return;
	TWeakObjectPtr<UOneMindOverlaySubsystem> Weak(this);
	auto Bind = [Weak](FText (UOneMindOverlaySubsystem::*Fn)() const) {
		return TAttribute<FText>::CreateLambda([Weak, Fn]() { return Weak.IsValid() ? (Weak.Get()->*Fn)() : FText(); });
	};
	const FSlateColor Panel(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Overlay = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(12.f)
		[
			SNew(SBorder).BorderBackgroundColor(Panel).Padding(8.f)
			[
				SNew(STextBlock).Text(Bind(&UOneMindOverlaySubsystem::GetStatusText)).ColorAndOpacity(FLinearColor::White)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(12.f)
		[
			SNew(SBox).MaxDesiredWidth(560.f)
			[
				SNew(SBorder).BorderBackgroundColor(Panel).Padding(8.f)
				[
					SNew(STextBlock).Text(Bind(&UOneMindOverlaySubsystem::GetInspectorText)).AutoWrapText(true)
						.ColorAndOpacity(FLinearColor::White)
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(8.f)
		[
			SNew(STextBlock).Text(Bind(&UOneMindOverlaySubsystem::GetAttributionText)).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f))
		];
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 10);
}

void UOneMindOverlaySubsystem::Deinitialize()
{
	if (Overlay.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
	{
		GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

FText UOneMindOverlaySubsystem::GetStatusText() const
{
	UWorld* World = GetWorld();
	if (!World) return FText();
	FString S;
	UGameInstance* GI = World->GetGameInstance();
	if (UOneMindBusSubsystem* Bus = GI ? GI->GetSubsystem<UOneMindBusSubsystem>() : nullptr)
	{
		S += Bus->GetStatusText() + TEXT("\n");
	}
	if (UOneMindLayerSubsystem* L = World->GetSubsystem<UOneMindLayerSubsystem>())
	{
		int32 Key = 1;
		for (const FOneMindLayerStatus& St : L->GetLayers())
		{
			S += FString::Printf(TEXT("[%d] %-10s %-4s %-12s %6d  feed:%s %s\n"), Key++, *St.Id, St.bOn ? TEXT("on") : TEXT("off"),
				*St.State, St.Entities, *St.Freshness, *St.LastError.Left(60));
		}
	}
	for (TActorIterator<AOneMindEntityRenderer> It(World); It; ++It)
	{
		S += It->GetStatsText() + TEXT("\n");
		break;
	}
	S += TEXT("keys: 1-9 toggle layer | left click select | M send marker at cursor | Esc clear");
	return FText::FromString(S);
}

void UOneMindOverlaySubsystem::SelectEntity(const FString& LayerAndId)
{
	SelectedId = LayerAndId;
	RawCot.Reset();
	InspectorText = LayerAndId.IsEmpty() ? FString() : TEXT("loading ") + LayerAndId;
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UOneMindBusSubsystem* Bus = GI ? GI->GetSubsystem<UOneMindBusSubsystem>() : nullptr;
	if (!Bus || !Bus->GetCore() || LayerAndId.IsEmpty()) return;
	FString Layer, Id;
	if (!LayerAndId.Split(TEXT("/"), &Layer, &Id)) Id = LayerAndId;
	const std::string EntityId = TCHAR_TO_UTF8(*Id);
	TWeakObjectPtr<UOneMindOverlaySubsystem> Weak(this);
	om::ClientCore* Core = Bus->GetCore();
	Core->FetchEntity(EntityId, [Weak, Core, EntityId, LayerAndId](const om::EntityDetails* D, const std::string& Err)
	{
		if (!Weak.IsValid() || Weak->SelectedId != LayerAndId) return;
		FString T;
		if (!D)
		{
			T = FString::Printf(TEXT("%s\nno details (%s)"), *LayerAndId, UTF8_TO_TCHAR(Err.c_str()));
		}
		else
		{
			FString Section;
			for (const om::InspectorRow& R : om::BuildInspectorRows(*D, om::WallClockMs()))
			{
				const FString Sec = UTF8_TO_TCHAR(R.section.c_str());
				if (Sec != Section)
				{
					Section = Sec;
					T += TEXT("\n") + Sec.ToUpper() + TEXT("\n");
				}
				T += FString::Printf(TEXT("  %s: %s\n"), UTF8_TO_TCHAR(R.key.c_str()), UTF8_TO_TCHAR(R.value.c_str()));
			}
			if (!Err.empty()) T += FString::Printf(TEXT("(svc.entity.get: %s; showing the last record seen)\n"), UTF8_TO_TCHAR(Err.c_str()));
		}
		Weak->InspectorText = T;
		Core->FetchRawCot(EntityId, [Weak, LayerAndId](const std::string& Xml, const std::string& E2)
		{
			if (!Weak.IsValid() || Weak->SelectedId != LayerAndId) return;
			Weak->RawCot = E2.empty() ? FString(UTF8_TO_TCHAR(Xml.c_str())) : FString::Printf(TEXT("(raw CoT: %s)"), UTF8_TO_TCHAR(E2.c_str()));
		});
	});
}

void UOneMindOverlaySubsystem::ClearSelection()
{
	SelectedId.Reset();
	InspectorText.Reset();
	RawCot.Reset();
}

FText UOneMindOverlaySubsystem::GetInspectorText() const
{
	if (SelectedId.IsEmpty()) return FText::FromString(TEXT("Inspector: click an entity"));
	FString T = TEXT("INSPECTOR ") + SelectedId + TEXT("\n") + InspectorText;
	if (!RawCot.IsEmpty()) T += TEXT("\nRAW COT\n") + RawCot.Left(2000);
	return FText::FromString(T);
}

FText UOneMindOverlaySubsystem::GetAttributionText() const
{
	return FText::FromString(OneMindGlobeAttribution());
}
