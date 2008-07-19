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

#include "../Memory.h"
#include "MoveToPositionTask.h"
#include "../Library.h"

namespace ai
{

#define MAX_DISTANCE_FOR_REACHING_ENTITY 50.0f

// This should be unreachable if no target position is specified.
MoveToPositionTask::MoveToPositionTask() :
	_targetPosition(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY),
	_prevTargetPosition(0,0,0),
	_targetYaw(idMath::INFINITY),
	_targetEntity(NULL)
{}

MoveToPositionTask::MoveToPositionTask(const idVec3& targetPosition, float targetYaw) :
	_targetPosition(targetPosition),
	_prevTargetPosition(0,0,0),
	_targetYaw(targetYaw),
	_targetEntity(NULL)
{}

MoveToPositionTask::MoveToPositionTask(idEntity* targetEntity) :
	_targetPosition(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY),
	_prevTargetPosition(0,0,0),
	_targetYaw(idMath::INFINITY),
	_targetEntity(targetEntity)
{}

// Get the name of this task
const idStr& MoveToPositionTask::GetName() const
{
	static idStr _name(TASK_MOVE_TO_POSITION);
	return _name;
}

void MoveToPositionTask::Init(idAI* owner, Subsystem& subsystem)
{
	// Just init the base class
	Task::Init(owner, subsystem);
}

bool MoveToPositionTask::Perform(Subsystem& subsystem)
{
	DM_LOG(LC_AI, LT_INFO)LOGSTRING("run to position Task performing.\r");

	idAI* owner = _owner.GetEntity();

	// This task may not be performed with empty entity pointer
	assert(owner != NULL);

	// Check for target refinements
	UpdateTargetPosition(owner);

	// Has the target position changed since the last run
	if (_prevTargetPosition != _targetPosition)
	{
		// Yes, move towards this new position
		if (!owner->MoveToPosition(_targetPosition))
		{
			// Destination unreachable, end task
			return true;
		}
	}

	// Remember this target
	_prevTargetPosition = _targetPosition;
		
	if (owner->AI_MOVE_DONE)
	{
		// Position reached, turn to the given yaw, if valid
		if (_targetYaw != idMath::INFINITY)
		{
			owner->TurnToward(_targetYaw);
		}
		return true;
	}
		 
	return false; // not finished yet
}

void MoveToPositionTask::SetPosition(idVec3 position)
{
	_targetPosition = position;

	// Invalidate the previous target position
	_prevTargetPosition = position + idVec3(1,1,1);
}

void MoveToPositionTask::UpdateTargetPosition(idAI* owner)
{
	// We have a target entity, this might be a moving target
	if (_targetEntity != NULL)
	{
		_targetPosition = _targetEntity->GetPhysics()->GetOrigin();

		// Let's see if we're close to the target already
		const idVec3& curPos = owner->GetPhysics()->GetOrigin();
		float distance = (curPos - _targetPosition).LengthFast();

		if (distance < MAX_DISTANCE_FOR_REACHING_ENTITY)
		{
			// Terminate this task
			_targetPosition = curPos;
		}
	}
}

// Save/Restore methods
void MoveToPositionTask::Save(idSaveGame* savefile) const
{
	Task::Save(savefile);

	savefile->WriteVec3(_targetPosition);
	savefile->WriteVec3(_prevTargetPosition);
	savefile->WriteFloat(_targetYaw);
	savefile->WriteObject(_targetEntity);
}

void MoveToPositionTask::Restore(idRestoreGame* savefile)
{
	Task::Restore(savefile);

	savefile->ReadVec3(_targetPosition);
	savefile->ReadVec3(_prevTargetPosition);
	savefile->ReadFloat(_targetYaw);
	savefile->ReadObject(reinterpret_cast<idClass*&>(_targetEntity));
}

MoveToPositionTaskPtr MoveToPositionTask::CreateInstance()
{
	return MoveToPositionTaskPtr(new MoveToPositionTask);
}

// Register this task with the TaskLibrary
TaskLibrary::Registrar moveToPositionTaskRegistrar(
	TASK_MOVE_TO_POSITION, // Task Name
	TaskLibrary::CreateInstanceFunc(&MoveToPositionTask::CreateInstance) // Instance creation callback
);

} // namespace ai
