/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef __AI_FLEE_DONE_H__
#define __AI_FLEE_DONE_H__

#include "State.h"

namespace ai
{

#define STATE_FLEE_DONE "FleeDone"

class FleeDoneState :
	public State
{
private:
	float _oldTurnRate;
	int _turnEndTime;
	bool _searchForFriendDone;

public:
	// Get the name of this state
	virtual const idStr& GetName() const;

	// This is called when the state is first attached to the AI's Mind.
	virtual void Init(idAI* owner);

	// Gets called each time the mind is thinking
	virtual void Think(idAI* owner);

	virtual bool CheckAlertLevel(idAI* owner);

	// Save/Restore methods
	virtual void Save(idSaveGame* savefile) const;
	virtual void Restore(idRestoreGame* savefile);

	static StatePtr CreateInstance();
};

} // namespace ai

#endif /* __AI_FLEE_DONE_H__ */
