#include "Automation/AMSignal.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildableRailroadSignal.h"
#include "FGRailroadSignalBlock.h"
#include "Patching/NativeHookManager.h"

namespace
{
	FCriticalSection AMSG_Lock;
	TSet<const AFGBuildableRailroadSignal*> AMSG_Blocked;
	std::atomic<int32> AMSG_BlockedCount { 0 };

	bool AMSG_IsBlocked(const AFGBuildableRailroadSignal* Signal)
	{
		if (!Signal || AMSG_BlockedCount.load() == 0)
		{
			return false;
		}
		FScopeLock Lock(&AMSG_Lock);
		return AMSG_Blocked.Contains(Signal);
	}

	/** The reservation enters the block through a blocked signal. */
	bool AMSG_EntersBlocked(const FFGRailroadBlockReservation* Reservation)
	{
		return Reservation && AMSG_IsBlocked(Reservation->EntrySignal.Get());
	}

	/** The signal and the others of its block show their aspect again (ours or the game's). */
	void AMSG_Redraw(AFGBuildableRailroadSignal* Signal)
	{
		if (const TSharedPtr<FFGRailroadSignalBlock> Block = Signal ? Signal->GetObservedBlock().Pin() : nullptr)
		{
			Block->OnBlockChanged.Broadcast();
		}
	}
}

void AMSignal::InstallHooks()
{
	static bool bInstalled = false;
	if (bInstalled)
	{
		return;
	}
	bInstalled = true;
	SUBSCRIBE_METHOD(FFGRailroadSignalBlock::HasOverlappingTrains,
		[](auto& Scope, const FFGRailroadSignalBlock* Self, FFGRailroadBlockReservation* Reservation)
		{
			if (AMSG_EntersBlocked(Reservation))
			{
				Scope.Override(true);
			}
		});
	SUBSCRIBE_METHOD(FFGRailroadSignalBlock::HasOverlappingApprovedReservations,
		[](auto& Scope, const FFGRailroadSignalBlock* Self, FFGRailroadBlockReservation* Reservation)
		{
			if (AMSG_EntersBlocked(Reservation))
			{
				Scope.Override(true);
			}
		});
	SUBSCRIBE_METHOD(FFGRailroadSignalBlock::GetAspectFor,
		[](auto& Scope, const FFGRailroadSignalBlock* Self, AFGBuildableRailroadSignal* Signal)
		{
			if (AMSG_IsBlocked(Signal))
			{
				Scope.Override(ERailroadSignalAspect::RSA_Stop);
			}
		});
	UE_LOG(LogAutomatedMachines, Log, TEXT("Railway signal hooks installed"));
}

void AMSignal::SetBlocked(AFGBuildableRailroadSignal* Signal, bool bBlocked)
{
	if (!Signal)
	{
		return;
	}
	bool bChanged = false;
	{
		FScopeLock Lock(&AMSG_Lock);
		bChanged = bBlocked ? !AMSG_Blocked.Contains(Signal) : AMSG_Blocked.Contains(Signal);
		if (bBlocked)
		{
			AMSG_Blocked.Add(Signal);
		}
		else
		{
			AMSG_Blocked.Remove(Signal);
		}
		AMSG_BlockedCount = AMSG_Blocked.Num();
	}
	if (bChanged)
	{
		AMSG_Redraw(Signal);
	}
}

bool AMSignal::IsBlocked(const AFGBuildableRailroadSignal* Signal)
{
	return AMSG_IsBlocked(Signal);
}

bool AMSignal::IsRed(const AFGBuildableRailroadSignal* Signal)
{
	return Signal && Signal->GetAspect() == ERailroadSignalAspect::RSA_Stop;
}

bool AMSignal::IsGreen(const AFGBuildableRailroadSignal* Signal)
{
	return Signal && (Signal->GetAspect() == ERailroadSignalAspect::RSA_Clear || Signal->GetAspect() == ERailroadSignalAspect::RSA_Dock);
}

bool AMSignal::IsBlockOccupied(AFGBuildableRailroadSignal* Signal)
{
	const TSharedPtr<FFGRailroadSignalBlock> Block = Signal ? Signal->GetObservedBlock().Pin() : nullptr;
	return Block.IsValid() && Block->IsOccupied();
}
