/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 1435 $
 * $Date: 2007-10-16 18:53:28 +0200 (Di, 16 Okt 2007) $
 * $Author: greebo $
 *
 ***************************************************************************/

#ifndef __AI_REACTING_TO_STIMULUS_STATE_H__
#define __AI_REACTING_TO_STIMULUS_STATE_H__

#include "State.h"

namespace ai
{

#define STATE_REACTING_TO_STIMULUS "ReactingToStimulus"
#define PRIORITY_REACTING_TO_STIMULUS 1000

class ReactingToStimulusState :
	public State
{
public:
	// Get the name of this state
	virtual const idStr& GetName() const;

	// Get/set the priority of this state
	virtual int GetPriority() const {
		return PRIORITY_REACTING_TO_STIMULUS;
	}

	// This is called when the state is first attached to the AI's Mind.
	virtual void Init(idAI* owner);

	// Gets called each time the mind is thinking
	virtual void Think(idAI* owner);

	static StatePtr CreateInstance();
};

} // namespace ai

#endif /* __AI_REACTING_TO_STIMULUS_STATE_H__ */
