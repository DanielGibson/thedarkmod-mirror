/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 1435 $
 * $Date: 2007-10-16 18:53:28 +0200 (Di, 16 Okt 2007) $
 * $Author: greebo $
 *
 ***************************************************************************/

#ifndef __AI_MIND_H__
#define __AI_MIND_H__

#include "Memory.h"
#include "States/State.h"
#include "Queue.h"

namespace ai
{

class Mind
{
private:
	// The reference to the owning entity
	idEntityPtr<idAI> _owner;

	StateQueue _stateQueue;

	// The structure holding all the variables
	Memory _memory;

	// This holds the id of the subsystem whose turn it is next frame
	SubsystemId _subsystemIterator;

	// TRUE if the the State is about to be switched the next frame
	bool _switchState;

	// A temporary variable to hold dying states
	// This is needed to avoid immediate destruction of states upon switching
	StatePtr _recycleBin;

public:
	Mind(idAI* owner);
	virtual ~Mind() {}

	virtual void Think();

	// State-related methods, see Mind base class for documentation
	virtual void PushState(const idStr& stateName);
	virtual void PushState(const StatePtr& state);

	virtual bool EndState();
	virtual void SwitchState(const idStr& stateName);
	virtual void SwitchState(const StatePtr& state);

	virtual void ClearStates();

	// Returns the reference to the current state
	ID_INLINE const StatePtr& GetState() const {
		assert(_stateQueue.size() > 0);
		return _stateQueue.front();
	}

	// Returns the Memory structure, which holds the various mind variables
	ID_INLINE Memory& GetMemory() {
		return _memory;
	}

	virtual void SetAlertPos();
	virtual void PerformSensoryScan(bool processNewStimuli);
	virtual bool PerformCombatCheck();
	virtual bool SetTarget();

	virtual void Save(idSaveGame* savefile) const;
	virtual void Restore(idRestoreGame* savefile);
};
typedef boost::shared_ptr<Mind> MindPtr;

} // namespace ai

#endif /* __AI_MIND_H__ */
