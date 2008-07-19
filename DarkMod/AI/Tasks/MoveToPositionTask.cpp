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

// This should be unreachable if no target position is specified.
MoveToPositionTask::MoveToPositionTask() :
	_targetPosition(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY),
	_prevTargetPosition(0,0,0),
	_targetYaw(idMath::INFINITY),
	_targetEntity(NULL),
	_entityReachDistance(DEFAULT_ENTITY_REACH_DISTANCE)
{}

MoveToPositionTask::MoveToPositionTask(const idVec3& targetPosition, float targetYaw) :
	_targetPosition(targetPosition),
	_prevTargetPosition(0,0,0),
	_targetYaw(targetYaw),
	_targetEntity(NULL),
	_entityReachDistance(DEFAULT_ENTITY_REACH_DISTANCE)
{}

MoveToPositionTask::MoveToPositionTask(idEntity* targetEntity, float entityReachDistance) :
	_targetPosition(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY),
	_prevTargetPosition(0,0,0),
	_targetYaw(idMath::INFINITY),
	_targetEntity(targetEntity),
	_entityReachDistance(entityReachDistance)
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

void MoveToPositionTask::SetEntityReachDistance(float distance)
{
	_entityReachDistance = distance;
}

void MoveToPositionTask::UpdateTargetPosition(idAI* owner)
{
	// We have a target entity, this might be a moving target
	if (_targetEntity != NULL)
	{
		_targetPosition = _targetEntity->GetPhysics()->GetOrigin();

		const idVec3& curPos = owner->GetPhysics()->GetOrigin();

		// Let's see if we're close enough to the target already
		float distance = (curPos - _targetPosition).LengthFast();

		if (distance < _entityReachDistance)
		{
			// Terminate this task
			_targetPosition = curPos;
		}
		else
		{
			// Fix for AIs walking away from the target position
			idVec3 delta = _targetEntity->GetPhysics()->GetOrigin() - curPos;
			
			// greebo: Move the target position just at the edge of the other AI's bbox
			float scale;
			_targetEntity->GetPhysics()->GetAbsBounds().RayIntersection(curPos, delta, scale);

			_targetPosition = curPos + delta*scale;
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
	savefile->WriteFloat(_entityReachDistance);
}

void MoveToPositionTask::Restore(idRestoreGame* savefile)
{
	Task::Restore(savefile);

	savefile->ReadVec3(_targetPosition);
	savefile->ReadVec3(_prevTargetPosition);
	savefile->ReadFloat(_targetYaw);
	savefile->ReadObject(reinterpret_cast<idClass*&>(_targetEntity));
	savefile->ReadFloat(_entityReachDistance);
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
