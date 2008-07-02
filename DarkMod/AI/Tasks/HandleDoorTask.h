/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef __AI_HANDLE_DOOR_TASK_H__
#define __AI_HANDLE_DOOR_TASK_H__

#include "Task.h"

#include "../../BinaryFrobMover.h"

namespace ai
{

// Define the name of this task
#define TASK_HANDLE_DOOR "HandleDoor"

class HandleDoorTask;
typedef boost::shared_ptr<HandleDoorTask> HandleDoorTaskPtr;

class HandleDoorTask :
	public Task
{
private:
	idVec3 _frontPos;
	idVec3 _backPos;

	enum EDoorHandlingState {
		EStateNone,
		EStateMovingToFrontPos,
		EStateWaitBeforeOpen,
		EStateStartOpen,
		EStateOpeningDoor,
		EStateMovingToBackPos,
		EStateWaitBeforeClose,
		EStateStartClose,
		EStateClosingDoor
	} _doorHandlingState;

	int _waitEndTime;
	bool _wasLocked;
	bool _doorInTheWay;
public:
	// Get the name of this task
	virtual const idStr& GetName() const;

	// Override the base Init method
	virtual void Init(idAI* owner, Subsystem& subsystem);

	virtual bool Perform(Subsystem& subsystem);

	virtual void OnFinish(idAI* owner);

	idVec3 GetAwayPos(idAI* owner, CFrobDoor* frobDoor);
	idVec3 GetTowardPos(idAI* owner, CFrobDoor* frobDoor);

	// this checks if the gap is large enough to fit through 
	// partially openend doors (blocked, interrupted)
	bool FitsThrough();

	// open door routine (checks if the door is locked and starts to open it when possible)
	bool OpenDoor();

	void ResetDoor(idAI* owner, CFrobDoor* newDoor);

	void Save(idSaveGame* savefile) const;
	void Restore(idRestoreGame* savefile);


	// Creates a new Instance of this task
	static HandleDoorTaskPtr CreateInstance();
};

} // namespace ai

#endif /* __AI_HANDLE_DOOR_TASK_H__ */
