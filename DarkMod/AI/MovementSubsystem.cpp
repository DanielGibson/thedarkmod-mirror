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

#include "MovementSubsystem.h"
#include "Library.h"
#include "States/State.h"
#include "Tasks/ResolveMovementBlockTask.h"
#include "Tasks/AnimalPatrolTask.h"
#include "Tasks/PathCornerTask.h"
#include "Tasks/PathAnimTask.h"
#include "Tasks/PathTurnTask.h"
#include "Tasks/PathCycleAnimTask.h"
#include "Tasks/PathSitTask.h"
#include "Tasks/PathSleepTask.h"
#include "Tasks/PathWaitTask.h"
#include "Tasks/PathWaitForTriggerTask.h"
#include "Tasks/PathHideTask.h"
#include "Tasks/PathShowTask.h"
#include "Tasks/PathLookatTask.h"
#include "Tasks/PathInteractTask.h"
#include "Tasks/MoveToPositionTask.h"


namespace ai
{

#define HISTORY_SIZE 32
#define HISTORY_BOUNDS_THRESHOLD 40	// units (grayman #2345 changed to 40 for quicker blocking recognition; was 10)
#define BLOCK_TIME_OUT 400			// milliseconds (grayman #2345 changed to 400 for quicker blocking recognition; was 800)
#define BLOCKED_TOO_LONG 400		// milliseconds (grayman #2345 - how long to stay blocked w/o moving, if you're not waiting for someone to pass by)
#define MAX_PATH_CORNER_SEARCH_ITERATIONS 100
#define PAUSE_TIME 3000				// milliseconds (grayman #2345 - how long to stay paused after treadmilling)

MovementSubsystem::MovementSubsystem(SubsystemId subsystemId, idAI* owner) :
	Subsystem(subsystemId, owner),
	_curHistoryIndex(0),
	_historyBoundsThreshold(HISTORY_BOUNDS_THRESHOLD),
	_state(ENotBlocked),
	_lastTimeNotBlocked(-1),
	_blockTimeOut(BLOCK_TIME_OUT),
	_timeBlockStarted(-1), // grayman #2345
	_blockTimeShouldEnd(BLOCKED_TOO_LONG), // grayman #2345
	_lastFrameBlockCheck(-1),	// grayman #2345
	_timePauseStarted(-1),		// grayman #2345
	_pauseTimeOut(PAUSE_TIME)	// grayman #2345

{
	_patrolling = false;

	_historyBounds.Clear();

	_originHistory.SetNum(HISTORY_SIZE);
}

// Called regularly by the Mind to run the currently assigned routine.
bool MovementSubsystem::PerformTask()
{
	idAI* owner = _owner.GetEntity();

	// Watchdog to keep AI from running into things forever
	CheckBlocked(owner);

	Patrol();
	
	return Subsystem::PerformTask();
}


void MovementSubsystem::StartPatrol()
{
	if (!_patrolling)
	{
		idAI* owner = _owner.GetEntity();
		Memory& memory = owner->GetMemory();

		bool animalPatrol = owner->spawnArgs.GetBool("animal_patrol", "0");

		// Check if the owner has patrol routes set
		idPathCorner* path = memory.currentPath.GetEntity();
		idPathCorner* lastPath = memory.lastPath.GetEntity();
		
		if (path == NULL && lastPath == NULL)
		{
			// Get a new random path off the owner's targets, this is the current one
			path = idPathCorner::RandomPath(owner, NULL, owner);
			memory.currentPath = path;

			// Also, pre-select a next path to allow path predictions
			if (path != NULL)
			{
				memory.nextPath = idPathCorner::RandomPath(path, NULL, owner);
			}
		}
		else if (path != NULL)
		{
			// we already have a stored path, patrolling was already started and is resumed now
			// if we are currently sleeping/sitting, just continue where we are (probably a path_wait)
			if (owner->GetMoveType() != MOVETYPE_SLEEP && owner->GetMoveType() != MOVETYPE_SIT)
			{
				idPathCorner* candidate = GetNextPathCorner(path, owner);

				if (candidate != NULL)
				{
					// advance to next path corner, don't resume other path tasks at the current (presumably wrong) position 
					memory.currentPath = candidate;
					memory.nextPath = idPathCorner::RandomPath(candidate, NULL, owner);
				}
				else
				{
					// We don't have a valid path_corner in our current path branch,
					// or we ended in a "dead end" or in a loop, restart the system
					RestartPatrol();
				}
			}
		}
		else // path == NULL && last path != NULL
		{
			// patrol routine had ended before
			// restart patrolling
			RestartPatrol();
		}

		if (memory.currentPath.GetEntity() != NULL || animalPatrol)
		{
			if (animalPatrol)
			{
				// For animals, push the AnimalPatrol task anyway, they don't need paths
				PushTask(AnimalPatrolTask::CreateInstance());
			}
			else
			{
				StartPathTask();
			}
		
			_patrolling = true;
		}
	}
}

idPathCorner* MovementSubsystem::GetNextPathCorner(idPathCorner* curPath, idAI* owner)
{
	if (curPath == NULL) return NULL; // safety check

	idPathCorner* currentTestPath = curPath;
	
	for (int i = 0; i < MAX_PATH_CORNER_SEARCH_ITERATIONS; i++)
	{
		if (idStr::Cmp(currentTestPath->spawnArgs.GetString("classname"), "path_corner") == 0)
		{
			// found a path_corner
			return currentTestPath;
		}

		// get next path
		currentTestPath = idPathCorner::RandomPath(currentTestPath, NULL, owner);

		if (currentTestPath == NULL)
		{
			// dead end, return NULL
			return NULL;
		}
		else if (currentTestPath == curPath)
		{
			// loop detected
			return NULL;
		}
	}

	return NULL;
}

void MovementSubsystem::RestartPatrol()
{
	idAI* owner = _owner.GetEntity();
	Memory& memory = owner->GetMemory();

	idPathCorner* newPath = idPathCorner::RandomPath(owner, NULL, owner);
	memory.currentPath = newPath;
	memory.nextPath = idPathCorner::RandomPath(newPath, NULL, owner);

	// if the first path is a path corner, just start with that
	// otherwise, move to idle position before restarting patrol
	if (idStr::Cmp(newPath->spawnArgs.GetString("classname"), "path_corner") != 0)
	{
		float startPosTolerance = owner->spawnArgs.GetFloat("startpos_tolerance", "-1");
		owner->movementSubsystem->PushTask(
			TaskPtr(new MoveToPositionTask(memory.idlePosition, memory.idleYaw, startPosTolerance))
		);
	}
}

void MovementSubsystem::Patrol()
{
	idAI* owner = _owner.GetEntity();
	Memory& memory = owner->GetMemory();

	if (_patrolling == false)
	{
		return;
	}

	if (_taskQueue.empty())
	{
		NextPath();
		if (memory.currentPath.GetEntity() == NULL)
		{
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("No more targets.\r");
			_patrolling = false;
			return;
		}

		StartPathTask();
	}
}

void MovementSubsystem::NextPath()
{
	idAI* owner = _owner.GetEntity();
	Memory& memory = owner->GetMemory();

	idPathCorner* path = memory.currentPath.GetEntity();

	// The current path gets stored in lastPath
    memory.lastPath = path;

    // The pre-selected "next path" is now our current one
    idPathCorner* currentPath = memory.nextPath.GetEntity();

    memory.currentPath = currentPath;

    // Now pre-select a new (random) path entity for the next round
    // this information is important for the PathCornerTask to decide which action to take on exit
	idPathCorner* next(NULL);
	if (currentPath != NULL)
	{
		next = idPathCorner::RandomPath(currentPath, NULL, owner);
	}
	
    memory.nextPath = next;
}

void MovementSubsystem::StartPathTask()
{
	DM_LOG(LC_AI, LT_INFO)LOGSTRING("Starting next path task.\r");

	idPathCorner* path = _owner.GetEntity()->GetMind()->GetMemory().currentPath.GetEntity();

	// This may not be performed with an empty path corner entity,
	// that case should have been caught by the Patrol() routine
	assert(path);

	std::list<TaskPtr> tasks;
	TaskPtr task;

	// Get the classname, this determines the child routine we're spawning.
	idStr classname = path->spawnArgs.GetString("classname");

	// Depending on the classname we spawn one of the various Path*Tasks
	if (classname == "path_corner")
	{
		tasks.push_back(TaskPtr(new PathCornerTask(path)));
	}
	else if (classname == "path_anim")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			tasks.push_back(TaskPtr(new PathAnimTask(path)));
			// The "task" variable will be pushed later on in this code
			tasks.push_back(TaskPtr(new PathTurnTask(path)));
		}
		else 
		{
			// No "angle" key set, just schedule the animation task
			tasks.push_back(TaskPtr(new PathAnimTask(path)));
		}
	}
	else if (classname == "path_cycleanim")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			tasks.push_back(TaskPtr(new PathCycleAnimTask(path)));
			// The "task" variable will be pushed later on in this code
			tasks.push_back(PathTurnTaskPtr(new PathTurnTask(path)));
		}
		else 
		{
			// No "angle" key set, just schedule the animation task
			tasks.push_back(PathCycleAnimTaskPtr(new PathCycleAnimTask(path)));
		}
	}
	else if (classname == "path_sit")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			tasks.push_back(TaskPtr(new PathSitTask(path)));
			// The "task" variable will be pushed later on in this code
			tasks.push_back(PathTurnTaskPtr(new PathTurnTask(path)));
		}
		else 
		{
			// No "angle" key set, just schedule the animation task
			tasks.push_back(PathSitTaskPtr(new PathSitTask(path)));
		}
	}

	else if (classname == "path_sleep")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the sleep task
			tasks.push_back(TaskPtr(new PathSleepTask(path)));
			// The "task" variable will be pushed later on in this code
			tasks.push_back(PathTurnTaskPtr(new PathTurnTask(path)));
		}
		else 
		{
			// No "angle" key set, just schedule the sleep task
			tasks.push_back(PathSleepTaskPtr(new PathSleepTask(path)));
		}
	}

	else if (classname == "path_turn")
	{
		tasks.push_back(PathTurnTaskPtr(new PathTurnTask(path)));
	}
	else if (classname == "path_wait")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			tasks.push_back(TaskPtr(new PathWaitTask(path)));
			// The "task" variable will be pushed later on in this code
			tasks.push_back(PathTurnTaskPtr(new PathTurnTask(path)));
		}
		else 
		{
			// No "angle" key set, just schedule the wait task
			tasks.push_back(PathWaitTaskPtr(new PathWaitTask(path)));
		}
	}
	else if (classname == "path_waitfortrigger")
	{
		tasks.push_back(PathWaitForTriggerTaskPtr(new PathWaitForTriggerTask(path)));
	}
	else if (classname == "path_hide")
	{
		tasks.push_back(PathHideTaskPtr(new PathHideTask(path)));
	}
	else if (classname == "path_show")
	{
		tasks.push_back(PathShowTaskPtr(new PathShowTask(path)));
	}
	else if (classname == "path_lookat")
	{
		tasks.push_back(PathLookatTaskPtr(new PathLookatTask(path)));
	}
	else if (classname == "path_interact")
	{
		tasks.push_back(PathInteractTaskPtr(new PathInteractTask(path)));
	}
	else
	{
		// Finish this task
		gameLocal.Warning("Unknown path corner classname '%s'\n", classname.c_str());
		return;
	}
	
	// Push the (rest of the) tasks to the subsystem
	for (std::list<TaskPtr>::iterator i = tasks.begin(); i != tasks.end(); ++i)
	{
		PushTask(*i);
	}
}

void MovementSubsystem::ClearTasks()
{
	Subsystem::ClearTasks();
	_patrolling = false;
}

// grayman #2345 - extricate yourself somehow if allowed. One situtation where you
// shouldn't extricate is if an AI blocking you is trying to resolve the block. His
// attempt won't work if you head off in another direction.

bool MovementSubsystem::AttemptToExtricate()
{
	idAI* owner = _owner.GetEntity();

	if (!owner->m_canExtricate)
	{
		return false;
	}

	// Look around to see if there's somewhere you can go.

	const idVec3& ownerOrigin = owner->GetPhysics()->GetOrigin();

	// Set all attachments to nonsolid, temporarily
	owner->SaveAttachmentContents();
	owner->SetAttachmentContents(0);

	// Look around in 90 degree increments. Move toward
	// whichever destination lets you travel the farthest.

	trace_t result;
	idBounds bounds = owner->GetPhysics()->GetBounds();
	float extricateDistance = 2*(bounds[1][0] + bounds[1][1]); // 2 x (x_size/2 + y_size/2)
	idVec3 dir = extricateDistance*owner->viewAxis.ToAngles().ToForward();

	idVec3 traceEnd;
	idBounds bnds = owner->GetPhysics()->GetBounds();
	idVec3 moveTo = ownerOrigin; // initialize to your origin
	float moveFraction = 0;		 // fractional distance from your origin to moveTo

	for (int i = 0 ; i < 3 ; i++)
	{
		// rotate 90 degrees clockwise around z

		float temp = dir.x;
		dir.x = dir.y;
		dir.y = -temp;

		traceEnd = ownerOrigin + dir;
		gameLocal.clip.TraceBounds(result, ownerOrigin, traceEnd, bnds, CONTENTS_SOLID|CONTENTS_CORPSE, owner);

		if (result.fraction >= 1.0f) // didn't hit anything, so quit looking
		{
			moveTo = traceEnd;
			moveFraction = 1;
			break;
		}

		 // Trace hit something. Is this move the farthest yet?

		if (result.fraction > moveFraction)
		{
			moveTo = traceEnd;
			moveFraction = result.fraction;
		}	
	}

	owner->RestoreAttachmentContents(); // Put back attachments

	if (moveFraction == 0) // Going anywhere?
	{
		// No way out. Try again later. Something might move out of the way.
		return false;
	}

	owner->MoveToPosition(moveTo);

	return true;
}

void MovementSubsystem::CheckBlocked(idAI* owner)
{
	// grayman #2345 - this section was moved up from below, so that it happens
	// even when the AI is standing still. A history of origins should reflect
	// standing still.

	const idVec3& ownerOrigin = owner->GetPhysics()->GetOrigin();
	int prevIndex = _curHistoryIndex-1;
	if (prevIndex < 0)
	{
		prevIndex = _originHistory.Num() - 1;
	}
	_originHistory[_curHistoryIndex++] = ownerOrigin;

	// Wrap the index around if needed
	_curHistoryIndex %= _originHistory.Num();

	// Calculate the new bounds
	_historyBounds.FromPoints(_originHistory.Ptr(), _originHistory.Num());

	// grayman #2345 - end of moved section

	// Check the owner's move type to decide whether 
	// we should watch out for possible blocking or not

	if ((owner->GetMoveType() == MOVETYPE_ANIM) && owner->AI_FORWARD)
	{
		// Owner is supposed to be moving

		// grayman #2345 - Rather than checking _historyBoundsThreshold, check the
		// distance traveled since the previous think. This is a more accurate
		// way to recognize being blocked, since it pays more attention to recent
		// history and less to the past. You might be traveling a reasonable distance,
		// but are you getting closer to your goal? Check current yaw against your ideal yaw
		// to see how far off you are.

		idVec3 prevOrigin = _originHistory[prevIndex];
		idVec3 currentOrigin = ownerOrigin;
		currentOrigin.z = prevOrigin.z = 0.0; // ignore vertical components
		idVec3 vecTraveled = currentOrigin - prevOrigin;
		float traveledPrev = vecTraveled.LengthFast(); // new
		traveledPrev /= (gameLocal.framenum - _lastFrameBlockCheck); // normalize to a per-frame value
		float yawDiff = idMath::AngleNormalize180(owner->GetCurrentYaw() - owner->GetIdealYaw()); // how close are you to your ideal yaw?
		idEntity *tactileEntity = owner->GetTactileEntity(); // grayman #2345

		bool belowThreshold = false;
		bool torsoCustomIdleAnim = false;
		bool legsCustomIdleAnim = false;
		if (traveledPrev < 0.1) // movement is very close to zero
		{
			// grayman #2345 - If you're barely moving or stopped, and there's an idle animation playing,
			// don't consider yourself blocked. You're supposed to be moving, but you probably
			// haven't started yet because of the animation. This happens often around doors.

			idStr torsoString = "Torso_CustomIdleAnim";
			idStr legsString = "Legs_CustomIdleAnim";
			torsoCustomIdleAnim = (torsoString.Cmp(owner->GetAnimState(ANIMCHANNEL_TORSO)) == 0);
			legsCustomIdleAnim = (legsString.Cmp(owner->GetAnimState(ANIMCHANNEL_LEGS)) == 0);

			if (!torsoCustomIdleAnim && !legsCustomIdleAnim)
			{
				belowThreshold = true;
			}
		}
		else if (((traveledPrev < 0.3) || (idMath::Fabs(yawDiff) > 45)) && tactileEntity)  // movement is low or you're not heading toward your goal, and you bumped into something
		{
			belowThreshold = true;
		}

		switch (_state)
		{
		case ENotBlocked:
			if (belowThreshold)
			{
				// Yellow alarm, we might be blocked, or we might as well
				// just have been starting to move
				_state = EPossiblyBlocked;
				// Changed state to possibly blocked, record time
				_lastTimeNotBlocked =  gameLocal.time - gameLocal.msec;
			}
			break;
		case EPossiblyBlocked:
			if (belowThreshold)
			{
				if (gameLocal.time >= _lastTimeNotBlocked + _blockTimeOut)
				{
					// Possibly blocked for too long, raise status
					_state = EBlocked;

					// Send a signal to the current State
					owner->GetMind()->GetState()->OnMovementBlocked(owner);
					_timeBlockStarted =  gameLocal.time - gameLocal.msec;
				}
			}
			else if (!torsoCustomIdleAnim && !legsCustomIdleAnim) // Bounds might not be safe yet if you're doing an idle animation
			{
				// Bounds are safe, back to green state
				_state = ENotBlocked;
			}
			break;
		case EBlocked:
			if (belowThreshold)
			{
				// grayman #2345 - blocked too long w/o moving?
				if (gameLocal.time >= _timeBlockStarted + _blockTimeShouldEnd)
				{
					// Do something to extricate yourself. AttemptToExtricate() returns TRUE if
					// it found somewhere to go.
					if (!AttemptToExtricate())
					{
						_timePauseStarted =  gameLocal.time - gameLocal.msec;
						_state = EPaused;
						owner->PushMove();
						owner->StopMove(MOVE_STATUS_WAITING);
					}
				}
			}
			else if (!torsoCustomIdleAnim && !legsCustomIdleAnim) // Bounds might not be safe yet if you're doing an idle animation
			{
				// grayman #2345 - go back to EPossiblyBlocked, instead of all the way back to ENotBlocked
				_state = EPossiblyBlocked;
				_lastTimeNotBlocked =  gameLocal.time - gameLocal.msec;
//				_state = ENotBlocked; // Threshold exceeded, we're unblocked again
			}
			break;
		case EResolvingBlock:
			// nothing so far
			break;
		case EWaiting:	// grayman #2345 - Waiting for passing AI
			break;
		case EPaused:	// grayman #2345 - stop treadmilling for a few seconds
			break;
		};
	}
	else
	{
		// Not moving, or sleeping, or something else
		if (IsWaiting())
		{
			// do nothing
		}
		else if (IsPaused()) // grayman #2345
		{
			if (gameLocal.time >= _timePauseStarted + _pauseTimeOut)
			{
				_state = ENotBlocked;
				owner->PopMove(); // restore move state
			}
		}
		else
		{
			_state = ENotBlocked; // grayman #2345
//		_historyBounds.Clear(); // grayman #2345 - don't clear this; let it reflect true recent movement, even if standing still
		}
	}

	_lastFrameBlockCheck = gameLocal.framenum; // grayman #2345

	DebugDraw(owner);
}

void MovementSubsystem::SetBlockedState(const BlockedState newState)
{
	_state = newState;

	if (_state == ENotBlocked)
	{
		_lastTimeNotBlocked = gameLocal.time;
//		_historyBounds.Clear(); // grayman #2345 - never clear this
	}
}

void MovementSubsystem::SetWaiting(void) // grayman #2345
{
	_state = EWaiting;
}

bool MovementSubsystem::IsWaiting(void) // grayman #2345
{
	return (_state == EWaiting);
}

bool MovementSubsystem::IsPaused(void) // grayman #2345
{
	return (_state == EPaused);
}

bool MovementSubsystem::IsNotBlocked(void) // grayman #2345
{
	return (_state == ENotBlocked);
}

void MovementSubsystem::ResolveBlock(idEntity* blockingEnt)
{
	idAI* owner = _owner.GetEntity();
	DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Asking %s to resolve a block by %s\r", owner->name.c_str(),blockingEnt->name.c_str());
	
	if (owner->GetMemory().resolvingMovementBlock)
	{
		return; // Already resolving
	}

	// Push a resolution task
	PushTask(TaskPtr(new ResolveMovementBlockTask(blockingEnt)));

	// Remember this state
	SetBlockedState(EResolvingBlock);

	// grayman #2345 - tell the blocking AI not to try to extricate if it has a chance

	if (blockingEnt->IsType(idAI::Type))
	{
		idAI* ai = static_cast<idAI*>(blockingEnt);
		if (ai)
		{
			ai->m_canExtricate = false;
		}
	}
}

bool MovementSubsystem::IsResolvingBlock()
{
	return _state == EResolvingBlock;
}

idVec3 MovementSubsystem::GetLastMove(void)	// grayman #2356 - used to help determine true falling near func_statics
{
	int prevIndex = _curHistoryIndex-1;
	if (prevIndex < 0)
	{
		prevIndex = _originHistory.Num() - 1;
	}
	const idVec3& prevOrigin = _originHistory[prevIndex];
	const idVec3& ownerOrigin = _owner.GetEntity()->GetPhysics()->GetOrigin();
	return (ownerOrigin - prevOrigin);
}


// Save/Restore methods
void MovementSubsystem::Save(idSaveGame* savefile) const
{
	Subsystem::Save(savefile);

	savefile->WriteBool(_patrolling);

	savefile->WriteInt(_originHistory.Num());

	for (int i = 0; i < _originHistory.Num(); ++i)
	{
		savefile->WriteVec3(_originHistory[i]);
	}

	savefile->WriteInt(_curHistoryIndex);
	savefile->WriteBounds(_historyBounds);
	savefile->WriteFloat(_historyBoundsThreshold);
	savefile->WriteInt(static_cast<int>(_state));
	savefile->WriteInt(_lastTimeNotBlocked);
	savefile->WriteInt(_blockTimeOut);
	savefile->WriteInt(_timeBlockStarted);		// grayman #2345
	savefile->WriteInt(_blockTimeShouldEnd);	// grayman #2345
	savefile->WriteInt(_lastFrameBlockCheck);	// grayman #2345
	savefile->WriteInt(_pauseTimeOut);			// grayman #2345
}

void MovementSubsystem::Restore(idRestoreGame* savefile)
{
	Subsystem::Restore(savefile);

	savefile->ReadBool(_patrolling);

	int num;
	savefile->ReadInt(num);

	_originHistory.SetNum(num);

	for (int i = 0; i < num; ++i)
	{
		savefile->ReadVec3(_originHistory[i]);
	}

	savefile->ReadInt(_curHistoryIndex);
	savefile->ReadBounds(_historyBounds);
	savefile->ReadFloat(_historyBoundsThreshold);

	int temp;
	savefile->ReadInt(temp);
	assert(temp >= ENotBlocked && temp <= ENumBlockedStates); // grayman #2345 - was EBlocked, which isn't the last value in the enum
	_state = static_cast<BlockedState>(temp);

	savefile->ReadInt(_lastTimeNotBlocked);
	savefile->ReadInt(_blockTimeOut);
	savefile->ReadInt(_timeBlockStarted);		// grayman #2345
	savefile->ReadInt(_blockTimeShouldEnd);		// grayman #2345
	savefile->ReadInt(_lastFrameBlockCheck);	// grayman #2345
	savefile->ReadInt(_pauseTimeOut);			// grayman #2345
}

void MovementSubsystem::DebugDraw(idAI* owner)
{
	if (!cv_ai_debug_blocked.GetBool())
	{
		return;
	}

	if (!_historyBounds.IsCleared())
	{
		gameRenderWorld->DebugBox(colorWhite, idBox(_historyBounds), 3* gameLocal.msec);
	}

	idStr str;
	idVec4 colour;
	switch (_state) 
	{
		case ENotBlocked:
			str = "ENotBlocked";
			colour = colorGreen;
			break;
		case EPossiblyBlocked:
			str = "EPossiblyBlocked";
			colour = colorYellow;
			break;
		case EBlocked:
			str = "EBlocked";
			colour = colorRed;
			break;
		case EResolvingBlock:
			str = "EResolvingBlock";
			colour = colorMagenta;
			break;
		case EWaiting: // grayman #2345
			str = "EWaiting";
			colour = colorBlue;
			break;
		case EPaused: // grayman #2345
			str = "EPaused";
			colour = colorBlue;
			break;
	}

	gameRenderWorld->DrawText(str.c_str(), 
		(owner->GetEyePosition() - owner->GetPhysics()->GetGravityNormal()*60.0f), 
		0.25f, colour, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, 3 * gameLocal.msec);
}

} // namespace ai
