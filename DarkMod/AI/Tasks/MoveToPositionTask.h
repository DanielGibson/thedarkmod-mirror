/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef __AI_MOVE_TO_POSITION_H__
#define __AI_MOVE_TO_POSITION_H__

#include "Task.h"

namespace ai
{

// Define the name of this task
#define TASK_MOVE_TO_POSITION "MoveToPosition"

class MoveToPositionTask;
typedef boost::shared_ptr<MoveToPositionTask> MoveToPositionTaskPtr;

class MoveToPositionTask :
	public Task
{
private:

	// The target position
	idVec3 _targetPosition;

	// The previous target position
	idVec3 _prevTargetPosition;

	// Target yaw (is not INFINITY if set)
	float _targetYaw;

	idEntity* _targetEntity;

	// Default constructor
	MoveToPositionTask();

public:
	// Constructor taking the target position (and optional target yaw) as input argument
	MoveToPositionTask(const idVec3& targetPosition, float targetYaw = idMath::INFINITY);

	// Constructor taking a target entity
	MoveToPositionTask(idEntity* targetEntity);

	// Get the name of this task
	virtual const idStr& GetName() const;

	// Override the base Init method
	virtual void Init(idAI* owner, Subsystem& subsystem);

	virtual bool Perform(Subsystem& subsystem);

	void SetPosition(idVec3 targetPosition);

	// Save/Restore methods
	virtual void Save(idSaveGame* savefile) const;
	virtual void Restore(idRestoreGame* savefile);

	// Creates a new Instance of this task
	static MoveToPositionTaskPtr CreateInstance();

private:
	// Refines the goal position, if appropriate
	void UpdateTargetPosition(idAI* owner);
};

} // namespace ai

#endif /* __AI_MOVE_TO_POSITION_H__ */
