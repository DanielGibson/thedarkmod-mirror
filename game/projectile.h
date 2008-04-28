/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

// Copyright (C) 2004 Id Software, Inc.
//

#ifndef __GAME_PROJECTILE_H__
#define __GAME_PROJECTILE_H__

/**
* SFinalProjData: Structure storing the final projectile data at impact
* Passed on to CProjectileResult object
**/
typedef struct SFinalProjData_s
{
	/**
	 * greebo: The entity which fired the projectile
	 */
	idEntityPtr<idEntity> Owner;

	/**
	* Final world position of the origin of the projectile on impact
	**/
	idVec3	FinalOrigin;

	/**
	* Final orientation of the projectile on impact
	**/
	idMat3	FinalAxis;

	/**
	* Final linear velocity of the projectile just before impact
	**/
	idVec3	LinVelocity;

	/**
	* Final angular velocity of the projectile just before impact
	**/
	idVec3	AngVelocity;

	/**
	* Direction vector for the axis of the arrow.  Needed for pushing it in.
	**/
	idVec3	AxialDir;

	/**
	* Max Angle of incidence when projectile hits surface
	* (Calculated in CProjectileResult::Init )
	**/
	float IncidenceAngle;

	/**
	* Mass of the projectile (might be useful)
	**/
	float mass;

	/**
	* Name of the surface that was struck
	**/
	idStr	SurfaceType;

} SFinalProjData;

/*
===============================================================================

  idProjectile
	
===============================================================================
*/

extern const idEventDef EV_Explode;
// greebo: Exposed projectile launch method to scripts
extern const idEventDef EV_Launch;

class idProjectile : public idEntity {
public :
	CLASS_PROTOTYPE( idProjectile );

							idProjectile();
	virtual					~idProjectile();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Create( idEntity *owner, const idVec3 &start, const idVec3 &dir );
	virtual void			Launch( const idVec3 &start, const idVec3 &dir, const idVec3 &pushVelocity, const float timeSinceFire = 0.0f, const float launchPower = 1.0f, const float dmgPower = 1.0f );
	virtual void			FreeLightDef( void );

	idEntity *				GetOwner( void ) const;

	virtual void			Think( void );
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );
	virtual void			Explode( const trace_t &collision, idEntity *ignore );
	void					Fizzle( void );

	static idVec3			GetVelocity( const idDict *projectile );
	static idVec3			GetGravity( const idDict *projectile );

	enum {
		EVENT_DAMAGE_EFFECT = idEntity::EVENT_MAXEVENTS,
		EVENT_MAXEVENTS
	};

	static void				DefaultDamageEffect( idEntity *soundEnt, const idDict &projectileDef, const trace_t &collision, const idVec3 &velocity );
	static bool				ClientPredictionCollide( idEntity *soundEnt, const idDict &projectileDef, const trace_t &collision, const idVec3 &velocity, bool addDamageEffect );
	virtual void			ClientPredictionThink( void );
	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );
	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );

protected:
	idEntityPtr<idEntity>	owner;

	struct projectileFlags_s {
		bool				detonate_on_world			: 1;
		bool				detonate_on_actor			: 1;
		bool				randomShaderSpin			: 1;
		bool				isTracer					: 1;
		bool				noSplashDamage				: 1;
	} projectileFlags;

	float					thrust;
	int						thrust_end;
	float					damagePower;

	renderLight_t			renderLight;
	qhandle_t				lightDefHandle;				// handle to renderer light def
	idVec3					lightOffset;
	int						lightStartTime;
	int						lightEndTime;
	idVec3					lightColor;

	idForce_Constant		thruster;
	idPhysics_RigidBody		physicsObj;

	const idDeclParticle *	smokeFly;
	int						smokeFlyTime;

	typedef enum {
		// must update these in script/doom_defs.script if changed
		SPAWNED = 0,
		CREATED = 1,
		LAUNCHED = 2,
		FIZZLED = 3,
		EXPLODED = 4,
		INACTIVE = 5	// greebo: this applies to mines that haven't become active yet
	} projectileState_t;
	
	projectileState_t		state;

protected:
	/**
	* Determine whether the projectile "activates" based on the surface type argument
	* This checks a space-delimited spawnarg list of activating materials and
	* returns true if the struck material activates this projectile
	**/
	bool TestActivated( const char *typeName );

private:
	bool					netSyncPhysics;

	void					AddDefaultDamageEffect( const trace_t &collision, const idVec3 &velocity );

	void					Event_Explode( void );
	void					Event_Fizzle( void );
	void					Event_RadiusDamage( idEntity *ignore );
	void					Event_Touch( idEntity *other, trace_t *trace );
	void					Event_GetProjectileState( void );
	void					Event_ActivateProjectile();
	void					Event_Launch( idVec3 const &origin, idVec3 const &direction, idVec3 const &velocity );
};

class idGuidedProjectile : public idProjectile {
public :
	CLASS_PROTOTYPE( idGuidedProjectile );

							idGuidedProjectile( void );
							~idGuidedProjectile( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );
	virtual void			Think( void );
	virtual void			Launch( const idVec3 &start, const idVec3 &dir, const idVec3 &pushVelocity, const float timeSinceFire = 0.0f, const float launchPower = 1.0f, const float dmgPower = 1.0f );

protected:
	float					speed;
	idEntityPtr<idEntity>	enemy;
	virtual void			GetSeekPos( idVec3 &out );

private:
	idAngles				rndScale;
	idAngles				rndAng;
	idAngles				angles;
	int						rndUpdateTime;
	float					turn_max;
	float					clamp_dist;
	bool					burstMode;
	bool					unGuided;
	float					burstDist;
	float					burstVelocity;
};

class idSoulCubeMissile : public idGuidedProjectile {
public:
	CLASS_PROTOTYPE ( idSoulCubeMissile );
	~idSoulCubeMissile();
	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );
	virtual void			Think( void );
	virtual void			Launch( const idVec3 &start, const idVec3 &dir, const idVec3 &pushVelocity, const float timeSinceFire = 0.0f, const float power = 1.0f, const float dmgPower = 1.0f );

protected:
	virtual void			GetSeekPos( idVec3 &out );
	void					ReturnToOwner( void );
	void					KillTarget( const idVec3 &dir );

private:
	idVec3					startingVelocity;
	idVec3					endingVelocity;
	float					accelTime;
	int						launchTime;
	bool					killPhase;
	bool					returnPhase;
	idVec3					destOrg;
	idVec3					orbitOrg;
	int						orbitTime;
	int						smokeKillTime;
	const idDeclParticle *	smokeKill;
};

struct beamTarget_t {
	idEntityPtr<idEntity>	target;
	renderEntity_t			renderEntity;
	qhandle_t				modelDefHandle;
};

class idBFGProjectile : public idProjectile {
public :
	CLASS_PROTOTYPE( idBFGProjectile );

							idBFGProjectile();
							~idBFGProjectile();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );
	virtual void			Think( void );
	virtual void			Launch( const idVec3 &start, const idVec3 &dir, const idVec3 &pushVelocity, const float timeSinceFire = 0.0f, const float launchPower = 1.0f, const float dmgPower = 1.0f );
	virtual void			Explode( const trace_t &collision, idEntity *ignore );

private:
	idList<beamTarget_t>	beamTargets;
	renderEntity_t			secondModel;
	qhandle_t				secondModelDefHandle;
	int						nextDamageTime;
	idStr					damageFreq;

	void					FreeBeams();
	void					Event_RemoveBeams();
	void					ApplyDamage();
};

/*
===============================================================================

  idDebris
	
===============================================================================
*/

class idDebris : public idEntity {
public :
	CLASS_PROTOTYPE( idDebris );

							idDebris();
							~idDebris();

	// save games
	void					Save( idSaveGame *savefile ) const;					// archives object for save game file
	void					Restore( idRestoreGame *savefile );					// unarchives object from save game file

	void					Spawn( void );

	void					Create( idEntity *owner, const idVec3 &start, const idMat3 &axis );
	void					Launch( void );
	void					Think( void );
	void					Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
	void					Explode( void );
	void					Fizzle( void );
	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );


private:
	idEntityPtr<idEntity>	owner;
	idPhysics_RigidBody		physicsObj;
	const idDeclParticle *	smokeFly;
	int						smokeFlyTime;
	const idSoundShader *	sndBounce;

	void					Event_Explode( void );
	void					Event_Fizzle( void );
};

#endif /* !__GAME_PROJECTILE_H__ */
