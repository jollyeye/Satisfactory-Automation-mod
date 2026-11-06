#include "Automation/AMLight.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"

bool AMLight::IsLight(const UObject* Object)
{
	return Object && (Object->IsA<AFGBuildableLightSource>() || Object->IsA<AFGBuildableLightsControlPanel>());
}

FAMLightSetting AMLight::Read(const AFGBuildable* Light, const FString& Event)
{
	FAMLightSetting Setting;
	Setting.Event = Event;
	FLightSourceControlData Data;
	if (const AFGBuildableLightSource* Source = Cast<AFGBuildableLightSource>(Light))
	{
		Data = Source->GetLightControlData();
		Setting.bEnabled = Source->IsLightEnabled();
	}
	else if (const AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Light))
	{
		Data = Panel->GetLightControlData();
		Setting.bEnabled = Panel->IsLightEnabled();
	}
	Setting.ColorSlot = Data.ColorSlotIndex;
	Setting.Intensity = Data.Intensity;
	Setting.bNightMode = Data.IsTimeOfDayAware;
	return Setting;
}

void AMLight::Apply(AFGBuildable* Light, const FAMLightSetting& Setting)
{
	if (!Light || !Light->HasAuthority())
	{
		return;
	}
	FLightSourceControlData Data;
	Data.ColorSlotIndex = FMath::Clamp(Setting.ColorSlot, 0, 6);
	Data.Intensity = FMath::Clamp(Setting.Intensity, 0.1f, 1.f);
	Data.IsTimeOfDayAware = Setting.bNightMode;
	if (AFGBuildableLightSource* Source = Cast<AFGBuildableLightSource>(Light))
	{
		Source->SetLightControlData(Data);
		if (Source->IsLightEnabled() != Setting.bEnabled)
		{
			Source->SetLightEnabled(Setting.bEnabled);
		}
	}
	else if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Light))
	{
		Panel->SetLightControlData(Data);
		Panel->SetLightDataOnControlledLights(Data);
		if (Panel->IsLightEnabled() != Setting.bEnabled)
		{
			Panel->SetLightEnabled(Setting.bEnabled);
		}
	}
}
