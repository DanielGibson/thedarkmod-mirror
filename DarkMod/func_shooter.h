/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 866 $
 * $Date: 2007-03-23 22:25:02 +0100 (Fr, 23 M�r 2007) $
 * $Author: greebo $
 *
 ***************************************************************************/

#ifndef __GAME_FUNC_SHOOTER_H__
#define __GAME_FUNC_SHOOTER_H__

/**
* greebo: This entity fires projectiles in (periodic) intervals.
*		  All the key paramaters can be specified in the according entityDef.
*/
class tdmFuncShooter : 
	public idStaticEntity
{
public:
	CLASS_PROTOTYPE( tdmFuncShooter );

	// Constructor
	tdmFuncShooter( void );

	// Needed on game save/load
	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	// Gets called when this entity is actually being spawned
	void				Spawn( void );

	// Event interface
	void				Event_Activate( idEntity *activator );
	void				Event_ShooterSetState( bool state );
	void				Event_ShooterGetState();
	void				Event_ShooterFireProjectile();

	// Snapshot interface
	virtual void		WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void		ReadFromSnapshot( const idBitMsgDelta &msg );

	// Overload the derived idEntity::Think method so that this object gets called each frame
	virtual void		Think();

	/**
	* Fires a projectile and sets the timer to gameLocal.time.
	*/
	virtual void		Fire();

private:
	// Calculates the next time this shooter should fire
	void				setupNextFireTime();

	// is TRUE if the shooter can fire projectiles
	bool				_active;

	// The time interval between fires in ms
	int					_fireInterval;

	// This is the maximum tolerance the fireInterval can have
	int					_fireIntervalFuzzyness;

	// The last time the shooter fired a projectile
	int					_lastFireTime;

	// The next game time the shooter should fire
	int					_nextFireTime;
};

#endif /* !__GAME_FUNC_SHOOTER_H__ */

