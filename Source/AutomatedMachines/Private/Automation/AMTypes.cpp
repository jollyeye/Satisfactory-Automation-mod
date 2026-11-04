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

FText AMRules::GetConditionDescription(EAMCondition Condition)
{
	switch (Condition)
	{
	case EAMCondition::OutputFull:
		return FText::FromString(TEXT("Rings when the output slot fills up: there is no room for the next batch, so the machine is about to stop.\n\nUse it to pause the machines that feed this one, so they stop wasting power and resources."));
	case EAMCondition::OutputNotFull:
		return FText::FromString(TEXT("Rings when the output slot has room again, for example after a belt or a player took items out.\n\nPair it with Output full to resume the machines you paused."));
	case EAMCondition::StartsProducing:
		return FText::FromString(TEXT("Rings when the machine starts making items: after it was paused, had no power, no input or a full output."));
	case EAMCondition::StopsProducing:
		return FText::FromString(TEXT("Rings when the machine stops making items, for any reason: no input, full output, no power, or paused."));
	default:
		return FText::GetEmpty();
	}
}

FText AMRules::GetActionDescription(EAMAction Action)
{
	switch (Action)
	{
	case EAMAction::Pause:
		return FText::FromString(TEXT("Puts the machine on standby, like its own standby button: it makes nothing and uses no power until it is resumed."));
	case EAMAction::Resume:
		return FText::FromString(TEXT("Takes the machine off standby, so it works again. Does nothing if it is already running."));
	default:
		return FText::GetEmpty();
	}
}
