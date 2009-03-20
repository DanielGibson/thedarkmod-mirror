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
#include "PathSleepTask.h"
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
			task = PathAnimTaskPtr(new PathAnimTask(path));
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
			// We have an angle key set, push a PathTurnTask on top of the anim task
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
			// No "angle" key set, just schedule the animation task
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
		return true;
	}
	
	idPathCorner* next = idPathCorner::RandomPath(path, NULL, _owner.GetEntity());

	if (next == NULL && !tasks.empty())
	{
		DM_LOG(LC_AI, LT_INFO)LOGSTRING("No more targets.\r");

		// This is the last path in the chain, we should end the PatrolTask now
		// Switch to the first task in the list and overwrite the PatrolTask by doing so
		subsystem.SwitchTask(*tasks.begin());

		// Remove the switched task from the list to prevent it from being added again below
		tasks.pop_front();
	}

	// Push the (rest of the) tasks to the subsystem
	for (std::list<TaskPtr>::iterator i = tasks.begin(); i != tasks.end(); ++i)
	{
		subsystem.PushTask(*i);
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
