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

#include "ObservantState.h"
#include "../Memory.h"
#include "../../AIComm_Message.h"
#include "../Tasks/RandomHeadturnTask.h"
#include "../Tasks/SingleBarkTask.h"
#include "../Tasks/WaitTask.h"
#include "SuspiciousState.h"
#include "../Library.h"

namespace ai
{

// Get the name of this state
const idStr& ObservantState::GetName() const
{
	static idStr _name(STATE_OBSERVANT);
	return _name;
}

bool ObservantState::CheckAlertLevel(idAI* owner)
{
	if (owner->AI_AlertIndex < 1)
	{
		// Alert index is too low for this state, fall back
		owner->GetMind()->EndState();
		return false;
	}
	else if (owner->AI_AlertIndex > 1)
	{
		// Alert index is too high, switch to the higher State
		owner->GetMind()->PushState(STATE_SUSPICIOUS);
		return false;
	}

	// Alert Index is matching, return OK
	return true;
}

void ObservantState::Init(idAI* owner)
{
	// Init base class first
	State::Init(owner);

	DM_LOG(LC_AI, LT_INFO)LOGSTRING("ObservantState initialised.\r");
	assert(owner);

	// Ensure we are in the correct alert level
	if (!CheckAlertLevel(owner)) return;

	float alertTime = owner->atime1 + owner->atime1_fuzzyness * (gameLocal.random.RandomFloat() - 0.5);
	_alertLevelDecreaseRate = (owner->thresh_2 - owner->thresh_1) / alertTime;

	// Shortcut reference
	Memory& memory = owner->GetMemory();

	// Stop playing idle animation
	owner->actionSubsystem->ClearTasks();

	// barking
	idStr soundName("");

	if (owner->AlertIndexIncreased() && memory.alertType != EAlertTypeMissingItem)
	{
		if (memory.alertClass == EAlertVisual)
		{
			soundName = "snd_alert1s";
		}
		else if (memory.alertClass == EAlertAudio)
		{
			soundName = "snd_alert1h";
		}
		else
		{
			soundName = "snd_alert1";
		}
	}
	else if (owner->HasSeenEvidence())
	{
		if (owner->m_lastAlertLevel >= owner->thresh_3)
		{
			soundName = "snd_alertdown0SeenEvidence";
		}
	}
	else if (owner->m_lastAlertLevel >= owner->thresh_4)
	{
		soundName = "snd_alertdown0SeenNoEvidence";
	}

	if (memory.alertType != EAlertTypeMissingItem)
	{
		owner->commSubsystem->AddCommTask(
				CommunicationTaskPtr(new SingleBarkTask(soundName))
		);
	}

	// Let the AI update their weapons (make them nonsolid)
	owner->UpdateAttachmentContents(false);
}

// Gets called each time the mind is thinking
void ObservantState::Think(idAI* owner)
{
	UpdateAlertLevel();
	// Ensure we are in the correct alert level
	if (!CheckAlertLevel(owner)) return;
	
	if (owner->GetMoveType() != MOVETYPE_SLEEP)
	{
		// Let the AI check its senses
		owner->PerformVisualScan();
	}
}

StatePtr ObservantState::CreateInstance()
{
	return StatePtr(new ObservantState);
}

// Register this state with the StateLibrary
StateLibrary::Registrar observantStateRegistrar(
	STATE_OBSERVANT, // Task Name
	StateLibrary::CreateInstanceFunc(&ObservantState::CreateInstance) // Instance creation callback
);

} // namespace ai
