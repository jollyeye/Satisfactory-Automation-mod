#include "Automation/AMTypes.h"
#include "FGRecipe.h"
#include "ItemAmount.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableStorage.h"

namespace
{
	FText AMTY_Text(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}
}

FString AMRules::NormalizeEventName(const FString& Name)
{
	return Name.TrimStartAndEnd().ToUpper();
}

FText AMRules::GetConditionText(EAMCondition Condition)
{
	switch (Condition)
	{
	case EAMCondition::OutputFull:        return AMTY_Text(TEXT("Output full"));
	case EAMCondition::OutputNotFull:     return AMTY_Text(TEXT("Output not full"));
	case EAMCondition::StartsProducing:   return AMTY_Text(TEXT("Starts producing"));
	case EAMCondition::StopsProducing:    return AMTY_Text(TEXT("Stops producing"));
	case EAMCondition::LosesPower:        return AMTY_Text(TEXT("Loses power"));
	case EAMCondition::GetsPower:         return AMTY_Text(TEXT("Gets power"));
	case EAMCondition::InputMissing:      return AMTY_Text(TEXT("Input missing"));
	case EAMCondition::InputAvailable:    return AMTY_Text(TEXT("Input available"));
	case EAMCondition::Paused:            return AMTY_Text(TEXT("Paused"));
	case EAMCondition::Resumed:           return AMTY_Text(TEXT("Resumed"));
	case EAMCondition::ProductivityBelow: return AMTY_Text(TEXT("Productivity below"));
	case EAMCondition::ProductivityAbove: return AMTY_Text(TEXT("Productivity above"));
	case EAMCondition::RecipeChanged:     return AMTY_Text(TEXT("Recipe changed"));
	case EAMCondition::InputFull:         return AMTY_Text(TEXT("Input full"));
	case EAMCondition::InputNotFull:      return AMTY_Text(TEXT("Input not full"));
	case EAMCondition::WrongInput:        return AMTY_Text(TEXT("Wrong input"));
	case EAMCondition::InputCorrect:      return AMTY_Text(TEXT("Input correct"));
	case EAMCondition::StorageFull:       return AMTY_Text(TEXT("Full"));
	case EAMCondition::StorageNotFull:    return AMTY_Text(TEXT("Not full"));
	case EAMCondition::StorageEmpty:      return AMTY_Text(TEXT("Empty"));
	case EAMCondition::StorageNotEmpty:   return AMTY_Text(TEXT("Not empty"));
	case EAMCondition::FillAbove:         return AMTY_Text(TEXT("Filled above"));
	case EAMCondition::FillBelow:         return AMTY_Text(TEXT("Filled below"));
	case EAMCondition::BeltBlocked:       return AMTY_Text(TEXT("Belt blocked"));
	case EAMCondition::BeltFlowing:       return AMTY_Text(TEXT("Belt flowing"));
	default:                              return FText::GetEmpty();
	}
}

FText AMRules::GetActionText(EAMAction Action)
{
	switch (Action)
	{
	case EAMAction::Pause:         return AMTY_Text(TEXT("Pause"));
	case EAMAction::Resume:        return AMTY_Text(TEXT("Resume"));
	case EAMAction::TogglePause:   return AMTY_Text(TEXT("Pause or resume"));
	case EAMAction::SetClockSpeed: return AMTY_Text(TEXT("Set clock speed"));
	case EAMAction::DeleteOutput:  return AMTY_Text(TEXT("Delete output"));
	case EAMAction::DeleteInput:   return AMTY_Text(TEXT("Delete input"));
	case EAMAction::ChangeRecipe:  return AMTY_Text(TEXT("Change recipe"));
	case EAMAction::ShowMessage:   return AMTY_Text(TEXT("Show message"));
	case EAMAction::PlaySound:     return AMTY_Text(TEXT("Play sound"));
	case EAMAction::DeleteContents: return AMTY_Text(TEXT("Delete contents"));
	default:                       return FText::GetEmpty();
	}
}

FText AMRules::GetConditionDescription(EAMCondition Condition)
{
	switch (Condition)
	{
	case EAMCondition::OutputFull:
		return AMTY_Text(TEXT("Rings when the output slot fills up: there is no room for the next batch, so the machine is about to stop.\n\nUse it to pause the machines that feed this one, so they stop wasting power and resources."));
	case EAMCondition::OutputNotFull:
		return AMTY_Text(TEXT("Rings when the output slot has room again, for example after a belt or a player took items out.\n\nPair it with Output full to resume the machines you paused."));
	case EAMCondition::StartsProducing:
		return AMTY_Text(TEXT("Rings when the machine starts making items: after it was paused, had no power, no input or a full output."));
	case EAMCondition::StopsProducing:
		return AMTY_Text(TEXT("Rings when the machine stops making items, for any reason: no input, full output, no power, or paused."));
	case EAMCondition::LosesPower:
		return AMTY_Text(TEXT("Rings when the machine has no power any more: its power circuit is down (a fuse blew, the generators stopped) or it was disconnected."));
	case EAMCondition::GetsPower:
		return AMTY_Text(TEXT("Rings when the machine has power again, e.g. after you reset a blown fuse or connected it."));
	case EAMCondition::InputMissing:
		return AMTY_Text(TEXT("Rings when the input slots do not have enough of an ingredient for the next batch, so the machine has to wait.\n\nUse it to start the machines that supply this one."));
	case EAMCondition::InputAvailable:
		return AMTY_Text(TEXT("Rings when the input slots have every ingredient for the next batch again."));
	case EAMCondition::Paused:
		return AMTY_Text(TEXT("Rings when the machine is put on standby: with its standby button, or by an action of an alarm."));
	case EAMCondition::Resumed:
		return AMTY_Text(TEXT("Rings when the machine is taken off standby: with its standby button, or by an action of an alarm."));
	case EAMCondition::ProductivityBelow:
		return AMTY_Text(TEXT("Rings when the productivity shown in the machine's window drops below the percentage you set: the machine spent too much time waiting for input, for room in the output, or for power."));
	case EAMCondition::ProductivityAbove:
		return AMTY_Text(TEXT("Rings when the productivity shown in the machine's window rises above the percentage you set."));
	case EAMCondition::RecipeChanged:
		return AMTY_Text(TEXT("Rings when someone picks another recipe for this machine (also when an action changes it)."));
	case EAMCondition::InputFull:
		return AMTY_Text(TEXT("Rings when an input slot is full and cannot take more of its ingredient: more arrives than the machine uses, so the belt or pipe feeding it backs up.\n\nUse it to slow down or pause the machines that supply this one."));
	case EAMCondition::InputNotFull:
		return AMTY_Text(TEXT("Rings when every input slot has room again for more of its ingredient."));
	case EAMCondition::WrongInput:
		return AMTY_Text(TEXT("Rings when a belt or pipe brings something that is not an ingredient of the recipe. The machine refuses it, so the belt stops behind it.\n\nUse it to get a message, or switch on \"Pass wrong items through\" in Options."));
	case EAMCondition::InputCorrect:
		return AMTY_Text(TEXT("Rings when no belt or pipe brings a wrong item any more."));
	case EAMCondition::StorageFull:
		return AMTY_Text(TEXT("Rings when the container is full: every slot holds a full stack, so belts cannot bring anything more.\n\nUse it to pause the machines that fill it."));
	case EAMCondition::StorageNotFull:
		return AMTY_Text(TEXT("Rings when the container has room again (a slot is empty or not a full stack)."));
	case EAMCondition::StorageEmpty:
		return AMTY_Text(TEXT("Rings when the last item has left the container.\n\nUse it to start the machines that fill it."));
	case EAMCondition::StorageNotEmpty:
		return AMTY_Text(TEXT("Rings when the first item arrives in an empty container."));
	case EAMCondition::FillAbove:
		return AMTY_Text(TEXT("Rings when the container is filled above the percentage you set. 100% is every slot holding a full stack; a slot with half a stack counts as half a slot.\n\nUse it to slow down or pause the machines that fill it before it is completely full."));
	case EAMCondition::FillBelow:
		return AMTY_Text(TEXT("Rings when the container is filled below the percentage you set (counted like Filled above).\n\nUse it to start the machines that fill it again before it runs empty."));
	case EAMCondition::BeltBlocked:
		return AMTY_Text(TEXT("Rings when a belt brings an item the container has no room for, so the belt stops. This can happen before the container is full: the item's stacks are full and there is no empty slot left for it.\n\nUse it to pause the machines that fill it, or to empty the container."));
	case EAMCondition::BeltFlowing:
		return AMTY_Text(TEXT("Rings when the container can take the items of every belt again."));
	default:
		return FText::GetEmpty();
	}
}

FText AMRules::GetActionDescription(EAMAction Action)
{
	switch (Action)
	{
	case EAMAction::Pause:
		return AMTY_Text(TEXT("Puts the machine on standby, like its own standby button: it makes nothing and uses no power until it is resumed."));
	case EAMAction::Resume:
		return AMTY_Text(TEXT("Takes the machine off standby, so it works again. Does nothing if it is already running."));
	case EAMAction::TogglePause:
		return AMTY_Text(TEXT("Switches standby: a running machine is paused, a paused machine is resumed. Handy for one alarm that turns something on and off."));
	case EAMAction::SetClockSpeed:
		return AMTY_Text(TEXT("Sets the machine's clock speed to the percentage you choose, like the clock speed slider in its window. Above 100% needs Power Shards in the machine; the speed is limited to what its shards allow."));
	case EAMAction::DeleteOutput:
		return AMTY_Text(TEXT("Destroys everything in the machine's output slots. The items are gone for good: no points, no refund.\n\nWith \"Output full\" on the same machine, it never stops for a full output: what nobody takes away is thrown away."));
	case EAMAction::DeleteInput:
		return AMTY_Text(TEXT("Destroys everything in the machine's input slots, including fluids. The items are gone for good: no points, no refund.\n\nHandy to clear out leftovers, e.g. after the recipe was changed."));
	case EAMAction::ShowMessage:
		return AMTY_Text(TEXT("Shows a message on the screen of every player, in the game's notifications.\n\nIt shows the alarm's name with your text under it; leave the text empty to show which machine rang it, e.g. \"PLATES BACKED UP - Rung by Constructor\"."));
	case EAMAction::PlaySound:
		return AMTY_Text(TEXT("Plays a short sound for every player, wherever they are. Choose one of a few sounds of the game; picking one in the list plays it once so you can hear it."));
	case EAMAction::DeleteContents:
		return AMTY_Text(TEXT("Destroys everything in the container. The items are gone for good: no points, no refund.\n\nWith \"Full\" or \"Filled above\" on the same container, it works as an overflow that throws away what nobody uses."));
	case EAMAction::ChangeRecipe:
		return AMTY_Text(TEXT("Switches the machine to the recipe you choose.\n\nThe game only allows this with empty slots, so everything in the input and output slots is destroyed first (no refund). Does nothing if the machine already makes this recipe."));
	default:
		return FText::GetEmpty();
	}
}

AMRules::FValueInfo AMRules::GetConditionValueInfo(EAMCondition Condition)
{
	FValueInfo Info;
	if (Condition == EAMCondition::ProductivityBelow || Condition == EAMCondition::ProductivityAbove)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("Productivity");
		Info.Min = 0.f;
		Info.Max = 100.f;
		Info.Default = 50.f;
	}
	else if (Condition == EAMCondition::FillAbove || Condition == EAMCondition::FillBelow)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("Filled");
		Info.Min = 0.f;
		Info.Max = 100.f;
		Info.Default = Condition == EAMCondition::FillAbove ? 90.f : 10.f;
	}
	return Info;
}

AMRules::FValueInfo AMRules::GetActionValueInfo(EAMAction Action)
{
	FValueInfo Info;
	if (Action == EAMAction::SetClockSpeed)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("Clock speed");
		Info.Min = 1.f;
		Info.Max = 250.f;
		Info.Default = 100.f;
	}
	return Info;
}

bool AMRules::ActionNeedsRecipe(EAMAction Action)
{
	return Action == EAMAction::ChangeRecipe;
}

bool AMRules::ActionNeedsMessage(EAMAction Action)
{
	return Action == EAMAction::ShowMessage;
}

bool AMRules::IsWorldAction(EAMAction Action)
{
	return Action == EAMAction::ShowMessage || Action == EAMAction::PlaySound;
}

bool AMRules::ActionNeedsSound(EAMAction Action)
{
	return Action == EAMAction::PlaySound;
}

const TArray<AMRules::FSound>& AMRules::GetSounds()
{
	// Chosen from the game's sound events by name (short, not looping: no matching Stop_ event).
	static const TArray<FSound> Sounds = {
		{ TEXT("Blip"), TEXT("/Game/WwiseAudio/Events/Interface/CodecMessage/Play_UI_CodexMessage_Blip.Play_UI_CodexMessage_Blip"),
			TEXT("A short blip, the sound of a new message arriving.") },
		{ TEXT("Ping"), TEXT("/Game/WwiseAudio/Events/Equipment/ResourcePing/Play_EQ_Resource_Ping_Fire.Play_EQ_Resource_Ping_Fire"),
			TEXT("The ping of the resource scanner.") },
		{ TEXT("Warning"), TEXT("/Game/WwiseAudio/Events/Character/Player/TakeDamage/Play_P_Suit_Damage_Warning.Play_P_Suit_Damage_Warning"),
			TEXT("The warning of your suit when you take damage: for things that need you now.") },
		{ TEXT("Alert"), TEXT("/Game/WwiseAudio/Events/Buildable/Factory/Fracker/Play_FrackerSmasher_Alarm.Play_FrackerSmasher_Alarm"),
			TEXT("The alarm of the Resource Well Pressurizer: loud, made to be heard over running machines.") },
		{ TEXT("Success"), TEXT("/Game/WwiseAudio/Events/Buildable/Factory/MAM/Play_MAM_Scanner_Complete_Fanfare.Play_MAM_Scanner_Complete_Fanfare"),
			TEXT("The fanfare of a finished MAM research: for good news.") },
	};
	return Sounds;
}

int32 AMRules::GetSoundIndex(float Value)
{
	return FMath::Clamp(FMath::RoundToInt(Value), 0, GetSounds().Num() - 1);
}

FText AMRules::DescribeRecipe(const UObject* WorldContext, TSubclassOf<UFGRecipe> Recipe)
{
	if (!Recipe)
	{
		return FText::GetEmpty();
	}
	auto Describe = [](const TArray<FItemAmount>& Amounts)
	{
		TArray<FString> Parts;
		for (const FItemAmount& Amount : Amounts)
		{
			const EResourceForm Form = UFGItemDescriptor::GetForm(Amount.ItemClass);
			const bool bFluid = Form == EResourceForm::RF_LIQUID || Form == EResourceForm::RF_GAS;
			const FString Count = bFluid ? FString::Printf(TEXT("%g m3"), Amount.Amount / 1000.f) : FString::FromInt(Amount.Amount);
			Parts.Add(FString::Printf(TEXT("%s %s"), *Count, *UFGItemDescriptor::GetItemName(Amount.ItemClass).ToString()));
		}
		return FString::Join(Parts, TEXT(" + "));
	};
	return FText::FromString(FString::Printf(TEXT("%s  ->  %s"),
		*Describe(UFGRecipe::GetIngredients(WorldContext, Recipe)), *Describe(UFGRecipe::GetProducts(Recipe))));
}

float AMRules::CleanValue(const FValueInfo& Info, float Value)
{
	if (!Info.bNeeded)
	{
		return 0.f;
	}
	if (Value <= 0.f && Info.Min > 0.f)
	{
		return Info.Default;
	}
	return FMath::Clamp(Value, Info.Min, Info.Max);
}

bool AMRules::IsConditionAvailable(EAMCondition Condition, const UObject* Machine)
{
	const bool bStorageEvent = (Condition >= EAMCondition::StorageFull && Condition <= EAMCondition::FillBelow)
		|| Condition == EAMCondition::BeltBlocked || Condition == EAMCondition::BeltFlowing;
	if (!Machine)
	{
		return true;
	}
	if (Machine->IsA<AFGBuildableManufacturer>())
	{
		return !bStorageEvent;
	}
	if (Machine->IsA<AFGBuildableStorage>())
	{
		return bStorageEvent;
	}
	if (Machine->IsA<AFGBuildableResourceExtractor>())
	{
		switch (Condition)
		{
		case EAMCondition::OutputFull:
		case EAMCondition::OutputNotFull:
		case EAMCondition::StartsProducing:
		case EAMCondition::StopsProducing:
		case EAMCondition::LosesPower:
		case EAMCondition::GetsPower:
		case EAMCondition::Paused:
		case EAMCondition::Resumed:
		case EAMCondition::ProductivityBelow:
		case EAMCondition::ProductivityAbove:
			return true;
		default:
			return false;
		}
	}
	return false;
}

bool AMRules::IsActionAvailable(EAMAction Action, const UObject* Machine)
{
	if (!Machine || IsWorldAction(Action))
	{
		return true;
	}
	if (Machine->IsA<AFGBuildableManufacturer>())
	{
		return Action != EAMAction::DeleteContents;
	}
	if (Machine->IsA<AFGBuildableStorage>())
	{
		return Action == EAMAction::DeleteContents;
	}
	if (Machine->IsA<AFGBuildableResourceExtractor>())
	{
		switch (Action)
		{
		case EAMAction::Pause:
		case EAMAction::Resume:
		case EAMAction::TogglePause:
		case EAMAction::SetClockSpeed:
		case EAMAction::DeleteOutput:
			return true;
		default:
			return false;
		}
	}
	return false;
}

bool AMRules::HasPassWrongItems(const UObject* Machine)
{
	return Machine && Machine->IsA<AFGBuildableManufacturer>();
}
