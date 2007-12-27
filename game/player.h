/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Source$
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/
// Copyright (C) 2004 Id Software, Inc.
//

#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

#include "../DarkMod/ButtonStateTracker.h"
class CInventoryWeaponItem;

/*
===============================================================================

	Player entity.
	
===============================================================================
*/

/* FORWARD DECLS */
class idDeclPDA;
class idDeclVideo;

extern const idEventDef EV_Player_GetButtons;
extern const idEventDef EV_Player_GetMove;
extern const idEventDef EV_Player_GetViewAngles;
extern const idEventDef EV_Player_EnableWeapon;
extern const idEventDef EV_Player_DisableWeapon;
extern const idEventDef EV_Player_ExitTeleporter;
extern const idEventDef EV_Player_SelectWeapon;
extern const idEventDef EV_SpectatorTouch;
extern const idEventDef EV_Player_PlayStartSound;
extern const idEventDef EV_Player_DeathMenu;
extern const idEventDef EV_Player_MissionFailed;
extern const idEventDef EV_Player_GiveHealthPool;

const float THIRD_PERSON_FOCUS_DISTANCE	= 512.0f;
const int	LAND_DEFLECT_TIME = 150;
const int	LAND_RETURN_TIME = 300;
const int	FOCUS_TIME = 300;
const int	FOCUS_GUI_TIME = 500;

#define TDM_PLAYER_WEAPON_CATEGORY			"Weapons"
const int MAX_WEAPONS = 16;

const int DEAD_HEARTRATE = 0;			// fall to as you die
const int LOWHEALTH_HEARTRATE_ADJ = 20; // 
const int DYING_HEARTRATE = 30;			// used for volumen calc when dying/dead
const int BASE_HEARTRATE = 70;			// default
const int ZEROSTAMINA_HEARTRATE = 115;  // no stamina
const int MAX_HEARTRATE = 130;			// maximum
const int ZERO_VOLUME = -40;			// volume at zero
const int DMG_VOLUME = 5;				// volume when taking damage
const int DEATH_VOLUME = 15;			// volume at death

const int SAVING_THROW_TIME = 5000;		// maximum one "saving throw" every five seconds

const int ASYNC_PLAYER_INV_AMMO_BITS = idMath::BitsForInteger( 999 );	// 9 bits to cover the range [0, 999]
const int ASYNC_PLAYER_INV_CLIP_BITS = -7;								// -7 bits to cover the range [-1, 60]

struct idLevelTriggerInfo {
	idStr levelName;
	idStr triggerName;
};

// powerups - the "type" in item .def must match
enum {
	BERSERK = 0, 
	INVISIBILITY,
	MEGAHEALTH,
	ADRENALINE,
	MAX_POWERUPS
};

// powerup modifiers
enum {
	SPEED = 0,
	PROJECTILE_DAMAGE,
	MELEE_DAMAGE,
	MELEE_DISTANCE
};

// influence levels
enum {
	INFLUENCE_NONE = 0,			// none
	INFLUENCE_LEVEL1,			// no gun or hud
	INFLUENCE_LEVEL2,			// no gun, hud, movement
	INFLUENCE_LEVEL3,			// slow player movement
};

// Player control immobilization categories.
enum {
	EIM_ALL					= -1,
	EIM_UPDATE				= BIT( 0),	// For internal use only. True if immobilization needs to be recalculated.
	EIM_VIEW_ANGLE			= BIT( 1),	// Looking around
	EIM_MOVEMENT			= BIT( 2),	// Forwards/backwards, strafing and swimming.
	EIM_CROUCH				= BIT( 3),	// Crouching.
	EIM_CROUCH_HOLD			= BIT( 4),	// Prevent changes to crouching state. (NYI)
	EIM_JUMP				= BIT( 5),	// Jumping.
	EIM_CLIMB				= BIT( 6),	// Climbing ladders, ropes and mantling. (NYI)
	EIM_FROB				= BIT( 7),	// Frobbing.
	EIM_ATTACK				= BIT( 8),	// Using weapons (NYI)
	EIM_WEAPON_SELECT		= BIT( 9),	// Selecting weapons.
	EIM_ITEM_USE			= BIT(10),	// Using items
	EIM_ITEM_SELECT			= BIT(11),	// Selecting items.
	EIM_ITEM_DROP			= BIT(12),	// Dropping inventory items.
};
// greebo: The immobilisation mask during objectives GUI display
const int EIM_OBJECTIVES_OPEN = EIM_ALL;

typedef struct {
	int		time;
	idVec3	dir;		// scaled larger for running
} loggedAccel_t;

typedef struct {
	int		areaNum;
	idVec3	pos;
} aasLocation_t;

class idPlayer : public idActor {
public:
	enum {
		EVENT_IMPULSE = idEntity::EVENT_MAXEVENTS,
		EVENT_EXIT_TELEPORTER,
		EVENT_ABORT_TELEPORTER,
		EVENT_POWERUP,
		EVENT_SPECTATE,
		EVENT_MAXEVENTS
	};

	usercmd_t				usercmd;

	class idPlayerView		playerView;			// handles damage kicks and effects

	bool					noclip;
	bool					godmode;

	bool					spawnAnglesSet;		// on first usercmd, we must set deltaAngles
	idAngles				spawnAngles;
	idAngles				viewAngles;			// player view angles
	idAngles				cmdAngles;			// player cmd angles

	int						buttonMask;
	int						oldButtons;
	int						oldFlags;

	int						lastHitTime;			// last time projectile fired by player hit target
	int						lastSndHitTime;			// MP hit sound - != lastHitTime because we throttle
	int						lastSavingThrowTime;	// for the "free miss" effect

	idScriptBool			AI_FORWARD;
	idScriptBool			AI_BACKWARD;
	idScriptBool			AI_STRAFE_LEFT;
	idScriptBool			AI_STRAFE_RIGHT;
	idScriptBool			AI_ATTACK_HELD;
	idScriptBool			AI_WEAPON_FIRED;
	idScriptBool			AI_JUMP;
	idScriptBool			AI_CROUCH;
	idScriptBool			AI_ONGROUND;
	idScriptBool			AI_ONLADDER;
	idScriptBool			AI_DEAD;
	idScriptBool			AI_RUN;
	idScriptBool			AI_PAIN;
	idScriptBool			AI_HARDLANDING;
	idScriptBool			AI_SOFTLANDING;
	idScriptBool			AI_RELOAD;
	idScriptBool			AI_TELEPORT;
	idScriptBool			AI_TURN_LEFT;
	idScriptBool			AI_TURN_RIGHT;
	/**
	* Leaning
	**/
	idScriptBool			AI_LEAN_LEFT;
	idScriptBool			AI_LEAN_RIGHT;
	idScriptBool			AI_LEAN_FORWARD;

	/**
	* Set to true if the player is creeping
	**/
	idScriptBool			AI_CREEP;

	// greebo: The levelTrigger were in the old idInventory structure. This contains
	// all the leveltriggers (which can be used to trigger targets in an upcoming map)
	// Sounds useful, that's why I moved it here.
	idList<idLevelTriggerInfo> levelTriggers;

	/**
	* greebo: Helper class keeping track of which buttons are currently
	*		  held down and which got released.
	*		  calls PerformButtonRelease() on this entity on this occasion.
	*/
	ButtonStateTracker		m_ButtonStateTracker;

	/**
	* Set to true if the player is holding an item with the Grabber
	**/
	bool					m_bGrabberActive;

	/**
	* Set to true if the player is dragging a body
	* NOT YET IMPLEMENTED
	**/
	bool					m_bDraggingBody;

	/**
	* Set to true if the player is shouldering a body
	* NOT YET IMPLEMENTED
	**/
	bool					m_bShoulderingBody;

	/**
	* Hack to fix the leaning test of key-releases
	* Timestamp to wait a few frames before testing for button release
	**/
	int						m_LeanButtonTimeStamp;

	idEntityPtr<idWeapon>	weapon;
	idUserInterface *		hud;				// MP: is NULL if not local player
	idUserInterface *		objectiveSystem;	 // not used by TDM (only for PDA)
	bool					objectiveSystemOpen; // not used by TDM (only for PDA)
	int						objectiveGUIHandle; // is non-zero if objective system is open

	// greebo: A list of HUD messages which are displayed one after the other
	idList<idStr>			hudMessages;

	int						weapon_soulcube;
	int						weapon_pda;
	int						weapon_fists;

	bool					m_HeartBeatAllow; /// disable hearbeat except when dying or drowning - Need this to track state
	int						heartRate;
	idInterpolate<float>	heartInfo;
	int						lastHeartAdjust;
	int						lastHeartBeat;
	int						lastDmgTime;
	int						deathClearContentsTime;
	bool					doingDeathSkin;
	int						lastArmorPulse;		// lastDmgTime if we had armor at time of hit
	float					stamina;
	float					healthPool;			// amount of health to give over time
	int						nextHealthPulse;
	bool					healthPulse;
	bool					healthTake;
	int						nextHealthTake;
	// greebo: added these to make the interval customisable
	int						healthPoolStepAmount;			// The amount of healing in each pulse
	int						healthPoolTimeInterval;			// The time between health pulses
	float					healthPoolTimeIntervalFactor;	// The factor to increase the time interval after each pulse


	bool					hiddenWeapon;		// if the weapon is hidden ( in noWeapons maps )
	idEntityPtr<idProjectile> soulCubeProjectile;

	// mp stuff
	static idVec3			colorBarTable[ 5 ];
	int						spectator;
	idVec3					colorBar;			// used for scoreboard and hud display
	int						colorBarIndex;
	bool					scoreBoardOpen;
	bool					forceScoreBoard;
	bool					forceRespawn;
	bool					spectating;
	int						lastSpectateTeleport;
	bool					lastHitToggle;
	bool					forcedReady;
	bool					wantSpectate;		// from userInfo
	bool					weaponGone;			// force stop firing
	bool					useInitialSpawns;	// toggled by a map restart to be active for the first game spawn
	int						latchedTeam;		// need to track when team gets changed
	int						tourneyRank;		// for tourney cycling - the higher, the more likely to play next - server
	int						tourneyLine;		// client side - our spot in the wait line. 0 means no info.
	int						spawnedTime;		// when client first enters the game

	idEntityPtr<idEntity>	teleportEntity;		// while being teleported, this is set to the entity we'll use for exit
	int						teleportKiller;		// entity number of an entity killing us at teleporter exit
	bool					lastManOver;		// can't respawn in last man anymore (srv only)
	bool					lastManPlayAgain;	// play again when end game delay is cancelled out before expiring (srv only)
	bool					lastManPresent;		// true when player was in when game started (spectators can't join a running LMS)
	bool					isLagged;			// replicated from server, true if packets haven't been received from client.
	bool					isChatting;			// replicated from server, true if the player is chatting.

	// timers
	int						minRespawnTime;		// can respawn when time > this, force after g_forcerespawn
	int						maxRespawnTime;		// force respawn after this time

	// the first person view values are always calculated, even
	// if a third person view is used
	idVec3					firstPersonViewOrigin;
	idMat3					firstPersonViewAxis;

	idDragEntity			dragEntity;

	// A pointer to our weaponslot.
	CInventoryCursor		*m_WeaponCursor;

public:
	CLASS_PROTOTYPE( idPlayer );

							idPlayer();
	virtual					~idPlayer();

	void					Spawn( void );
	void					Think( void );

	/**
	* greebo: This creates all the default inventory items and adds the weapons.
	*/
	void					SetupInventory();

	/**
	* greebo: Parses the spawnargs for any weapon definitions and adds them
	*         to the inventory. Expects the weapon category to exist.
	*/
	void					addWeaponsToInventory();

	// save games
	void					Save( idSaveGame *savefile ) const;					// archives object for save game file
	void					Restore( idRestoreGame *savefile );					// unarchives object from save game file

	virtual void			Hide( void );
	virtual void			Show( void );

	void					Init( void );
	void					PrepareForRestart( void );
	virtual void			Restart( void );
	void					LinkScriptVariables( void );
	void					SetupWeaponEntity( void );
	void					SelectInitialSpawnPoint( idVec3 &origin, idAngles &angles );
	void					SpawnFromSpawnSpot( void );
	void					SpawnToPoint( const idVec3	&spawn_origin, const idAngles &spawn_angles );
	void					SetClipModel( void );	// spectator mode uses a different bbox size

	void					SavePersistantInfo( void );
	void					RestorePersistantInfo( void );
	void					SetLevelTrigger( const char *levelName, const char *triggerName );

	bool					UserInfoChanged( bool canModify );
	idDict *				GetUserInfo( void );
	bool					BalanceTDM( void );

	void					CacheWeapons( void );

	void					EnterCinematic( void );
	void					ExitCinematic( void );
	bool					HandleESC( void );
	bool					SkipCinematic( void );

	int						GetImmobilization();
	int						GetImmobilization( const char *source );
	void					SetImmobilization( const char *source, int type );

	float					GetHinderance();
	/**
	* Sets the movement hinderance.  This should be a fraction relative to max movement speed
	* @Gildoran: What are mCap and aCap? Modifier and absolute?
	**/
	void					SetHinderance( const char *source, float mCap, float aCap );

	/**
	 * greebo: Plays the footstep sound according to the current movement type.
	 */
	virtual void			PlayFootStepSound();

	void					UpdateConditions( void );
	void					SetViewAngles( const idAngles &angles );

							// delta view angles to allow movers to rotate the view of the player
	void					UpdateDeltaViewAngles( const idAngles &angles );

	/**
	* Get or set the listener location for the player, in world coordinates
	**/
	idVec3					GetListenerLoc( void );
	void					SetListenerLoc( idVec3 loc );

	/**
	* Set/Get the door listening location
	**/
	void					SetDoorListenLoc( idVec3 loc );
	idVec3					GetDoorListenLoc( void );


	void					CrashLand( const idVec3 &savedOrigin, const idVec3 &savedVelocity );
	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );

	virtual void			GetAASLocation( idAAS *aas, idVec3 &pos, int &areaNum ) const;
	virtual void			GetAIAimTargets( const idVec3 &lastSightPos, idVec3 &headPos, idVec3 &chestPos );
	virtual void			DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage );
	void					CalcDamagePoints(  idEntity *inflictor, idEntity *attacker, const idDict *damageDef,
							   const float damageScale, const int location, int *health, int *armor );
	virtual	void			Damage
							( 
							idEntity *inflictor, idEntity *attacker, const idVec3 &dir,
							const char *damageDefName, const float damageScale, const int location,
							trace_t *collision = NULL
							);

							// use exitEntityNum to specify a teleport with private camera view and delayed exit
	virtual void			Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination );

	void					Kill( bool delayRespawn, bool nodamage );
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
	void					StartFxOnBone(const char *fx, const char *bone);

	renderView_t *			GetRenderView( void );
	void					CalculateRenderView( void );	// called every tic by player code
	void					CalculateFirstPersonView( void );

	void					DrawHUD( idUserInterface *hud );

	void					WeaponFireFeedback( const idDict *weaponDef );

	float					DefaultFov( void ) const;
	float					CalcFov( bool honorZoom );
	void					CalculateViewWeaponPos( idVec3 &origin, idMat3 &axis );
	idVec3					GetEyePosition( void ) const;
	void					GetViewPos( idVec3 &origin, idMat3 &axis ) const;
	void					OffsetThirdPersonView( float angle, float range, float height, bool clip );

	bool					Give( const char *statname, const char *value );
	bool					GiveItem( idItem *item );
	void					GiveItem( const char *name );
	void					GiveHealthPool( float amt );
	
	void					GivePDA( const char *pdaName, idDict *item );

	bool					GivePowerUp( int powerup, int time );
	float					PowerUpModifier( int type );

	int						SlotForWeapon( const char *weaponName );
	void					Reload( void );
	void					NextWeapon( void );
	void					NextBestWeapon( void );
	void					PrevWeapon( void );

	// returns FALSE if the weapon with the requested index could not be selected
	bool					SelectWeapon( int num, bool force );

	/**
	 * greebo: This returns the current weapon being focused at by the weapon inventory cursor.
	 *         Can return NULL, but should not in 99% of the cases.
	 */
	CInventoryWeaponItem*	getCurrentWeaponItem();

	void					DropWeapon( bool died ) ;
	void					StealWeapon( idPlayer *player );
	void					AddProjectilesFired( int count );
	void					AddProjectileHits( int count );
	void					SetLastHitTime( int time );
	void					LowerWeapon( void );
	void					RaiseWeapon( void );
	void					WeaponLoweringCallback( void );
	void					WeaponRisingCallback( void );
	void					RemoveWeapon( const char *weap );
	bool					CanShowWeaponViewmodel( void ) const;

	void					AddAIKill( void );
	void					SetSoulCubeProjectile( idProjectile *projectile );

	void					AdjustHeartRate( int target, float timeInSecs, float delay, bool force );
	void					SetCurrentHeartRate( void );
	int						GetBaseHeartRate( void );
	void					UpdateAir( void );
	
	/**
	* This updates the audiovisual effects when the player is underwater
	*/
	void					UpdateUnderWaterEffects();

	/**
	* greebo: Accessor methods for the airTicks member variable. 
	*/
	int						getAirTicks() const;
	void					setAirTicks(int airTicks);

	virtual bool			HandleSingleGuiCommand( idEntity *entityGui, idLexer *src );
	bool					GuiActive( void ) { return focusGUIent != NULL; }

	void					PerformImpulse( int impulse );

	/**
	* greebo: This gets called by the ButtonStateTracker helper class
	*		  whenever a key is released.
	*
	* @impulse: The impulse number
	* @holdTime: The time the button has been held down
	*/
	void					PerformKeyRelease(int impulse, int holdTime);

	/**
	* sparhawk: This gets called by the ButtonStateTracker helper class
	*			whenever a key is held.
	*
	* @impulse: The impulse number
	* @holdTime: The time the button has been held down
	*/
	void					PerformKeyRepeat(int impulse, int holdTime);

	void					Spectate( bool spectate );
	void					TogglePDA( void );
	void					ToggleScoreboard( void );

	// greebo: Toggles the objectives GUI
	void					ToggleObjectivesGUI();

	void					RouteGuiMouse( idUserInterface *gui );
	void					UpdateHud( void );

	// greebo: Checks if any messages are still pending.
	void					UpdateHUDMessages();

	const idDeclPDA *		GetPDA( void ) const;
	void					SetInfluenceFov( float fov );
	void					SetInfluenceView( const char *mtr, const char *skinname, float radius, idEntity *ent );
	void					SetInfluenceLevel( int level );
	int						GetInfluenceLevel( void ) { return influenceActive; };
	void					SetPrivateCameraView( idCamera *camView );
	idCamera *				GetPrivateCameraView( void ) const { return privateCameraView; }
	void					StartFxFov( float duration  );
	void					UpdateHudWeapon( bool flashWeapon = true );
	void					UpdateHudStats( idUserInterface *hud );
	void					UpdateHudAmmo( idUserInterface *hud );

	void					Event_StopAudioLog( void );
	void					StartAudioLog( void );
	void					StopAudioLog( void );
	void					ShowTip( const char *title, const char *tip, bool autoHide );
	void					HideTip( void );
	bool					IsTipVisible( void ) { return tipUp; };
	void					ShowObjective( const char *obj );
	void					HideObjective( void );

	virtual void			ClientPredictionThink( void );
	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );
	void					WritePlayerStateToSnapshot( idBitMsgDelta &msg ) const;
	void					ReadPlayerStateFromSnapshot( const idBitMsgDelta &msg );

	virtual bool			ServerReceiveEvent( int event, int time, const idBitMsg &msg );

	virtual bool			GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis );
	virtual bool			GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis );

	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );
	bool					IsReady( void );
	bool					IsRespawning( void );
	bool					IsInTeleport( void );

	idEntity				*GetInfluenceEntity( void ) { return influenceEntity; };
	const idMaterial		*GetInfluenceMaterial( void ) { return influenceMaterial; };
	float					GetInfluenceRadius( void ) { return influenceRadius; };

	// server side work for in/out of spectate. takes care of spawning it into the world as well
	void					ServerSpectate( bool spectate );
	// for very specific usage. != GetPhysics()
	idPhysics				*GetPlayerPhysics( void );
	void					TeleportDeath( int killer );
	void					SetLeader( bool lead );
	bool					IsLeader( void );

	void					UpdateSkinSetup( bool restart );

	bool					OnLadder( void ) const;

	virtual	void			UpdatePlayerIcons( void );
	virtual	void			DrawPlayerIcons( void );
	virtual	void			HidePlayerIcons( void );
	bool					NeedsIcon( void );

	bool					SelfSmooth( void );

	void					SetSelfSmooth( bool b );


	/**
	* Do the frobbing trace and bounds intersection to mark entities as frobable
	**/
	void					FrobCheck( void );

	// Gets called when the player hits the frob button.
	void					PerformFrob();

	/**
	 * greebo: Performs a frob action on the given entity. The above method
	 *         PerformFrob() without arguments redirects the call to this one.
	 *         This method might be invoked by scripts as well to simulate a 
	 *         frob action without having the player to hit any buttons.
	 */
	void					PerformFrob(idEntity* frobbed);

	/**
	 * AdjustLightgem will calculate how much the lightgem should light up.
	 * This function is obsolote now and replaced by a different version.
	 * TODO: Shall it be removed completely?
	 */
	void AdjustLightgem(void);

	/**
	 * GetHeadEntity will return the entity for the head of the playermodel
	 */
	idEntity *GetHeadEntity(void) { return head.GetEntity(); };

	/**
	* Update movement volumes: Reads the movement volume
	* modifiers from cvars (for now)
	**/
	void UpdateMoveVolumes( void );

	/**
	* Get the volume modifier for a given movement type
	**/
	float GetMovementVolMod( void );

	// Cycles to the next item in the inventory.
	void inventoryNextItem( void );
	// Cycles to the previous item in the inventory.
	void inventoryPrevItem( void );
	// Cycles to the next group in the inventory.
	void inventoryNextGroup( void );
	// Cycles to the previous group in the inventory.
	void inventoryPrevGroup( void );
	// Drops the currently held/selected item.
	void inventoryDropItem( void );

	// Uses the currently held/selected item.
	void inventoryUseItem();
	// Performs the inventory action for onButtonRelease
	void inventoryUseKeyRelease(int holdTime);

	// Uses a specific item, if it's in the same inventory.
	void inventoryUseItem(IMPULSE_STATE nState, idEntity* useEnt, int holdTime);

	// Changes the inventory selection to the item with the given name
	void inventoryChangeSelection(const idStr& name);

	// Sends appropriate messages/updates varaiables/etc after the cursor has changed. Returns if shifting should occur.
	void inventoryChangeSelection(idUserInterface *_hud, bool bUpdate = false, CInventoryItem *Prev = NULL);

	/**
	 * Overload the idEntity::AddToInventory method to catch weapon items.
	 */
	virtual CInventoryItem *AddToInventory(idEntity *ent, idUserInterface *_hud = NULL);

	/**
	 * greebo: Attempts to put the current grabber item back into the inventory.
	 *
	 * @returns: TRUE if an item was put back, FALSE if the grabber hands are empty.
	 */
	bool AddGrabberEntityToInventory();
	
	/**
	 * greebo: Returns the lightgem modifier value according to the currently selected inventory items
	 *         and other factors (like crouching). Returns a value between 0 and DARKMOD_LG_MAX. 
	 *         This value is added to the calculated lightgem value.
	 */
	int GetLightgemModifier();

	/// Am I a ranged threat to the given entity (or entities in general if target is NULL)?
	float			RangedThreatTo(idEntity* target);
	
	// greebo: Sends a message to the HUD (used for "Game Saved" and such).
	void SendHUDMessage(const idStr& text);

	void PrintDebugHUD(void);

	/**
	* greebo: Sets the time between health "pulses" if the healthPool > 0
	*
	* @newTimeInterval: the new value for the time interval
	* @factor: The factor that the time interval is being multiplied with after each pulse.
	*          This can be used to increase the time between pulses gradually.
	* @stepAmount: The amount of health to be taken from the healthpool at each step
	*/
	void			setHealthPoolTimeInterval(int newTimeInterval, float factor, int stepAmount);

private:
	jointHandle_t			hipJoint;
	jointHandle_t			chestJoint;
	jointHandle_t			headJoint;

	idPhysics_Player		physicsObj;			// player physics

	idList<aasLocation_t>	aasLocation;		// for AI tracking the player

	int						bobFoot;
	float					bobFrac;
	float					bobfracsin;
	int						bobCycle;			// for view bobbing and footstep generation
	float					xyspeed;
	int						stepUpTime;
	float					stepUpDelta;
	float					idealLegsYaw;
	float					legsYaw;
	bool					legsForward;
	float					oldViewYaw;
	idAngles				viewBobAngles;
	idVec3					viewBob;
	int						landChange;
	int						landTime;

	int						currentWeapon;
	int						idealWeapon;
	int						previousWeapon;
	int						weaponSwitchTime;
	bool					weaponEnabled;
	bool					showWeaponViewModel;

	const idDeclSkin *		skin;
	const idDeclSkin *		powerUpSkin;
	idStr					baseSkinName;

	int						numProjectilesFired;	// number of projectiles fired
	int						numProjectileHits;		// number of hits on mobs

	bool					airless;
	int						airTics;				// set to pm_airTics at start, drops in vacuum
	int						lastAirDamage;
	
	bool					underWaterEffectsActive; // True, if the under water effects are in charge
	int						underWaterGUIHandle;	 // The handle of the GUI underwater overlay

	bool					gibDeath;
	bool					gibsLaunched;
	idVec3					gibsDir;
	int						mInventoryOverlay;

	idInterpolate<float>	zoomFov;
	idInterpolate<float>	centerView;
	bool					fxFov;

	float					influenceFov;
	int						influenceActive;		// level of influence.. 1 == no gun or hud .. 2 == 1 + no movement
	idEntity *				influenceEntity;
	const idMaterial *		influenceMaterial;
	float					influenceRadius;
	const idDeclSkin *		influenceSkin;

	idCamera *				privateCameraView;

	/**
	* Location of the player's ears for sound rendering
	**/
	idVec3					m_ListenerLoc;

	/**
	* Location of the player's ear point when the player is leaning against
	* a door (i.e., a point on the other side of the door)
	**/
	idVec3					m_DoorListenLoc;

	/**
	* m_immobilization keeps track of sources of immobilization.
	* m_immobilizationCache caches the total immobilization so it
	* only gets recalculated when something is changed.
	**/
	idDict					m_immobilization;
	int						m_immobilizationCache;

	/**
	* m_hinderance keeps track of sources of hinderance. (slowing the player)
	* m_hinderanceCache caches the current hinderance level so it
	* only gets recalculated when something is changed.
	**/
	idDict					m_hinderance;
	float					m_hinderanceCache;

	/**
	 * greebo: This is the list of named lightgem modifier values. These can be accessed
	 *         via script events to allow several modifiers to be active at the same time.
	 *         To save for performance, the sum of these values is stored in m_LightgemModifier
	 */
	std::map<idStr, int>	m_LightgemModifierList;

	// greebo: The sum of the values in the above list
	int						m_LightgemModifier;

	static const int		NUM_LOGGED_VIEW_ANGLES = 64;		// for weapon turning angle offsets
	idAngles				loggedViewAngles[NUM_LOGGED_VIEW_ANGLES];	// [gameLocal.framenum&(LOGGED_VIEW_ANGLES-1)]
	static const int		NUM_LOGGED_ACCELS = 16;			// for weapon turning angle offsets
	loggedAccel_t			loggedAccel[NUM_LOGGED_ACCELS];	// [currentLoggedAccel & (NUM_LOGGED_ACCELS-1)]
	int						currentLoggedAccel;

	// if there is a focusGUIent, the attack button will be changed into mouse clicks
	idEntity *				focusGUIent;
	idUserInterface *		focusUI;				// focusGUIent->renderEntity.gui, gui2, or gui3
	idAI *					focusCharacter;
	int						talkCursor;				// show the state of the focusCharacter (0 == can't talk/dead, 1 == ready to talk, 2 == busy talking)
	int						focusTime;
	idAFEntity_Vehicle *	focusVehicle;
	idUserInterface *		cursor;
	
	// full screen guis track mouse movements directly
	int						oldMouseX;
	int						oldMouseY;

	idStr					pdaAudio;
	idStr					pdaVideo;
	idStr					pdaVideoWave;

	bool					tipUp;
	bool					objectiveUp;

	int						lastDamageDef;
	idVec3					lastDamageDir;
	int						lastDamageLocation;
	int						smoothedFrame;
	bool					smoothedOriginUpdated;
	idVec3					smoothedOrigin;
	idAngles				smoothedAngles;

	// mp
	bool					ready;					// from userInfo
	bool					respawning;				// set to true while in SpawnToPoint for telefrag checks
	bool					leader;					// for sudden death situations
	int						lastSpectateChange;
	int						lastTeleFX;
	unsigned int			lastSnapshotSequence;	// track state hitches on clients
	bool					weaponCatchup;			// raise up the weapon silently ( state catchups )
	int						MPAim;					// player num in aim
	int						lastMPAim;
	int						lastMPAimTime;			// last time the aim changed
	int						MPAimFadeTime;			// for GUI fade
	bool					MPAimHighlight;
	bool					isTelefragged;			// proper obituaries

	idPlayerIcon			playerIcon;

	bool					selfSmooth;


	void					LookAtKiller( idEntity *inflictor, idEntity *attacker );

	void					StopFiring( void );
	void					FireWeapon( void );
	void					Weapon_Combat( void );
	void					Weapon_NPC( void );
	void					Weapon_GUI( void );
	void					UpdateWeapon( void );
	void					UpdateSpectating( void );
	void					SpectateFreeFly( bool force );	// ignore the timeout to force when followed spec is no longer valid
	void					SpectateCycle( void );
	idAngles				GunTurningOffset( void );
	idVec3					GunAcceleratingOffset( void );

	void					UseObjects( void );
	void					BobCycle( const idVec3 &pushVelocity );
	void					UpdateViewAngles( void );
	void					EvaluateControls( void );
	void					AdjustSpeed( void );
	void					AdjustBodyAngles( void );
	void					InitAASLocation( void );
	void					SetAASLocation( void );
	void					Move( void );
	void					UpdatePowerUps( void );
	void					UpdateDeathSkin( bool state_hitch );
	void					SetSpectateOrigin( void );

	void					ClearFocus( void );
	void					UpdateFocus( void );
	void					UpdateLocation( void );
	idUserInterface *		ActiveGui( void );
	void					UpdatePDAInfo( bool updatePDASel );
	int						AddGuiPDAData( const declType_t dataType, const char *listName, const idDeclPDA *src, idUserInterface *gui );
	void					ExtractEmailInfo( const idStr &email, const char *scan, idStr &out );

	void					UseVehicle( void );

	void					Event_GetButtons( void );
	void					Event_GetMove( void );
	void					Event_GetViewAngles( void );
	void					Event_StopFxFov( void );
	void					Event_EnableWeapon( void );
	void					Event_DisableWeapon( void );
	void					Event_GetCurrentWeapon( void );
	void					Event_GetPreviousWeapon( void );
	void					Event_SelectWeapon( const char *weaponName );
	void					Event_GetWeaponEntity( void );
	void					Event_OpenPDA( void );
	void					Event_PDAAvailable( void );
	void					Event_InPDA( void );
	void					Event_ExitTeleporter( void );
	void					Event_HideTip( void );
	void					Event_LevelTrigger( void );
	void					Event_Gibbed( void );
	void					Event_GetIdealWeapon( void );
	void					Event_RopeRemovalCleanup( idEntity *RopeEnt );


/**
* TDM Events
**/
	void					Event_GetEyePos( void );
	void					Event_SetImmobilization( const char *source, int type );
	void					Event_GetImmobilization( const char *source );
	void					Event_GetNextImmobilization( const char *prefix, const char *lastMatch );
	void					Event_SetHinderance( const char *source, float mCap, float aCap );
	void					Event_GetHinderance( const char *source );
	void					Event_GetNextHinderance( const char *prefix, const char *lastMatch );

	void					Event_SetGui( int handle, const char *guiFile );
	void					Event_GetInventoryOverlay(void);

	void					Event_PlayStartSound( void );
	void					Event_MissionFailed( void );
	void					Event_LoadDeathMenu( void );

	void					Event_HoldEntity( idEntity *ent );
	void					Event_HeldEntity( void );

	/**
	 * greebo: Sets the named lightgem modifier to a particular value. 
	 * Setting the modifier to 0 removes it from the internal list.
	 */
	void					Event_SetLightgemModifier(const char* modifierName, int amount);

	/**
	 * greebo: Reads the lightgem modifier setting from the worldspawn entity (defaults to 0).
	 */
	void					Event_ReadLightgemModifierFromWorldspawn();

/**
* NOTE: The following objective functions all take the "user" objective indices
* That is, the indices start at 1 instead of 0
*
* If the objective/component for that index was not found
* The getters return -1 for completion state and FALSE for component state
**/
	void					Event_SetObjectiveState( int ObjIndex, int State );
	void					Event_GetObjectiveState( int ObjIndex );
	void					Event_SetObjectiveComp( int ObjIndex, int CompIndex, int bState );
	void					Event_GetObjectiveComp( int ObjIndex, int CompIndex );
	void					Event_ObjectiveUnlatch( int ObjIndex );
	void					Event_ObjectiveComponentUnlatch( int ObjIndex, int CompIndex );
	void					Event_SetObjectiveVisible( int ObjIndex, bool bVal );
	void					Event_SetObjectiveOptional( int ObjIndex, bool bVal );
	void					Event_SetObjectiveOngoing( int ObjIndex, bool bVal );
	void					Event_SetObjectiveEnabling( int ObjIndex, const char *strIn );

	/**
	* greebo: This scriptevent routes the call to the member method "GiveHealthPool".
	*/
	void					Event_GiveHealthPool( float amount );

	/**
	 * greebo: These scriptevents handle the player zoom in/out behaviour.
	 */
	void					Event_StartZoom(float duration, float startFOV, float endFOV);
	void					Event_EndZoom(float duration);
	void					Event_ResetZoom();
	void					Event_GetFov();
};

ID_INLINE bool idPlayer::IsReady( void ) {
	return ready || forcedReady;
}

ID_INLINE bool idPlayer::IsRespawning( void ) {
	return respawning;
}

ID_INLINE idPhysics* idPlayer::GetPlayerPhysics( void ) {
	return &physicsObj;
}

ID_INLINE bool idPlayer::IsInTeleport( void ) {
	return ( teleportEntity.GetEntity() != NULL );
}

ID_INLINE void idPlayer::SetLeader( bool lead ) {
	leader = lead;
}

ID_INLINE bool idPlayer::IsLeader( void ) {
	return leader;
}

ID_INLINE bool idPlayer::SelfSmooth( void ) {
	return selfSmooth;
}

ID_INLINE void idPlayer::SetSelfSmooth( bool b ) {
	selfSmooth = b;
}

#endif /* !__GAME_PLAYER_H__ */

