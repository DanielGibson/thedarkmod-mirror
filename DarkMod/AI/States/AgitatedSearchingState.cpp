/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id$", init_version);

#include "AgitatedSearchingState.h"
#include "../Memory.h"
#include "../Tasks/InvestigateSpotTask.h"
#include "../Tasks/SingleBarkTask.h"
#include "../Tasks/RepeatedBarkTask.h"
#include "../Tasks/WaitTask.h"
#include "CombatState.h"
#include "../Library.h"
#include "../../AbsenceMarker.h"
#include "../../AIComm_Message.h"

namespace ai
{

// Get the name of this state
const idStr& AgitatedSearchingState::GetName() const
{
	static idStr _name(STATE_AGITATED_SEARCHING);
	return _name;
}

bool AgitatedSearchingState::CheckAlertLevel(idAI* owner)
{
	if (owner->AI_AlertIndex < 4)
	{
		// Alert index is too low for this state, fall back
		owner->GetMind()->EndState();
		return false;
	}
	else if (owner->AI_AlertIndex > 4)
	{
		// Alert index is too high, switch to the higher State
		owner->Event_CloseHidingSpotSearch();
		owner->GetMind()->PushState(STATE_COMBAT);
		return false;
	}

	// Alert Index is matching, return OK
	return true;
}

void AgitatedSearchingState::Init(idAI* owner)
{
	// Init base class first (note: we're not calling SearchingState::Init() on purpose here)
	State::Init(owner);

	DM_LOG(LC_AI, LT_INFO)LOGSTRING("AgitatedSearchingState initialised.\r");
	assert(owner);

	// Ensure we are in the correct alert level
	if (!CheckAlertLevel(owner)) return;

	// Shortcut reference
	Memory& memory = owner->GetMemory();

	float alertTime = owner->atime4 + owner->atime4_fuzzyness * (gameLocal.random.RandomFloat() - 0.5);
	_alertLevelDecreaseRate = (owner->thresh_5 - owner->thresh_4) / alertTime;

	// Setup a new hiding spot search
	StartNewHidingSpotSearch(owner);

	if (owner->AlertIndexIncreased())
	{
		if (memory.alertType == EAlertTypeSuspicious || memory.alertType == EAlertTypeEnemy)
		{
			owner->commSubsystem->AddCommTask(
				CommunicationTaskPtr(new SingleBarkTask("snd_alert4"))
			);
		}
	}

	owner->commSubsystem->AddSilence(5000);

	int minTime = SEC2MS(owner->spawnArgs.GetFloat("searchbark_delay_min", "10"));
	int maxTime = SEC2MS(owner->spawnArgs.GetFloat("searchbark_delay_max", "15"));

	if (owner->HasSeenEvidence())
	{
		owner->commSubsystem->AddCommTask(
			CommunicationTaskPtr(new RepeatedBarkTask("snd_state4SeenEvidence", minTime, maxTime))
		);
	}
	else
	{
		owner->commSubsystem->AddCommTask(
			CommunicationTaskPtr(new RepeatedBarkTask("snd_state4SeenNoEvidence", minTime, maxTime))
		);
	}
	
	owner->DrawWeapon();

	// Let the AI update their weapons (make them solid)
	owner->UpdateAttachmentContents(true);
}

// Gets called each time the mind is thinking
void AgitatedSearchingState::Think(idAI* owner)
{
	SearchingState::Think(owner);
}

StatePtr AgitatedSearchingState::CreateInstance()
{
	return StatePtr(static_cast<State*>(new AgitatedSearchingState));
}

// Register this state with the StateLibrary
StateLibrary::Registrar agitatedSearchingStateRegistrar(
	STATE_AGITATED_SEARCHING, // Task Name
	StateLibrary::CreateInstanceFunc(&AgitatedSearchingState::CreateInstance) // Instance creation callback
);

} // namespace ai
