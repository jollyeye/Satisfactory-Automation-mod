#include "Automation/AMTypes.h"
#include "FGRecipe.h"
#include "ItemAmount.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePriorityPowerSwitch.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePipeReservoir.h"
#include "Buildables/FGBuildableResourceSink.h"
#include "Buildables/FGBuildableRailroadSignal.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "FGPowerConnectionComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	/**
	 * The building has an input port for belts (whether a belt is attached now or not). The HUB's
	 * built-in biomass burners have none: nothing can arrive on a belt there.
	 */
	bool AMTY_HasBeltInput(const UObject* Machine)
	{
		const AActor* Actor = Cast<AActor>(Machine);
		if (!Actor)
		{
			return false;
		}
		TInlineComponentArray<UFGFactoryConnectionComponent*> Connections(Actor);
		for (const UFGFactoryConnectionComponent* Connection : Connections)
		{
			if (Connection && Connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
			{
				return true;
			}
		}
		return false;
	}
}

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
	case EAMCondition::ItemCountAbove:    return AMTY_Text(TEXT("Item count above"));
	case EAMCondition::ItemCountBelow:    return AMTY_Text(TEXT("Item count below"));
	case EAMCondition::FuelEmpty:         return AMTY_Text(TEXT("Out of fuel"));
	case EAMCondition::FuelAvailable:     return AMTY_Text(TEXT("Has fuel"));
	case EAMCondition::FuelBelow:         return AMTY_Text(TEXT("Fuel below"));
	case EAMCondition::LoadAbove:         return AMTY_Text(TEXT("Load above"));
	case EAMCondition::LoadBelow:         return AMTY_Text(TEXT("Load below"));
	case EAMCondition::FuseBlown:         return AMTY_Text(TEXT("Fuse blown"));
	case EAMCondition::FuseReset:         return AMTY_Text(TEXT("Fuse reset"));
	case EAMCondition::MilestoneCompleted: return AMTY_Text(TEXT("Milestone completed"));
	case EAMCondition::ResearchCompleted: return AMTY_Text(TEXT("Research completed"));
	case EAMCondition::SwitchedOn:        return AMTY_Text(TEXT("Switched on"));
	case EAMCondition::SwitchedOff:       return AMTY_Text(TEXT("Switched off"));
	case EAMCondition::GridAUsesMore:     return AMTY_Text(TEXT("A usage > B usage"));
	case EAMCondition::GridBUsesMore:     return AMTY_Text(TEXT("A usage < B usage"));
	case EAMCondition::GridAMakesMore:    return AMTY_Text(TEXT("A power > B power"));
	case EAMCondition::GridBMakesMore:    return AMTY_Text(TEXT("A power < B power"));
	case EAMCondition::FlowAbove:         return AMTY_Text(TEXT("Flow above"));
	case EAMCondition::FlowBelow:         return AMTY_Text(TEXT("Flow below"));
	case EAMCondition::CouponsAbove:      return AMTY_Text(TEXT("Coupons above"));
	case EAMCondition::CouponsBelow:      return AMTY_Text(TEXT("Coupons below"));
	case EAMCondition::SignalRed:         return AMTY_Text(TEXT("Turns red"));
	case EAMCondition::SignalGreen:       return AMTY_Text(TEXT("Turns green"));
	case EAMCondition::TrainInBlock:      return AMTY_Text(TEXT("Train on track"));
	case EAMCondition::BlockEmpty:        return AMTY_Text(TEXT("Track empty"));
	case EAMCondition::TrainArrives:      return AMTY_Text(TEXT("Train arrives"));
	case EAMCondition::TrainLeaves:       return AMTY_Text(TEXT("Train leaves"));
	case EAMCondition::TransferDone:      return AMTY_Text(TEXT("Transfer done"));
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
	case EAMAction::SwitchOn:      return AMTY_Text(TEXT("Turn on"));
	case EAMAction::SwitchOff:     return AMTY_Text(TEXT("Turn off"));
	case EAMAction::ToggleSwitch:  return AMTY_Text(TEXT("Turn on or off"));
	case EAMAction::SetPriority:   return AMTY_Text(TEXT("Set priority"));
	case EAMAction::SetFlowLimit:  return AMTY_Text(TEXT("Set flow limit"));
	case EAMAction::FlushNetwork:  return AMTY_Text(TEXT("Flush pipe network"));
	case EAMAction::FlushBuffer:   return AMTY_Text(TEXT("Flush buffer"));
	case EAMAction::BlockSignal:   return AMTY_Text(TEXT("Block"));
	case EAMAction::UnblockSignal: return AMTY_Text(TEXT("Unblock"));
	case EAMAction::StationText:   return AMTY_Text(TEXT("Display text"));
	case EAMAction::StationName:   return AMTY_Text(TEXT("Display its name"));
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
		return AMTY_Text(TEXT("Rings when the container cannot take what a belt brings, so the belt stops: every slot holds a full stack, or the item's stacks are full and no slot is empty for it (in a container with several kinds of items).\n\nUse it to pause the machines that fill it."));
	case EAMCondition::StorageNotFull:
		return AMTY_Text(TEXT("Rings when the container can take what its belts bring again."));
	case EAMCondition::StorageEmpty:
		return AMTY_Text(TEXT("Rings when the last item has left the container.\n\nUse it to start the machines that fill it."));
	case EAMCondition::StorageNotEmpty:
		return AMTY_Text(TEXT("Rings when the first item arrives in an empty container."));
	case EAMCondition::FillAbove:
		return AMTY_Text(TEXT("Rings when the container is filled above the percentage you set. 100% is every slot holding a full stack; a slot with half a stack counts as half a slot.\n\nUse it to slow down or pause the machines that fill it before it is completely full."));
	case EAMCondition::FillBelow:
		return AMTY_Text(TEXT("Rings when the container is filled below the percentage you set (counted like Filled above).\n\nUse it to start the machines that fill it again before it runs empty."));
	case EAMCondition::ItemCountAbove:
		return AMTY_Text(TEXT("Rings when the container holds more of one item than the number you set. Pick the item under the alarm.\n\nMade for containers with several kinds of items: e.g. pause the Constructor making Iron Plates when there are more than 500."));
	case EAMCondition::ItemCountBelow:
		return AMTY_Text(TEXT("Rings when the container holds less of one item than the number you set. Pick the item under the alarm.\n\nE.g. resume the Constructor making Iron Plates when there are fewer than 200."));
	case EAMCondition::FuelEmpty:
		return AMTY_Text(TEXT("Rings when the generator has burned its last fuel: no fuel in its slot and nothing burning, so it stops making power.\n\nUse it to get a message, or to start a backup generator."));
	case EAMCondition::FuelAvailable:
		return AMTY_Text(TEXT("Rings when the generator has fuel again."));
	case EAMCondition::FuelBelow:
		return AMTY_Text(TEXT("Rings when the generator's fuel slot holds fewer items than the number you set: it is running low.\n\nUse it to start the machines that bring its fuel, or to get a warning before it runs out."));
	case EAMCondition::LoadAbove:
		return AMTY_Text(TEXT("Rings when the generator's load rises above the percentage you set: how much of what it can make the power grid is using.\n\nUse it to start more generators before the grid runs out of power and the fuse blows."));
	case EAMCondition::LoadBelow:
		return AMTY_Text(TEXT("Rings when the generator's load drops below the percentage you set: the grid needs less power.\n\nUse it to pause extra generators and save fuel."));
	case EAMCondition::FuseBlown:
		return AMTY_Text(TEXT("Rings when the fuse of the generator's power grid blows: the machines used more power than the generators make, so the whole grid is off until someone resets the fuse."));
	case EAMCondition::FuseReset:
		return AMTY_Text(TEXT("Rings when the fuse of the generator's power grid is reset and the grid is on again."));
	case EAMCondition::MilestoneCompleted:
		return AMTY_Text(TEXT("Rings when a milestone is completed in the HUB (any milestone, by any player).\n\nUse it to show a message, play a sound, or start machines waiting for the next tier."));
	case EAMCondition::ResearchCompleted:
		return AMTY_Text(TEXT("Rings when a MAM research is completed (any research, also hard drives): its results can be claimed.\n\nUse it to get a message or a sound while you are away from the MAM."));
	case EAMCondition::SwitchedOn:
		return AMTY_Text(TEXT("Rings when the switch is turned on: by a player, or by an action of an alarm. The two power grids it joins are one again."));
	case EAMCondition::SwitchedOff:
		return AMTY_Text(TEXT("Rings when the switch is turned off: by a player, by an action of an alarm, or (a priority switch) by the game during a power shortage.\n\nUse it to get a message when a priority switch cuts off part of the factory."));
	case EAMCondition::GridAUsesMore:
		return AMTY_Text(TEXT("Rings when the machines of Power Grid A (the left graph in the switch's window) use more power than those of Power Grid B.\n\nPair it with \"A usage < B usage\", which rings when it turns the other way."));
	case EAMCondition::GridBUsesMore:
		return AMTY_Text(TEXT("Rings when the machines of Power Grid A use less power than those of Power Grid B (the right graph in the switch's window)."));
	case EAMCondition::GridAMakesMore:
		return AMTY_Text(TEXT("Rings when the generators of Power Grid A can make more power than those of Power Grid B: their capacity (the top line of the graph), which counts only generators that have fuel.\n\nPair it with \"A power < B power\", which rings when it turns the other way."));
	case EAMCondition::GridBMakesMore:
		return AMTY_Text(TEXT("Rings when the generators of Power Grid A can make less power than those of Power Grid B (their capacity, generators with fuel only)."));
	case EAMCondition::TrainArrives:
		return AMTY_Text(TEXT("Rings when a train docks at the station."));
	case EAMCondition::TrainLeaves:
		return AMTY_Text(TEXT("Rings when the docked train leaves the station."));
	case EAMCondition::TransferDone:
		return AMTY_Text(TEXT("Rings when the docked train has finished loading / unloading at the station's platforms."));
	case EAMCondition::SignalRed:
		return AMTY_Text(TEXT("Rings when the signal turns red: trains must stop at it (its block is taken, or it is blocked)."));
	case EAMCondition::SignalGreen:
		return AMTY_Text(TEXT("Rings when the signal turns green: trains may pass."));
	case EAMCondition::TrainInBlock:
		return AMTY_Text(TEXT("Rings when a train enters the track this signal guards (after it, up to the next signals)."));
	case EAMCondition::BlockEmpty:
		return AMTY_Text(TEXT("Rings when the last train leaves the track this signal guards."));
	case EAMCondition::CouponsAbove:
		return AMTY_Text(TEXT("Rings when there are more coupons than this to print (the coupons the AWESOME Sinks have earned and nobody has printed yet; the same number at every sink).\n\nUse it to get a message when it is time to go shopping."));
	case EAMCondition::CouponsBelow:
		return AMTY_Text(TEXT("Rings when there are fewer coupons than this to print (for example after printing them)."));
	case EAMCondition::FlowAbove:
		return AMTY_Text(TEXT("Rings when more fluid than this flows through it (m3 per minute, like its window's flow)."));
	case EAMCondition::FlowBelow:
		return AMTY_Text(TEXT("Rings when less fluid than this flows through it (m3 per minute). 0: rings when nothing flows any more."));
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
	case EAMAction::SwitchOn:
		return AMTY_Text(TEXT("Turns the switch on, like its own button: the power grids on its two sides are joined. Does nothing if it is already on."));
	case EAMAction::SwitchOff:
		return AMTY_Text(TEXT("Turns the switch off, like its own button: the power grid behind it is cut off. Does nothing if it is already off."));
	case EAMAction::ToggleSwitch:
		return AMTY_Text(TEXT("Turns the switch off if it is on, on if it is off. Handy for one alarm that switches something on and off."));
	case EAMAction::SetFlowLimit:
		return AMTY_Text(TEXT("Sets the flow limit, like the slider in its window (m3 per minute). 0 closes it: nothing flows through."));
	case EAMAction::FlushNetwork:
		return AMTY_Text(TEXT("Empties the pipe network, like the Flush button of a pipe: all the fluid in it is destroyed and the network forgets its fluid.\n\nValves and buffers do not split a network: this empties every pipe, pump, valve and buffer connected to it."));
	case EAMAction::StationText:
		return AMTY_Text(TEXT("The station shows this text instead of its name (its sign, the map, the timetables), until an alarm displays its name again. Trains still go there: their timetables point at the station, not at its name.\n\nSaves always keep the station's own name."));
	case EAMAction::StationName:
		return AMTY_Text(TEXT("The station shows its own name again."));
	case EAMAction::BlockSignal:
		return AMTY_Text(TEXT("On: the signal stays red when the alarm rings: trains arriving at it wait, until an alarm unblocks it (off). A train already past the signal is not stopped."));
	case EAMAction::UnblockSignal:
		return AMTY_Text(TEXT("Off: the signal works normally again when the alarm rings."));
	case EAMAction::FlushBuffer:
		return AMTY_Text(TEXT("Empties this fluid buffer only, like its own Flush button: its fluid is destroyed. The pipes around it keep theirs."));
	case EAMAction::SetPriority:
		return AMTY_Text(TEXT("Sets the priority of the switch, like the priority in its window. During a power shortage the game turns off the switches with the highest number first; 0 means the game never turns this one off."));
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
	else if (Condition == EAMCondition::LoadAbove || Condition == EAMCondition::LoadBelow)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("Load");
		Info.Min = 0.f;
		Info.Max = 100.f;
		Info.Default = Condition == EAMCondition::LoadAbove ? 90.f : 20.f;
	}
	else if (Condition == EAMCondition::FlowAbove || Condition == EAMCondition::FlowBelow)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("Flow");
		Info.Min = 0.f;
		Info.Max = 600.f;
		Info.Default = Condition == EAMCondition::FlowAbove ? 300.f : 1.f;
		Info.Unit = TEXT("m3/min");
	}
	else if (Condition == EAMCondition::CouponsAbove || Condition == EAMCondition::CouponsBelow)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("Coupons");
		Info.Min = 0.f;
		Info.Max = 999999.f;
		Info.Default = Condition == EAMCondition::CouponsAbove ? 10.f : 1.f;
		Info.Unit = TEXT("");
	}
	else if (Condition == EAMCondition::FuelBelow)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("#");
		Info.Min = 0.f;
		Info.Max = 999999.f;
		Info.Default = 20.f;
		Info.Unit = TEXT("");
	}
	else if (Condition == EAMCondition::ItemCountAbove || Condition == EAMCondition::ItemCountBelow)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("#");
		Info.Min = 0.f;
		Info.Max = 999999.f;
		Info.Default = Condition == EAMCondition::ItemCountAbove ? 500.f : 200.f;
		Info.Unit = TEXT("");
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
	else if (Action == EAMAction::SetFlowLimit)
	{
		Info.bNeeded = true;
		Info.Label = TEXT("Flow limit");
		Info.Min = 0.f;
		Info.Max = 600.f;
		Info.Default = 300.f;
		Info.Unit = TEXT("m3/min");
	}
	else if (Action == EAMAction::SetPriority)
	{
		// The game's priorities: 1 to 8, 0 = never turned off by the game.
		Info.bNeeded = true;
		Info.Label = TEXT("Priority");
		Info.Min = 0.f;
		Info.Max = 8.f;
		Info.Default = 1.f;
		Info.Unit = TEXT("");
	}
	return Info;
}

bool AMRules::ActionNeedsRecipe(EAMAction Action)
{
	return Action == EAMAction::ChangeRecipe;
}

bool AMRules::ActionNeedsMessage(EAMAction Action)
{
	return Action == EAMAction::ShowMessage || Action == EAMAction::StationText;
}

bool AMRules::IsWorldAction(EAMAction Action)
{
	return Action == EAMAction::ShowMessage || Action == EAMAction::PlaySound;
}

bool AMRules::IsWorldCondition(EAMCondition Condition)
{
	return Condition == EAMCondition::MilestoneCompleted || Condition == EAMCondition::ResearchCompleted;
}

bool AMRules::IsMamCondition(EAMCondition Condition)
{
	return Condition == EAMCondition::ResearchCompleted;
}

bool AMRules::IsSwitchCondition(EAMCondition Condition)
{
	return Condition == EAMCondition::SwitchedOn || Condition == EAMCondition::SwitchedOff
		|| (Condition >= EAMCondition::GridAUsesMore && Condition <= EAMCondition::GridBMakesMore);
}

bool AMRules::ActionsConflict(EAMAction A, EAMAction B)
{
	auto Family = [](EAMAction Action) -> int32
	{
		switch (Action)
		{
		case EAMAction::Pause:
		case EAMAction::Resume:
		case EAMAction::TogglePause:  return 1;
		case EAMAction::SwitchOn:
		case EAMAction::SwitchOff:
		case EAMAction::ToggleSwitch: return 2;
		case EAMAction::BlockSignal:
		case EAMAction::UnblockSignal: return 3;
		case EAMAction::StationText:
		case EAMAction::StationName:   return 4;
		default:                      return 100 + static_cast<int32>(Action);
		}
	};
	return Family(A) == Family(B);
}

bool AMRules::IsSignalCondition(EAMCondition Condition)
{
	return Condition == EAMCondition::SignalRed || Condition == EAMCondition::SignalGreen
		|| Condition == EAMCondition::TrainInBlock || Condition == EAMCondition::BlockEmpty;
}

bool AMRules::IsSignalAction(EAMAction Action)
{
	return Action == EAMAction::BlockSignal || Action == EAMAction::UnblockSignal;
}

bool AMRules::IsValve(const UObject* Machine)
{
	const AActor* Actor = Cast<AActor>(Machine);
	return Actor && Actor->IsA<AFGBuildablePipelinePump>() && !Actor->FindComponentByClass<UFGPowerConnectionComponent>();
}

bool AMRules::IsSwitchAction(EAMAction Action)
{
	return Action == EAMAction::SwitchOn || Action == EAMAction::SwitchOff || Action == EAMAction::ToggleSwitch || Action == EAMAction::SetPriority;
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
	// World events belong to the HUB, never to a machine.
	if (IsWorldCondition(Condition))
	{
		return false;
	}
	// Wrong input / Input correct watch what arrives on an input belt: not for a building without
	// an input port (the HUB's biomass burners).
	if (Machine && (Condition == EAMCondition::WrongInput || Condition == EAMCondition::InputCorrect) && !AMTY_HasBeltInput(Machine))
	{
		return false;
	}
	const bool bStorageEvent = Condition >= EAMCondition::StorageFull && Condition <= EAMCondition::ItemCountBelow;
	const bool bGeneratorEvent = Condition >= EAMCondition::FuelEmpty && Condition <= EAMCondition::FuseReset;
	const bool bSwitchEvent = IsSwitchCondition(Condition);
	if (!Machine)
	{
		return true;
	}
	if (Machine->IsA<AFGBuildableCircuitSwitch>())
	{
		return bSwitchEvent;
	}
	if (Machine->IsA<AFGBuildablePipelinePump>())
	{
		// Flow; a pump (it has power) also power. No standby: their windows have no standby button.
		const AActor* PumpActor = Cast<AActor>(Machine);
		const bool bPowered = PumpActor && PumpActor->FindComponentByClass<UFGPowerConnectionComponent>() != nullptr;
		switch (Condition)
		{
		case EAMCondition::FlowAbove:
		case EAMCondition::FlowBelow:
			return true;
		case EAMCondition::LosesPower:
		case EAMCondition::GetsPower:
			return bPowered;
		default:
			return false;
		}
	}
	if (Machine->IsA<AFGBuildableRailroadSignal>())
	{
		return IsSignalCondition(Condition);
	}
	const bool bStationEvent = Condition == EAMCondition::TrainArrives || Condition == EAMCondition::TrainLeaves || Condition == EAMCondition::TransferDone;
	if (Machine->IsA<AFGBuildableRailroadStation>())
	{
		return bStationEvent;
	}
	if (bStationEvent)
	{
		return false;
	}
	if (IsSignalCondition(Condition))
	{
		return false;
	}
	if (Machine->IsA<AFGBuildableResourceSink>())
	{
		// Only the coupons (a world number), like the MAM's alarms.
		return Condition == EAMCondition::CouponsAbove || Condition == EAMCondition::CouponsBelow;
	}
	if (Condition == EAMCondition::FlowAbove || Condition == EAMCondition::FlowBelow
		|| Condition == EAMCondition::CouponsAbove || Condition == EAMCondition::CouponsBelow)
	{
		return false;
	}
	if (Machine->IsA<AFGBuildableManufacturer>())
	{
		return !bStorageEvent && !bGeneratorEvent && !bSwitchEvent;
	}
	if (Machine->IsA<AFGBuildableStorage>() || Machine->IsA<AFGBuildablePipeReservoir>())
	{
		// Fluid buffers: like containers (full, empty, filled above / below), without item counts.
		if (Machine->IsA<AFGBuildablePipeReservoir>() && (Condition == EAMCondition::ItemCountAbove || Condition == EAMCondition::ItemCountBelow))
		{
			return false;
		}
		return bStorageEvent;
	}
	if (Machine->IsA<AFGBuildableGeneratorFuel>())
	{
		switch (Condition)
		{
		case EAMCondition::StartsProducing:
		case EAMCondition::StopsProducing:
		case EAMCondition::Paused:
		case EAMCondition::Resumed:
		case EAMCondition::InputFull:
		case EAMCondition::InputNotFull:
		case EAMCondition::WrongInput:
		case EAMCondition::InputCorrect:
		case EAMCondition::FuelEmpty:
		case EAMCondition::FuelAvailable:
		case EAMCondition::FuelBelow:
		case EAMCondition::LoadAbove:
		case EAMCondition::LoadBelow:
		case EAMCondition::FuseBlown:
		case EAMCondition::FuseReset:
			return true;
		default:
			return false;
		}
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
	if (Machine->IsA<AFGBuildablePipelinePump>())
	{
		// Only what the valve's window has: the flow limit; and flushing its network.
		return Action == EAMAction::SetFlowLimit || Action == EAMAction::FlushNetwork;
	}
	if (Machine->IsA<AFGBuildablePipeReservoir>())
	{
		return Action == EAMAction::FlushBuffer || Action == EAMAction::FlushNetwork;
	}
	if (Machine->IsA<AFGBuildableRailroadSignal>())
	{
		return IsSignalAction(Action);
	}
	const bool bStationAction = Action == EAMAction::StationText || Action == EAMAction::StationName;
	if (Machine->IsA<AFGBuildableRailroadStation>())
	{
		return bStationAction || Action == EAMAction::Pause || Action == EAMAction::Resume || Action == EAMAction::TogglePause;
	}
	if (bStationAction)
	{
		return false;
	}
	if (IsSignalAction(Action))
	{
		return false;
	}
	if (Machine->IsA<AFGBuildableResourceSink>())
	{
		// Its only switch: on / standby.
		return Action == EAMAction::Pause || Action == EAMAction::Resume || Action == EAMAction::TogglePause;
	}
	if (Action == EAMAction::SetFlowLimit || Action == EAMAction::FlushNetwork || Action == EAMAction::FlushBuffer)
	{
		return false;
	}
	if (Machine->IsA<AFGBuildableCircuitSwitch>())
	{
		// Set priority: priority switches only.
		return IsSwitchAction(Action) && (Action != EAMAction::SetPriority || Machine->IsA<AFGBuildablePriorityPowerSwitch>());
	}
	if (Machine->IsA<AFGBuildableManufacturer>())
	{
		return Action != EAMAction::DeleteContents && !IsSwitchAction(Action);
	}
	if (Machine->IsA<AFGBuildableStorage>())
	{
		return Action == EAMAction::DeleteContents;
	}
	if (Machine->IsA<AFGBuildableGeneratorFuel>())
	{
		switch (Action)
		{
		case EAMAction::Pause:
		case EAMAction::Resume:
		case EAMAction::TogglePause:
		case EAMAction::SetClockSpeed:
		case EAMAction::DeleteInput:
			return true;
		default:
			return false;
		}
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

bool AMRules::ConditionNeedsItem(EAMCondition Condition)
{
	return Condition == EAMCondition::ItemCountAbove || Condition == EAMCondition::ItemCountBelow;
}
