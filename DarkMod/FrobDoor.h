/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Source$
 * $Revision$
 * $Date$
 * $Author$
 *
 * $Log$
 * Revision 1.21  2006/11/04 11:00:20  sparhawk
 * Randomizer for lockpicking added.
 *
 * Revision 1.20  2006/11/01 11:57:51  sparhawk
 * Signals method added to entity.
 *
 * Revision 1.19  2006/10/31 12:33:37  sparhawk
 * Doorhandle rotation added
 *
 * Revision 1.18  2006/10/30 17:10:25  sparhawk
 * Doorhandles are now working in the first stage.
 *
 * Revision 1.17  2006/10/03 13:13:39  sparhawk
 * Changes for door handles
 *
 * Revision 1.16  2006/06/27 08:48:45  ishtvan
 * fixed closing of portals more cleanly
 *
 * Revision 1.15  2006/06/27 08:33:57  ishtvan
 * fixed closing of portals
 *
 * Revision 1.14  2006/06/21 15:02:27  sparhawk
 * FrobDoor derived now from BinaryFrobMover
 *
 * Revision 1.13  2006/05/07 21:52:12  ishtvan
 * *) fixed interruption on opening problem
 * *) Added 'interruptable' spawnarg
 * *) Added offset position for translation in case the item starts inbetween states
 * *) Added translation speed variable
 *
 * Revision 1.12  2006/05/06 20:23:35  sparhawk
 * Fixed problem with determining when the animation is finished.
 *
 * Revision 1.11  2006/05/03 21:31:21  sparhawk
 * Statechange callback script added.
 *
 * Revision 1.10  2006/05/02 20:39:33  sparhawk
 * Translation added
 *
 * Revision 1.9  2006/04/29 22:10:56  sparhawk
 * Added some script functions to query the state of a door.
 *
 * Revision 1.8  2006/01/22 09:20:24  ishtvan
 * rewrote to match new soundprop interface
 *
 * Revision 1.7  2005/09/29 04:03:08  ishtvan
 * added support for double doors
 *
 * Revision 1.6  2005/09/27 08:07:15  ishtvan
 * *) Added new member vars to support multiple frob functionality
 *
 * *) Overloaded idMover::RotationDone()
 *
 * Revision 1.5  2005/04/07 08:42:38  ishtvan
 * Added placeholder method GetSoundLoss, which is called by CsndProp
 *
 * Revision 1.4  2004/11/24 21:59:06  sparhawk
 * *) Multifrob implemented
 * *) Usage of items against other items implemented.
 * *) Basic Inventory system added.
 *
 * Revision 1.3  2004/11/21 01:02:03  sparhawk
 * Doors can now be properly opened and have sound.
 *
 * Revision 1.2  2004/11/16 23:56:03  sparhawk
 * Frobcode has been generalized now and resides for all entities in the base classe.
 *
 * Revision 1.1  2004/11/14 20:19:12  sparhawk
 * Initial Release
 *
 *
 ***************************************************************************/

// Copyright (C) 2004 Gerhard W. Gruber <sparhawk@gmx.at>
//

#ifndef FROBDOOR_H
#define FROBDOOR_H

class CFrobDoorHandle;

// Number of clicksounds available
#define	MAX_PIN_CLICKS			14

// Number of clicks that have to be set as a minimum. A pattern of less than 
// 5 clicks is very easy to recognize, so it doesn't make sense to allow less than that.
#define MIN_CLICK_NUM			5
#define MAX_CLICK_NUM			10

/**
 * CFrobDoor is a replacement for idDoor. The reason for this replacement is
 * because idDoor is derived from idMover_binary and can only slide from one
 * position into another. In order to make swinging doors we need to rotate
 * them but this doesn't work with normal idDoors. So CFrobDoor is a mixture
 * of idDoor and idMover.
 */
class CFrobDoor : public CBinaryFrobMover {
public:
	CLASS_PROTOTYPE( CFrobDoor );

							CFrobDoor( void );

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

	void					Open(bool Master);
	void					OpenDoor(bool Master);		// Needed for the handle to riger the open state
	void					Close(bool Master);
	void					Lock(bool Master);
	void					Unlock(bool Master);

	void					GetPickable(void);
	void					GetDoorhandle(void);

	bool					UsedBy(idEntity *);

	/**
	* Write the proper sound loss value to the soundprop portal data
	* Called when door spawns, is and when it is opened or closed
	**/
	void					UpdateSoundLoss(void);

	/**
	* Find out if this door is touching another door, and if they share the same portal
	* If so, store a pointer to the other door m_DoubleDoor on this door.
	*
	* This is posted as an event to be called on all doors after entities spawn
	**/
	void					FindDoubleDoor( void );

	/**
	* Return the double door.  Returns NULL if there is none.
	**/
	CFrobDoor *				GetDoubleDoor( void );

	/**
	* Close the visportal, but only if the double door is also closed.
	**/
	void					ClosePortal( void );

	void					SetDoorhandle(CFrobDoorHandle *);
	void					SetFrobbed(bool val);
	bool					IsFrobbed(void);

	void					DoneStateChange(void);
	void					ToggleOpen(void);
	void					ToggleLock(void);

protected:
	// Create a random pin pattern for a given pin. Clicks defines the required 
	// number of clicks for this pin, and BaseCount, defines the minimum number
	// of clicks, which is always added.
	CStringList				*CreatePinPattern(int Clicks, int BaseCount);

protected:
	/**
	 * LinkedOpen will point to a door that is to be switched when this
	 * one is triggered. Note that the next door is flipped! This means
	 * it will change it's state according to it's current state. So if
	 * this door is open and the other one is closed this door will be
	 * closed and the other one will be opened. If both are open and they
	 * are used, both are closed and vice versa. With this pointer you can
	 * also create a chain of doors by each door pointing to the next one.
	 * Of ocurse the last door in the chain should NOT point to the first
	 * door, otherwise it will result in an endless loop.
	 */
	idStr						m_MasterOpen;
	idList<idStr>				m_OpenList;

	/**
	 * This member is the same as m_LinkedOpen, only for locks. This means
	 * that, if this door is locked, or unlocked, all other associated doors
	 * will also be locked or unlocked. Again the state depends on the respective
	 * entity state and not on the action itself. This means that if one door
	 * is locked and the other is unlocked, the lockstate will reverse. If both
	 * are locked or unlocked, both will become unlocked or locked.
	 * This way you can create i.e. a safety catch were always one door is open
	 * and the other one is closed. Or you can create a set of doors that all are
	 * locked when this one is unlocked.
	 */
	idStr						m_MasterLock;
	idList<idStr>				m_LockList;

	idList<CStringList *>		m_Pins;
	// Once a pin is successfully picked it should stay so, so we have to remember that state.
	idList<bool>				m_PinsPicked;
	bool						m_Pickable;

	/**
	* Pointer to the door's partner in a double door.
	* Double door means two doors placed right next to eachother, sharing the
	*	same visportal.
	* 
	* The doubledoor does not necessarily have to be linked in a frob chain,
	*	it could be independently opened.
	**/
	CFrobDoor					*m_DoubleDoor;

	/**
	 * Handle that is associated with this door, if the door has one.
	 */
	CFrobDoorHandle				*m_Doorhandle;

private:
};

#endif /* !TDMDOOR_H */
