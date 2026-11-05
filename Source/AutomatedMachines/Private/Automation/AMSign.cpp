#include "Automation/AMSign.h"
#include "Buildables/FGBuildableWidgetSign.h"
#include "FGSignTypes.h"

FAMSignDesign AMSign::FromSignData(const FPrefabSignData& Data, const FString& Event)
{
	FAMSignDesign Design;
	Design.Event = Event;
	Design.Layout = FSoftClassPath(Data.PrefabLayout.ToString());
	Design.Texts = Data.TextElementData;
	Design.Icons = Data.IconElementData;
	Design.Foreground = Data.ForegroundColor;
	Design.Background = Data.BackgroundColor;
	Design.Auxiliary = Data.AuxiliaryColor;
	Design.Emissive = Data.Emissive;
	Design.Glossiness = Data.Glossiness;
	Design.SignType = Data.SignTypeDesc.Get();
	return Design;
}

void AMSign::ToSignData(const FAMSignDesign& Design, FPrefabSignData& OutData)
{
	OutData.PrefabLayout = TSoftClassPtr<UFGSignPrefabWidget>(Design.Layout);
	OutData.TextElementData = Design.Texts;
	OutData.IconElementData = Design.Icons;
	OutData.ForegroundColor = Design.Foreground;
	OutData.BackgroundColor = Design.Background;
	OutData.AuxiliaryColor = Design.Auxiliary;
	OutData.Emissive = Design.Emissive;
	OutData.Glossiness = Design.Glossiness;
	if (Design.SignType)
	{
		OutData.SignTypeDesc = Design.SignType.Get();
	}
}

void AMSign::Show(AFGBuildableWidgetSign* Sign, const FAMSignDesign& Design)
{
	if (!Sign)
	{
		return;
	}
	// Keep what the design does not hold (e.g. who edited it last).
	FPrefabSignData Data;
	Sign->GetSignPrefabData(Data);
	ToSignData(Design, Data);
	Sign->SetPrefabSignData(Data, false);
}
