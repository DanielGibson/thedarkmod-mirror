/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef __AI_FLEE_TASK_H__
#define __AI_FLEE_TASK_H__

#include "Task.h"
#include "../../EscapePointManager.h"

namespace ai
{

// Define the name of this task
#define TASK_FLEE "FleeTask"

class FleeTask;
typedef boost::shared_ptr<FleeTask> FleeTaskPtr;

class FleeTask :
	public Task
{
	idEntityPtr<idActor> _enemy;
	int _escapeSearchLevel;
	int _failureCount;
	int _fleeStartTime;
	EscapeDistanceOption _distOpt;

public:
	// Get the name of this task
	virtual const idStr& GetName() const;

	// Override the base Init method
	virtual void Init(idAI* owner, Subsystem& subsystem);

	virtual bool Perform(Subsystem& subsystem);

	// Save/Restore methods
	virtual void Save(idSaveGame* savefile) const;
	virtual void Restore(idRestoreGame* savefile);

	// Creates a new Instance of this task
	static FleeTaskPtr CreateInstance();

};

} // namespace ai

#endif /* __AI_FLEE_TASK_H__ */
