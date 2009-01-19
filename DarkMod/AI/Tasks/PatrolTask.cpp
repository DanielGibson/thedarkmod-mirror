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
#include "PatrolTask.h"
#include "PathCornerTask.h"
#include "PathTurnTask.h"
#include "PathWaitTask.h"
#include "PathAnimTask.h"
#include "PathCycleAnimTask.h"
#include "PathSitTask.h"
#include "PathWaitForTriggerTask.h"
#include "PathHideTask.h"
#include "PathShowTask.h"
#include "PathLookatTask.h"
#include "PathInteractTask.h"
#include "../Library.h"

namespace ai
{

// Get the name of this task
const idStr& PatrolTask::GetName() const
{
	static idStr _name(TASK_PATROL);
	return _name;
}

void PatrolTask::Init(idAI* owner, Subsystem& subsystem)
{
	// Init the base class
	Task::Init(owner, subsystem);

	// Check if we are supposed to patrol and make sure that there
	// is a valid PathCorner entity set in the AI's mind

	if (owner->spawnArgs.GetBool("patrol", "1")) 
	{
		idPathCorner* path = owner->GetMemory().currentPath.GetEntity();

		// Check if we already have a path entity
		if (path == NULL)
		{
			// Path not yet initialised, get it afresh
			// Find the next path associated with the owning AI
			path = idPathCorner::RandomPath(owner, NULL, owner);
		}

		// If the path is still NULL, there is nothing setup, quit this task
		if (path == NULL)
		{
			// No path corner entities found!
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Warning: No Path corner entites found for %s\r", owner->name.c_str());
			
			subsystem.FinishTask();
			return;
		}

		// Store the path entity back into the mind, it might have changed
		owner->GetMemory().currentPath = path;
	}
	else
	{
		subsystem.FinishTask();
		return;
	}
}

bool PatrolTask::Perform(Subsystem& subsystem)
{
	DM_LOG(LC_AI, LT_INFO)LOGSTRING("Patrol Task performing.\r");

	idPathCorner* path = _owner.GetEntity()->GetMind()->GetMemory().currentPath.GetEntity();

	// This task may not be performed with an empty path corner entity,
	// that case should have been caught by the Init() routine
	assert(path);

	TaskPtr task;

	// Get the classname, this determines the child routine we're spawning.
	idStr classname = path->spawnArgs.GetString("classname");

	// Depending on the classname we spawn one of the various Path*Tasks
	if (classname == "path_corner")
	{
		task = PathCornerTaskPtr(new PathCornerTask(path));
	}
	else if (classname == "path_anim")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			subsystem.PushTask(TaskPtr(new PathAnimTask(path)));
			// The "task" variable will be pushed later on in this code
			task = PathTurnTaskPtr(new PathTurnTask(path));
		}
		else 
		{
			// No "angle" key set, just schedule the animation task
			task = PathAnimTaskPtr(new PathAnimTask(path));
		}
	}
	else if (classname == "path_cycleanim")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			subsystem.PushTask(TaskPtr(new PathCycleAnimTask(path)));
			// The "task" variable will be pushed later on in this code
			task = PathTurnTaskPtr(new PathTurnTask(path));
		}
		else 
		{
			// No "angle" key set, just schedule the animation task
			task = PathCycleAnimTaskPtr(new PathCycleAnimTask(path));
		}
	}
	else if (classname == "path_sit")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			subsystem.PushTask(TaskPtr(new PathSitTask(path)));
			// The "task" variable will be pushed later on in this code
			task = PathTurnTaskPtr(new PathTurnTask(path));
		}
		else 
		{
			// No "angle" key set, just schedule the animation task
			task = PathSitTaskPtr(new PathSitTask(path));
		}
	}

	else if (classname == "path_turn")
	{
		task = PathTurnTaskPtr(new PathTurnTask(path));
	}
	else if (classname == "path_wait")
	{
		if (path->spawnArgs.FindKey("angle") != NULL)
		{
			// We have an angle key set, push a PathTurnTask on top of the anim task
			subsystem.PushTask(TaskPtr(new PathWaitTask(path)));
			// The "task" variable will be pushed later on in this code
			task = PathTurnTaskPtr(new PathTurnTask(path));
		}
		else 
		{
			// No "angle" key set, just schedule the animation task
			task = PathWaitTaskPtr(new PathWaitTask(path));
		}
	}
	else if (classname == "path_waitfortrigger")
	{
		task = PathWaitForTriggerTaskPtr(new PathWaitForTriggerTask(path));
	}
	else if (classname == "path_hide")
	{
		task = PathHideTaskPtr(new PathHideTask(path));
	}
	else if (classname == "path_show")
	{
		task = PathShowTaskPtr(new PathShowTask(path));
	}
	else if (classname == "path_lookat")
	{
		task = PathLookatTaskPtr(new PathLookatTask(path));
	}
	else if (classname == "path_interact")
	{
		task = PathInteractTaskPtr(new PathInteractTask(path));
	}
	else
	{
		// Finish this task
		gameLocal.Warning("Unknown path corner classname '%s'\n", classname.c_str());
		return true;
	}
	
	idPathCorner* next = idPathCorner::RandomPath(path, NULL, _owner.GetEntity());

	if (next == NULL)
	{
		DM_LOG(LC_AI, LT_INFO)LOGSTRING("Cannot advance path pointer, no more targets.\r");
		subsystem.SwitchTask(task);
		// finish patrolling after this path
	}
	else
	{
		subsystem.PushTask(task);
	}


	return false; // not finished yet
}

PatrolTaskPtr PatrolTask::CreateInstance()
{
	return PatrolTaskPtr(new PatrolTask);
}

// Register this task with the TaskLibrary
TaskLibrary::Registrar patrolTaskRegistrar(
	TASK_PATROL, // Task Name
	TaskLibrary::CreateInstanceFunc(&PatrolTask::CreateInstance) // Instance creation callback
);

} // namespace ai
