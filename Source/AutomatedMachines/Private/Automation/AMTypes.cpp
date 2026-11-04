#include "Automation/AMTypes.h"

FString AMRules::NormalizeEventName(const FString& Name)
{
	return Name.TrimStartAndEnd().ToUpper();
}

FText AMRules::GetConditionText(EAMCondition Condition)
{
	switch (Condition)
	{
	case EAMCondition::OutputFull:      return FText::FromString(TEXT("Output full"));
	case EAMCondition::OutputNotFull:   return FText::FromString(TEXT("Output not full"));
	case EAMCondition::StartsProducing: return FText::FromString(TEXT("Starts producing"));
	case EAMCondition::StopsProducing:  return FText::FromString(TEXT("Stops producing"));
	default:                            return FText::GetEmpty();
	}
}

FText AMRules::GetActionText(EAMAction Action)
{
	switch (Action)
	{
	case EAMAction::Pause:  return FText::FromString(TEXT("Pause"));
	case EAMAction::Resume: return FText::FromString(TEXT("Resume"));
	default:                return FText::GetEmpty();
	}
}
