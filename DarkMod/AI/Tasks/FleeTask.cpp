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

#include "FleeTask.h"
#include "../Memory.h"
#include "../Library.h"

namespace ai
{

// Get the name of this task
const idStr& FleeTask::GetName() const
{
	static idStr _name(TASK_FLEE);
	return _name;
}

void FleeTask::Init(idAI* owner, Subsystem& subsystem)
{
	// Init the base class
	Task::Init(owner, subsystem);
	_fleeStartTime = gameLocal.time;

	_enemy = owner->GetEnemy();
	idActor* enemy = _enemy.GetEntity();

	Memory& memory = owner->GetMemory();
	
	// angua: only set flags when we are not already fleeing
	if (memory.fleeingDone == true)
	{
		_escapeSearchLevel = 3; // 3 means FIND_FRIENDLY_GUARDED
		_distOpt = DIST_NEAREST;
		_failureCount = 0; // This is used for _escapeLevel 1 only
	}

	memory.fleeingDone = false;
	owner->AI_MOVE_DONE = false;
	owner->AI_RUN = true;
}

bool FleeTask::Perform(Subsystem& subsystem)
{
	DM_LOG(LC_AI, LT_INFO)LOGSTRING("Flee Task performing.\r");

	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);
	Memory& memory = owner->GetMemory();
	idActor* enemy = _enemy.GetEntity();
	assert(enemy != NULL);


//	gameRenderWorld->DrawText( va("%d  %d",_escapeSearchLevel, _distOpt), owner->GetPhysics()->GetAbsBounds().GetCenter(), 
//		1.0f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, gameLocal.msec );

	// angua: in any case stop fleeing after max time (5 min).
	// Might stay in flee task forever if pathing to destination not possible otherwise
	// TODO: should be spawn arg, make member 
	int maxFleeTime = 300000;
	if (_failureCount > 5 || 
			(owner->AI_MOVE_DONE && !owner->AI_DEST_UNREACHABLE && !owner->m_HandlingDoor) ||
			gameLocal.time > _fleeStartTime + maxFleeTime)
	{
		owner->StopMove(MOVE_STATUS_DONE);
		//Fleeing is done, check if we can see the enemy
		if (owner->AI_ENEMY_VISIBLE)
		{
			_failureCount = 0;
			if (_distOpt == DIST_NEAREST)
			{
				// Find fleepoint far away
				_distOpt = DIST_FARTHEST;
				_escapeSearchLevel = 3;
			}
			else if (_escapeSearchLevel > 1)
			{
				_escapeSearchLevel --;
			}
		}
		else
		{
			memory.fleeingDone = true;
			// Turn around to look back to where we came from
			owner->TurnToward(owner->GetCurrentYaw() + 180);
			return true;
		}
	}
	
	if (owner->GetMoveStatus() != MOVE_STATUS_MOVING)
	{
		// If the AI is not running yet, start fleeing
		owner->AI_RUN = true;
		if (_escapeSearchLevel >= 3)
		{
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Trying to find escape route - FIND_FRIENDLY_GUARDED.");
			// Flee to the nearest friendly guarded escape point
			if (!owner->Flee(enemy, FIND_FRIENDLY_GUARDED, _distOpt))
			{
				_escapeSearchLevel = 2;
			}
			_fleeStartTime = gameLocal.time;
		}
		else if (_escapeSearchLevel == 2)
		{
			// Try to find another escape route
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Trying alternate escape route - FIND_FRIENDLY.");
			// Find another escape route to ANY friendly escape point
			if (!owner->Flee(enemy, FIND_FRIENDLY, _distOpt))
			{
				_escapeSearchLevel = 1;
			}
		}
		else
		{
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Searchlevel = 1, ZOMG, Panic mode, gotta run now!\r");

			// Get the distance to the enemy
			float enemyDistance = owner->TravelDistance(owner->GetPhysics()->GetOrigin(), enemy->GetPhysics()->GetOrigin());

			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Enemy is as near as %d", enemyDistance);
			if (enemyDistance < 500)
			{
				// Increase the fleeRadius (the nearer the enemy, the more)
				// The enemy is still near, run further
				if (!owner->Flee(enemy, FIND_AAS_AREA_FAR_FROM_THREAT, 500))
				{
					// No point could be found.
					_failureCount++;
				}
			}
			else
			{
				// Fleeing is done for now
				owner->StopMove(MOVE_STATUS_DONE);
			}
		}
	}

	return false; // not finished yet
}

void FleeTask::Save(idSaveGame* savefile) const
{
	Task::Save(savefile);

	savefile->WriteInt(_escapeSearchLevel);
	savefile->WriteInt(_failureCount);
	savefile->WriteInt(_fleeStartTime);

	savefile->WriteInt(static_cast<int>(_distOpt));

	_enemy.Save(savefile);
}

void FleeTask::Restore(idRestoreGame* savefile)
{
	Task::Restore(savefile);

	savefile->ReadInt(_escapeSearchLevel);
	savefile->ReadInt(_failureCount);
	savefile->ReadInt(_fleeStartTime);

	int distOptInt;
	savefile->ReadInt(distOptInt);
	_distOpt = static_cast<EscapeDistanceOption>(distOptInt);

	_enemy.Restore(savefile);
}

FleeTaskPtr FleeTask::CreateInstance()
{
	return FleeTaskPtr(new FleeTask);
}

// Register this task with the TaskLibrary
TaskLibrary::Registrar fleeTaskRegistrar(
	TASK_FLEE, // Task Name
	TaskLibrary::CreateInstanceFunc(&FleeTask::CreateInstance) // Instance creation callback
);

} // namespace ai
