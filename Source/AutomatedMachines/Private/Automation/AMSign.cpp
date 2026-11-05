#include "Automation/AMSign.h"
#include "Buildables/FGBuildableWidgetSign.h"
#include "FGSignTypes.h"
#include "UObject/UnrealType.h"

FAMSignDesign AMSign::FromSignData(const FPrefabSignData& Data, const FString& Event)
{
	FAMSignDesign Design;
	Design.Event = Event;
	Design.Layout = FSoftClassPath(Data.PrefabLayout.ToString());
	for (const TPair<FString, FString>& Text : Data.TextElementData)
	{
		FAMSignText& Entry = Design.Texts.AddDefaulted_GetRef();
		Entry.Key = Text.Key;
		Entry.Value = Text.Value;
	}
	for (const TPair<FString, int32>& Icon : Data.IconElementData)
	{
		FAMSignIcon& Entry = Design.Icons.AddDefaulted_GetRef();
		Entry.Key = Icon.Key;
		Entry.Id = Icon.Value;
	}
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
	OutData.TextElementData.Reset();
	for (const FAMSignText& Text : Design.Texts)
	{
		OutData.TextElementData.Add(Text.Key, Text.Value);
	}
	OutData.IconElementData.Reset();
	for (const FAMSignIcon& Icon : Design.Icons)
	{
		OutData.IconElementData.Add(Icon.Key, Icon.Id);
	}
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
	// By name: the starter project's header adds a bool parameter the game's function does not have,
	// so a direct call does not link against the game (the mod's DLL fails to load, error 127).
	UFunction* Function = Sign->FindFunction(TEXT("SetPrefabSignData"));
	if (!Function)
	{
		return;
	}
	TArray<uint8> Params;
	Params.SetNumZeroed(FMath::Max<int32>(Function->ParmsSize, 1));
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params.GetData());
		if (const FStructProperty* DataParam = CastField<FStructProperty>(*It))
		{
			if (DataParam->Struct == FPrefabSignData::StaticStruct())
			{
				DataParam->CopyCompleteValue(DataParam->ContainerPtrToValuePtr<void>(Params.GetData()), &Data);
			}
		}
	}
	Sign->ProcessEvent(Function, Params.GetData());
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params.GetData());
	}
}
