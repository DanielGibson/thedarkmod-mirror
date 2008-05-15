/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/
#ifndef _MULTI_STATE_MOVER_H_
#define _MULTI_STATE_MOVER_H_

#include "MultiStateMoverPosition.h"

/**
 * greebo: A MultiState mover is an extension to the vanilla D3 elevators.
 *
 * In contrast to the idElevator class, this multistate mover draws the floor
 * information from the MultiStateMoverPosition entities which are targetted
 * by this class.
 * 
 * The MultiStateMover will start moving when it's triggered (e.g. by buttons), 
 * where the information where to go is contained on the triggering button.
 */
class CMultiStateMover : 
	public idMover
{
	idList<MoverPositionInfo> positionInfo;

	idVec3 forwardDirection;

public:
	CLASS_PROTOTYPE( CMultiStateMover );

	CMultiStateMover();

	void	Spawn();

	void	Save(idSaveGame *savefile) const;
	void	Restore(idRestoreGame *savefile);

	void	Activate(idEntity* activator);

	// Returns the list of position infos, populates the list if none are assigned yet.
	const idList<MoverPositionInfo>& GetPositionInfoList();

	/** 
	 * greebo: Returns TRUE if this mover is at the given position.
	 * Note: NULL arguments always return false.
	 */
	bool IsAtPosition(CMultiStateMoverPosition* position);

protected:
	// override idMover's DoneMoving() to trigger targets
	virtual void DoneMoving();

private:
	// greebo: Controls the direction of targetted rotaters, depending on the given moveTargetPosition
	void	SetGearDirection(const idVec3& targetPos);

	// Returns the index of the named position info or -1 if not found
	int		GetPositionInfoIndex(const idStr& name) const;

	// Returns the index of the position info at the given position (using epsilon comparison)
	int		GetPositionInfoIndex(const idVec3& pos) const;

	// Returns the positioninfo entity of the given location or NULL if no suitable position found 
	CMultiStateMoverPosition* GetPositionEntity(const idVec3& pos) const;

	// Extracts all position entities from the targets
	void FindPositionEntities();

	void Event_Activate(idEntity* activator);
	void Event_PostSpawn();
};

#endif /* _MULTI_STATE_MOVER_H_ */
