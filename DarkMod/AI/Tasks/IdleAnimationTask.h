/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef __AI_IDLE_ANIMATION_TASK_H__
#define __AI_IDLE_ANIMATION_TASK_H__

#include "Task.h"

namespace ai
{

// Define the name of this task
#define TASK_IDLE_ANIMATION "IdleAnimation"

class IdleAnimationTask;
typedef boost::shared_ptr<IdleAnimationTask> IdleAnimationTaskPtr;

class IdleAnimationTask :
	public Task
{
	int _nextAnimationTime;

	idStringList _idleAnimations;
	idStringList _idleAnimationsTorso;
	idStringList _idleAnimationsSitting;

	int _idleAnimationInterval;

	// The index of the last anim played (to avoid duplicates)
	int _lastIdleAnim;

	// Default constructor is private
	IdleAnimationTask();
public:
	// Get the name of this task
	virtual const idStr& GetName() const;

	// Override the base Init method
	virtual void Init(idAI* owner, Subsystem& subsystem);

	virtual bool Perform(Subsystem& subsystem);

	virtual void OnFinish(idAI* owner);

	// Save/Restore methods
	virtual void Save(idSaveGame* savefile) const;
	virtual void Restore(idRestoreGame* savefile);

	// Creates a new Instance of this task
	static IdleAnimationTaskPtr CreateInstance();

protected:
	// De-serialises the comma-separated string list of animations
	void ParseAnimsToList(const std::string& animStringList, idStringList& targetList);

	// Attempt to play an animation from the given list. Set torsoOnly to true if legs channel is forbidden
	void AttemptToPlayAnim(idAI* owner, const idStringList& anims, bool torsoOnly);

	// Returns TRUE if the given anim has no_random_head_turning set 
	bool AnimHasNoHeadTurnFlag(idAI* owner, int animNum);

	// Returns a new idle anim index
	virtual int GetNewIdleAnimIndex(const idStringList& anims, idAI* owner);

	// Returns true if the named anim is ok at this point
	virtual bool AnimIsApplicable(idAI* owner, const idStr& animName);
};

} // namespace ai

#endif /* __AI_IDLE_ANIMATION_TASK_H__ */
