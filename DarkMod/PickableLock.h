/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef _PICKABLE_LOCK_H_
#define _PICKABLE_LOCK_H_

extern const idEventDef EV_TDM_UpdateHandlePosition;

/** 
 * greebo: This implements the Lock part of a door or chest.
 * It can be set up as being pickable by lockpicks.
 *
 * The class provides UseBy() methods and notifies its master
 * about lock state changes using the event system.
 */
class PickableLock :
	public idClass
{
public:
	CLASS_PROTOTYPE( PickableLock );

private:
	// The owning entity
	idEntity*		m_Owner;

	// Whether this lock is locked or unlocked
	bool			m_Locked;

	enum ELockpickState
	{
		UNLOCKED = 0,			// Not locked in the first place
		LOCKED,					// Lockpicking not started yet
		LOCKPICKING_STARTED,	// Right before playing the first pin sample sound
		ADVANCE_TO_NEXT_SAMPLE,	// Right after playing the first ping sample sound
		PIN_SAMPLE,				// Playing pick sample sound (including sample delay)
		PIN_SAMPLE_SWEETSPOT,	// Playing sweetspot sound (for pavlov mode, this is the hotspot)
		PIN_DELAY,				// Delay after pattern (for non-pavlov mode, this is the hotspot)
		AFTER_PIN_DELAY,		// Right after the post-pattern delay
		WRONG_LOCKPICK_SOUND,	// Playing wrong lockpick sound
		PIN_SUCCESS,			// Playing pin success sound
		PIN_FAILED,				// Playing pin failed sound
		LOCK_SUCCESS,			// Playing entire lock success
		PICKED,					// Lock is picked
		NUM_LPSTATES,
	};

	// Lockpick state
	ELockpickState	m_LockpickState;

	/**
	 * greebo: This variable keeps track of how many times the player missed
	 * a single pin of the lock. When auto-pick is enabled, the owner will be
	 * be unlocked automatically after a certain amount of rounds.
	 */
	int				m_FailedLockpickRounds;

	struct PinInfo
	{
		// The pin pattern (a list of sound shader names for each sample)
		idStringList pattern;

		/** 
		 * greebo: This is used for the random handle jiggling while lockpicking.
		 * It holds (for each pin) the posisiton indices the handles should be at,
		 * plus one extra position for the delay after the pattern.
		 *
		 * Example: 
		 * A single pattern has 5 samples: 0 1 2 3 4. When traversing the samples
		 * like this, the handle would move in linear steps to the next pin position.
		 *
		 * Using the info in this variable, the pin positions are mapped to different 
		 * values such that the handle moves randomly, something like this:
		 * 0 3 1 2 4. Here, the sample with the index 1 would be mapped 
		 * to the linear position 3. The first and last position are fixed.
		 */
		idList<int> positions;
	};

	idList<PinInfo>	m_Pins;
	
	bool			m_Pickable;

	/**
	 * FirstLockedPinIndex stores the index that is currently to be picked.
	 * If the value gets == m_Pins.Num() it means all pins are picked
	 * and the lock is sucessfully picked.
	 */
	int				m_FirstLockedPinIndex;

	/**
	 * This stores the index of the current pins soundsample to be played.
	 * The second index of the twodimensional array from m_SoundPinIndex.
	 */
	int				m_SoundPinSampleIndex;

	/**
	 * SoundTimerStarted increased for each soundsample that is started
	 */
	int				m_SoundTimerStarted;

public:
	// Default constructor
	PickableLock();

	// Pass the owning entity 
	void			SetOwner(idEntity* owner);

	// Use the given dictionary to initalise this lock (number of pins, etc.)
	void			InitFromSpawnargs(const idDict& spawnArgs);

	// Returns true if this lock is locked (duh)
	bool			IsLocked();
	void			SetLocked(bool locked);

	// Returns true when this lock is pickable at all
	bool			IsPickable();

	// Fork point to determine what should happen with a certain lockpicking impulse
	bool			ProcessLockpickImpulse(EImpulseState impulseState, int type);

	// Save/Restore routines
	void			Save(idSaveGame *savefile) const;
	void			Restore(idRestoreGame *savefile);

	// Called whenever the owner gets locked/unlocked
	void			OnLock();
	void			OnUnlock();

	// Called when the owner changes its frob status
	void			OnFrobbedStatusChange(bool val);

	// Called when a pin is successfully unlocked
	void			OnLockpickPinSuccess();
	// Called when the player failed to unlock this pin
	void			OnLockpickPinFailure();

	// Called when the player hits the attack action
	void			AttackAction(idPlayer* player);

	// Returns the "percentage" of this lock's picked status [0..1]
	float			CalculateHandleMoveFraction();

private:
	/**
	 * greebo: Checks whether the given lockpick type is matching the current pin.
	 * @returns: TRUE on match, FALSE if not matching or lock is not pickable/already picked
	 */
	bool			CheckLockpickType(int type);

	// Specialised methods to handle certain impulse events
	bool			ProcessLockpickPress(int type);
	bool			ProcessLockpickRepeat(int type);
	bool			ProcessLockpickRelease(int type);

	// During the lockpick "hotspot" phase the player is able to unlock the door
	// when pushing / releasing the right buttons.
	bool			LockpickHotspotActive();

	// For debugging purposes
	void			UpdateLockpickHUD();

	/**
	 * greebo: Play the given sound, this will post a "sound finished" event 
	 * after the sound has been played (+ the given delay in ms).
	 * When the sound is done, the lockpick state will be set to <nextState>.
	 */
	void			PropPickSound(const idStr& picksound, ELockpickState nextState, int additionalDelay = 0);

	// Gets called when a lockpick sound is finished playing
	void			Event_LockpickSoundFinished(ELockpickState nextState);

	/**
	 * Create a random pin pattern for a given pin. Clicks defines the required 
	 * number of clicks for this pin, and BaseCount, defines the minimum number
	 * of clicks, which is always added.
	 */
	idStringList	CreatePinPattern(int Clicks, int BaseCount, int MaxCount, int StrNumLen, const idStr &Header);
};

#endif /* _PICKABLE_LOCK_H_ */
