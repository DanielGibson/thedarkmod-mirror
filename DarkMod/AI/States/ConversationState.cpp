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

#include "ConversationState.h"
#include "../Memory.h"
#include "../Tasks/IdleAnimationTask.h"
#include "../Tasks/MoveToPositionTask.h"
#include "ObservantState.h"
#include "../Library.h"
#include "../Conversation/ConversationCommand.h"

// greebo: This spawnarg holds the currently played conversation sound
#define CONVERSATION_SPAWNARG "snd_TEMP_conv"

namespace ai
{

// Get the name of this state
const idStr& ConversationState::GetName() const
{
	static idStr _name(STATE_CONVERSATION);
	return _name;
}

bool ConversationState::CheckAlertLevel(idAI* owner)
{
	if (owner->AI_AlertIndex > 0)
	{
		// Alert index is too high, switch to the higher State
		owner->GetMind()->PushState(STATE_OBSERVANT);
		return false;
	}

	// Alert Index is matching, return OK
	return true;
}

void ConversationState::SetConversation(int index)
{
	// TODO: Sanity-Check

	_conversation = index;
}

void ConversationState::Init(idAI* owner)
{
	// Init base class first
	State::Init(owner);

	DM_LOG(LC_AI, LT_INFO)LOGSTRING("ConversationState initialised.\r");
	assert(owner);

	// Memory shortcut
	Memory& memory = owner->GetMemory();
	memory.alertClass = EAlertNone;
	memory.alertType = EAlertTypeNone;

	_alertLevelDecreaseRate = 0.01f;

	// Ensure we are in the correct alert level
	if (!CheckAlertLevel(owner)) return;

	// Check dialogue prerequisites
	if (!CheckConversationPrerequisites())
	{
		owner->mind->EndState();
		return;
	}

	// We haven't started doing our stuff yet
	_finishTime = -1;
	_commandType = ConversationCommand::ENumCommands;
	_state = ConversationCommand::ENotStartedYet;

	owner->GetSubsystem(SubsysAction)->ClearTasks();
	owner->GetSubsystem(SubsysSenses)->ClearTasks();
	owner->GetSubsystem(SubsysCommunication)->ClearTasks();
}

// Gets called each time the mind is thinking
void ConversationState::Think(idAI* owner)
{
	Memory& memory = owner->GetMemory();

	UpdateAlertLevel();

	// Ensure we are in the correct alert level
	if (!CheckAlertLevel(owner)) return;

	// Let the AI check its senses
	owner->PerformVisualScan();

	if (_finishTime > 0 && gameLocal.time > _finishTime)
	{
		_state = ConversationCommand::EFinished;
	}

	DrawDebugOutput(owner);
}

ConversationCommand::State ConversationState::GetExecutionState()
{
	return _state;
}

bool ConversationState::CheckConversationPrerequisites()
{
	// TODO
	return true;
}

void ConversationState::OnSubsystemTaskFinished(idAI* owner, SubsystemId subSystem)
{
	if (_state != ConversationCommand::EExecuting) return;

	if (subSystem == SubsysMovement)
	{
		// In case of active "walk" commands, set the state to "finished"
		if (_commandType == ConversationCommand::EWalkToEntity || _commandType == ConversationCommand::EWalkToPosition)
		{
			_state = ConversationCommand::EFinished;
		}
	}
}

void ConversationState::StartCommand(ConversationCommand& command)
{
	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	switch (command.GetType())
	{
	case ConversationCommand::EWaitSeconds:
		if (!idStr::IsNumeric(command.GetArgument(0)))
		{
			gameLocal.Warning("Conversation Command argument for 'WaitSeconds' is not numeric: %s", command.GetArgument(0).c_str());
		}
		_finishTime = gameLocal.time + SEC2MS(atof(command.GetArgument(0)));
		_state = ConversationCommand::EExecuting;
	break;
	/*
	EWaitForTrigger,
	EWaitForActor,
	EWalkToPosition*/
	case ConversationCommand::EWalkToEntity:
	{
		idEntity* ent = command.GetEntityArgument(0);
		if (ent != NULL)
		{
			// Start moving
			idVec3 distance = ent->GetPhysics()->GetOrigin() - owner->GetPhysics()->GetOrigin();
			idVec3 distanceNorm(distance);
			distanceNorm.NormalizeFast();

			idVec3 goal = owner->GetPhysics()->GetOrigin() + distance - distanceNorm*100.0f;

			owner->GetSubsystem(SubsysMovement)->PushTask(
				TaskPtr(new MoveToPositionTask(goal))
			);
			//owner->MoveToPosition(ent->GetPhysics()->GetOrigin());
			_state = ConversationCommand::EExecuting;
		}
		else
		{
			gameLocal.Warning("Conversation Command: 'WalkToEntity' could not find entity: %s", command.GetArgument(0).c_str());
		}
	}
	break;
	/*EWalkToEntity,
	EStopMove,
	ETalk,
	EPlayAnimOnce,
	EPlayAnimCycle,
	EActivateTarget,
	ELookAtActor,
	ELookAtPosition,
	ELookAtEntity,
	ETurnToActor,
	ETurnToPosition,
	ETurnToEntity,
	EAttackActor,
	EAttackEntity,
	*/
	case ConversationCommand::ETalk:
	{
		int length = Talk(owner, command.GetArgument(0));

		// Set the finish conditions for the current action
		_state = ConversationCommand::EExecuting;
		_finishTime = gameLocal.time + length + 200;
	}
	break;
	default:
		gameLocal.Warning("Unknown command type found %d", command.GetType());
		DM_LOG(LC_CONVERSATION, LT_ERROR)LOGSTRING("Unknown command type found %d", command.GetType());
		_state = ConversationCommand::EAborted;
	};

	// Store the command type
	_commandType = command.GetType();
}

void ConversationState::Execute(ConversationCommand& command)
{

}

int ConversationState::Talk(idAI* owner, const idStr& soundName)
{
	const idKeyValue* kv = owner->spawnArgs.FindKey(soundName);

	if (kv != NULL && kv->GetValue().Icmpn( "snd_", 4 ) == 0)
	{
		// The conversation argument is pointing to a valid spawnarg on the owner
		owner->spawnArgs.Set(CONVERSATION_SPAWNARG, kv->GetValue());
	}
	else
	{
		// The spawnargs don't define the sound shader, set the shader directly
		owner->spawnArgs.Set(CONVERSATION_SPAWNARG, soundName);
	}

	// Start the sound
	int length = owner->PlayAndLipSync(CONVERSATION_SPAWNARG, "talk1");

	// Clear the spawnarg again
	owner->spawnArgs.Set(CONVERSATION_SPAWNARG, "");

	return length;
}

void ConversationState::DrawDebugOutput(idAI* owner)
{
	if (!cv_ai_show_conversationstate.GetBool()) return;

	idStr str;

	switch (_state)
	{
		case ConversationCommand::ENotStartedYet: str = "Not Started Yet"; break;
		case ConversationCommand::EExecuting: str = "Executing"; break;
		case ConversationCommand::EFinished: str = "Finished"; break;
		case ConversationCommand::EAborted: str = "Aborted"; break;
		default:break;
	};

	gameRenderWorld->DrawText(str, owner->GetEyePosition() - idVec3(0,0,10), 0.3f, colorCyan, gameLocal.GetLocalPlayer()->viewAxis, 1, 48);
}

void ConversationState::Save(idSaveGame* savefile) const
{
	State::Save(savefile);

	savefile->WriteInt(_conversation);
	savefile->WriteInt(static_cast<int>(_state));
	savefile->WriteInt(static_cast<int>(_commandType));
	savefile->WriteInt(_finishTime);
}

void ConversationState::Restore(idRestoreGame* savefile)
{
	State::Restore(savefile);

	savefile->ReadInt(_conversation);

	int temp;
	savefile->ReadInt(temp);
	assert(temp >= 0 && temp <= ConversationCommand::ENumStates); // sanity check
	_state = static_cast<ConversationCommand::State>(temp);

	savefile->ReadInt(temp);
	assert(temp >= 0 && temp <= ConversationCommand::ENumCommands); // sanity check
	_commandType = static_cast<ConversationCommand::Type>(temp);

	savefile->ReadInt(_finishTime);
}

StatePtr ConversationState::CreateInstance()
{
	return StatePtr(new ConversationState);
}

// Register this state with the StateLibrary
StateLibrary::Registrar conversationStateRegistrar(
	STATE_CONVERSATION, // Task Name
	StateLibrary::CreateInstanceFunc(&ConversationState::CreateInstance) // Instance creation callback
);

} // namespace ai
