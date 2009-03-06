/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

// Copyright (C) 2006 Chris Sarantos <csarantos@gmail.com>

// TODO: Right now, AI_FindBody stats does not track the stat of team of body found
// AND team of AI finding body.  It only tracks team of AI finding body.
// Whether an AI reacts to a body should probably be determined by the AI, for example,
// average guards wouldn't react to a dead rat, but an elite might.
// If that's the case, we don't have to track both variables in the stats, since
// AI will only call FindBody if they react to that type of body.

#ifndef MISSIONDATA_H
#define MISSIONDATA_H

#include "../idlib/precompiled.h"
#include "DarkModGlobals.h"

// Maximum array sizes:
#define MAX_TEAMS 64
#define MAX_TYPES 16
#define MAX_AICOMP 16
#define MAX_ALERTLEVELS 16

/**
* Objective completion states
**/
typedef enum
{
	STATE_INCOMPLETE,
	STATE_COMPLETE,
	STATE_INVALID, 
	STATE_FAILED
} EObjCompletionState;
// NOTE: STATE_INVALID may also be used for initially deactivating objectives, 
// then activating later by setting STATE_INCOMPLETE

#include "EMissionResult.h"

/**
* Objective component specification types
**/
typedef enum
{
// The following apply to both AIs and items
	SPEC_NONE,
	SPEC_NAME,
	SPEC_OVERALL,
	SPEC_GROUP,			// for inventory items, info_location groups, etc
	SPEC_CLASSNAME,		// soft/scripting classname
	SPEC_SPAWNCLASS,	// hard / SDK classname

// Specifically for AI:
	SPEC_AI_TYPE,
	SPEC_AI_TEAM,
	SPEC_AI_INNOCENCE,
	SPEC_COUNT					// Dummy entry should not be used for anything
} ESpecificationMethod;

/**
* Objective component action types
* TODO: Move to game_local.h so that it can be used in external calls
* NOTE: Any change to these must be kept up to date in CompTypeNames hash, defined in MissionData.cpp
**/
typedef enum
{
// AI components - MUST BE KEPT TOGETHER IN THE ENUM because later these enums are used as an array index
// COMP_KILL must be kept as the first one
	COMP_KILL, // also includes non-living things being destroyed
	COMP_KO,
	COMP_AI_FIND_ITEM,
	COMP_AI_FIND_BODY,
	COMP_ALERT,
// END AI components that must be kept together
	COMP_DESTROY,		// Destroy an inanimate object
	COMP_ITEM,			// Add inventory item or imaginary loot (find object X)
	COMP_PICKPOCKET,	// Take inventory item from conscious AI
	COMP_LOCATION,		// Item X is at location Y
	COMP_CUSTOM_ASYNC,	// asynchronously updated custom objective (updated by mapper from script)

// The following are special clocked components, updated in CMissionData::UpdateObjectives
	COMP_CUSTOM_CLOCKED,
	COMP_INFO_LOCATION, // like location, but uses existing info_location areas instead of an info_objectivelocation entity
	COMP_DISTANCE,		// distance from origin of ent X to that of ent Y
	COMP_COUNT			// Dummy entry to yield the number of valid types
} EComponentType;

// TODO: move to game_local.h?
typedef struct SObjEntParms_s
{
	idStr	name;
	idStr	group; // inventory group for items, e.g., loot group "gems"
	idStr	classname;
	idStr	spawnclass;

	idVec3	origin;

// AI data:
	int		team;
	int		type;
	int		innocence;

/**
* Numerical value, filled by callbacks in some cases for things that are kept
* track of externally (for example, number of inventory items, overall loot, etc)
**/
	int value; // should default to 1
	int valueSuperGroup; // Just used to pass overall loot for now

	bool bIsAI;
	bool bWhileAirborne; // a must-have :)

	SObjEntParms_s( void ) { Clear(); }
	~SObjEntParms_s( void ) { Clear(); }

	/**
	* Initialize the struct to default values
	**/
	void Clear( void )
	{
		name.Clear();
		group.Clear();
		classname.Clear();
		spawnclass.Clear();

		origin = vec3_zero;
		team = -1;
		type = -1;
		innocence = -1;

		value = 1;
		valueSuperGroup = 1;
		bIsAI = false;
		bWhileAirborne = false;
	}
} SObjEntParms;

/**
* Structure for parsing boolean logic
**/
typedef struct SBoolParseNode_s
{
	int Ident;
	bool bNotted; // set to true if this node is NOTed

	idList< idList< SBoolParseNode_s > > Cols; // list of columns, each can contain a different number of rows

	// Link back to previous node this one branched off from
	SBoolParseNode_s *PrevNode;

	// matrix coordinates of this node within the matrix of the previous node
	int PrevCol; 
	int PrevRow;

	// Functions:

	SBoolParseNode_s( void ) { Clear(); }
	~SBoolParseNode_s( void ) { Clear(); }
	bool IsEmpty( void ) { return (Cols.Num() == 0 && Ident == -1); }

	/**
	* Clear the parse node
	**/
	void Clear( void )
	{
		Ident = -1;
		PrevCol = -1;
		PrevRow = -1;

		bNotted = false;
		Cols.Clear();
		PrevNode = NULL;
	}
} SBoolParseNode;

class CObjectiveComponent
{
public:
	friend class CMissionData;
	friend class CObjective;

	CObjectiveComponent( void );
	virtual ~CObjectiveComponent( void );

	void Save( idSaveGame *savefile ) const;
	void Restore( idRestoreGame *savefile );

	/**
	* Update the state of the objective component.  
	* Returns true if the state has changed as of this call
	**/
	bool SetState( bool bState );

public:
	/**
	* Index of this component in the form of [objective num, component num]
	* NOTE: This index is that from the external scripting.
	* So it starts at 1, not at zero.
	**/
	int m_Index[2]; 

protected:

/**
* Set to true if the FM author has NOTted this component
**/
	bool m_bNotted;
	
	EComponentType m_Type;

// This could be made more general into a list, but I can't think of any component
// types that would require more than 2 items to match.  More complicated logic
// can be constructed out of multiple components.
	ESpecificationMethod m_SpecMethod[2];

/**
* Values of the specifier to match, e.g., if specmethod is group, specvalue is "beast"
* Could be either an int or a string depending on spec type, so make room for both.
**/
	idStr m_SpecVal[2];

	/**
	* Current component state (true/false)
	**/
	bool		m_bState;

	/**
	* Current count of the number of times this event
	* happened and the specifiers were matched.
	* Some objective components use this, others rely on
	* other counters, like overall stats or the inventory
	**/
	int			m_EventCount;

	/**
	* Set to true if this component is only satisfied when the player performs the action
	**/
	bool		m_bPlayerResponsibleOnly;

	/**
	* Whether the irreversible component has latched into a state
	**/
	bool		m_bLatched;

	idStrList	m_Args;

// Only used by clocked objectives:
	int			m_ClockInterval; // milliseconds
	
	int			m_TimeStamp;

	/**
	* Whether the objective component latches after it changes once
	* Default is reversible.
	**/
	bool m_bReversible;

}; // CObjectiveComponent

/**
* Abstract class for storing objective data
* This class contains all the objective components
**/

class CObjective
{

public:
	friend class CObjectiveComponent;
	friend class CMissionData;

	CObjective();
	virtual ~CObjective();

	void Save( idSaveGame *savefile ) const;
	void Restore( idRestoreGame *savefile );

	void Clear();

	/**
	* Evaluate the boolean relationships for objective failure and success
	**/
	bool CheckFailure();
	bool CheckSuccess();

	/**
	* Parse m_SuccessLogicStr and m_FailureLogicStr into boolean evaluation
	* matrices to be evaluated by CheckFailure and CheckSuccess.
	* Returns false if the logic parse failed
	*
	* This should be run after CMissionData parsing sets those two strings
	**/
	bool ParseLogicStrs();

public:
	/** 
	* Text description of the objective in the objectives GUI
	**/
	idStr m_text;

	/** 
	* Set to false if an objective is optional
	**/
	bool m_bMandatory;

	/**
	* Sets whether the objective is shown in the objectives screen
	**/
	bool m_bVisible;
	
	/**
	* True if an objective is ongoing throughout the mission.
	* Will not be checked off as complete until the mission is over
	**/
	bool m_bOngoing;

	/**
	* True if this objective applies to the current skill level. Otherwise
	* the objective can be ignored.
	**/
	bool m_bApplies;

protected:

	/**
	* Integer index of this objective in the array
	**/
	int	m_ObjNum;

	/**
	* Handle for the FM author to refer to this objective (Not Yet Implemented)
	**/
	int m_handle;

	/**
	* Completion state.  Either COMP_INCOMPLETE, COMP_COMPLETE, COMP_FAILED or COMP_INVALID
	**/
	EObjCompletionState	m_state;

	/**
	* Set to true if one of the components changed this frame.  Test resets it to false.
	*/
	bool m_bNeedsUpdate;

	/**
	* Whether the objective may change state again once it initially changes to FAILED or SUCCESSFUL
	* Default is reversible.
	**/
	bool m_bReversible;

	/**
	* Set to true if the objective is irreversible and has latched into a state
	**/
	bool m_bLatched;

	/**
	* List of objective components (steal this, kill that, etc)
	**/
	idList<CObjectiveComponent> m_Components;

	/**
	* Other objectives that must be completed prior to the completion of this objective
	**/
	idList<int> m_EnablingObjs;

	/**
	 * greebo: These define the names of entities which should be triggered
	 * as soon as the objective completes or fails.
	 */
	idStr m_CompletionTarget;
	idStr m_FailureTarget;

	/**
	* String storing the script to call when this objective is completed
	* (optional)
	**/
	idStr m_CompletionScript;

	/**
	* String storing the script to call when this objective is failed
	* (optional)
	**/
	idStr m_FailureScript;
	
	/**
	* Success and failure logic strings.  Used to reload the parse matrix on save/restore
	**/
	idStr m_SuccessLogicStr;
	idStr m_FailureLogicStr;

	/**
	* Success and failure boolean parsing matrices
	**/
	SBoolParseNode m_SuccessLogic;
	SBoolParseNode m_FailureLogic;
};

struct SStat
{
	int Overall;
	int ByTeam[ MAX_TEAMS ];
	int ByType[ MAX_TYPES ];
	int ByInnocence[2];
	int WhileAirborne;

	SStat() 
	{
		Clear();
	}

	void Clear() 
	{
		Overall = 0;

		for (int i = 0; i < MAX_TEAMS; i++)
		{
			ByTeam[i] = 0;
		}

		for (int i = 0; i < MAX_TYPES; i++)
		{
			ByType[i] = 0;
		}

		ByInnocence[0] = 0;
		ByInnocence[1] = 0;
		WhileAirborne = 0;
	}
};

/**
* Mission stats: Keep track of everything except for loot groups, which are tracked by the inventory
**/
struct SMissionStats
{
	// AI Stats:
	SStat AIStats[ MAX_AICOMP ];
	
	SStat AIAlerts[ MAX_ALERTLEVELS ];

	int DamageDealt;
	int DamageReceived;
	int HealthReceived;
	int PocketsPicked;

	// Item stats are handled by the inventory, not here, 
	// Might need this for copying over to career stats though
	int FoundLoot;

	// greebo: This is the available amount of loot in the mission
	int TotalLootInMission;

	SMissionStats() 
	{
		Clear();
	}

	void Clear()
	{
		for (int i = 0; i < MAX_AICOMP; i++)
		{
			AIStats[i].Clear();
		}

		for (int i = 0; i < MAX_ALERTLEVELS; i++)
		{
			AIAlerts[i].Clear();
		}

		DamageDealt = 0;
		DamageReceived = 0;
		HealthReceived = 0;
		PocketsPicked = 0;
		FoundLoot = 0;
		TotalLootInMission = 0;
	}
};

/**
* CMissionData handles the tasks of maintaining stats and objective completion status
* in response to ingame events.
*
* Also handles the task of parsing objectives written by FM author
**/

class CMissionData 
{
public:
	friend class CObjective;

	CMissionData();
	virtual ~CMissionData();

	void Clear();

	void Save( idSaveGame *savefile ) const;
	void Restore( idRestoreGame *savefile );

	/**
	* Update objectives if they need it
	* Called each frame by idPlayer::Think, does nothing if no objectives need updating
	**/
	void UpdateObjectives( void );

	/**
	* Set the completion state of an objective.  Called both externally and internally.
	* NOTE: Uses the "internal" index number, so subtract the index by 1 if calling it with "user" index
	**/
	void SetCompletionState( int ObjIndex, int State );

	/**
	* Get completion state.  Uses "internal" index (starts at 0)
	**/
	int GetCompletionState( int ObjIndex );

	/**
	* Get component state.  Uses "internal" index (starts at 0)
	**/
	bool GetComponentState( int ObjIndex, int CompIndex );

	/**
	* Sets a given component state.  
	* Externally called version: Checks and reports index validity
	* Uses "user" index (starts at 1 instead of 0)
	* Calls internal SetComponentState.
	**/
	void SetComponentState_Ext( int ObjIndex, int CompIndex, bool bState );

	/**
	* Unlatch an irreversible objective (used by scripting)
	* Uses internal index (starts at 0)
	**/
	void UnlatchObjective( int ObjIndex );

	/**
	* Unlatch an irreversible component (used by scripting)
	* Uses internal indeces(starts at 0)
	**/
	void UnlatchObjectiveComp( int ObjIndex, int CompIndex );


/**
* Set whether an objective shows up in the player's objectives screen
**/
	void Event_SetObjVisible( int ObjIndex, bool bVal );
// self explanatory
	void Event_SetObjMandatory( int ObjIndex, bool bVal );
	void Event_SetObjOngoing( int ObjIndex, bool bVal );
/**
* Replace an objective's list of enabling components with a new one
* Takes a string list of space-delimited ints and parses it in.
**/
	void Event_SetObjEnabling( int ObjIndex, idStr StrIn );
//	void Event_SetObjDifficulty( int ObjIndex, int value );

/**
* Getters for the mission stats.  Takes an objective component event type,
* index for the category (for example, the index would be the team int if 
* you are calling GetStatByTeam)
* 
* The AlertLevel must be specified if you are getting alert stats, but otherwise
* is optional.
**/
	int GetStatByTeam( EComponentType CompType, int index, int AlertLevel = 0 );
	int GetStatByType( EComponentType CompType, int index, int AlertLevel = 0 );
	int GetStatByInnocence( EComponentType CompType, int index, int AlertLevel = 0 );
/**
* The following stat functions don't need an index var, since there is only one of them tracked
**/
	int GetStatOverall( EComponentType CompType, int AlertLevel = 0 );
	int GetStatAirborne( EComponentType CompType, int AlertLevel = 0);
	int GetDamageDealt( void );
	int GetDamageReceived( void );
	int GetHealthReceived();

// Callback functions:

	/**
	* Called by external callbacks when events occur that could effect objectives.
	* This is the most general call and requires passing in filled-in SObjEntParms objects
	* bBoolArg is a multifunctional bool argument
	* 
	* For AI, bBoolArg represents whether the player is responsible for the action
	* 
	* For items and locations, bBoolArg is true when the ent is entering the location/inventory
	* and false when leaving the location or being dropped from the inventory.
	*
	* This is the most generic version, will be called by the inventory after it puts "value" into the parms
	**/
	void MissionEvent
		( 
		EComponentType CompType, 					 
		SObjEntParms *EntDat1, 				 
		SObjEntParms *EntDat2,
		bool bBoolArg = false
		);

	void MissionEvent
		( 
		EComponentType CompType, 					 
		SObjEntParms *EntDat1, 				 
		bool bBoolArg = false
		)
	{ MissionEvent( CompType, EntDat1, NULL, bBoolArg ); };

	/**
	* Overloaded MissionEvent with entities instead of parms 
	* (for real ents as opposed to fake inventory items)
	* Used by AI events, locations, etc
	**/
	void MissionEvent
		( 
		EComponentType CompType, 					 
		idEntity *Ent1, 				 
		idEntity *Ent2,
		bool bBoolArg,
		bool bWhileAirborne = false
		);

	void MissionEvent
		( 
		EComponentType CompType, 					 
		idEntity *Ent1, 				 
		bool bBoolArg,
		bool bWhileAirborne = false
		)
	{ MissionEvent( CompType, Ent1, NULL, bBoolArg, bWhileAirborne ); }

	/**
	* Fill the SObjEntParms data from an entity.  Does not fill in value and superGroupValue
	**/
	void FillParmsData( idEntity *ent, SObjEntParms *parms );
	
	/**
	* Called when the player takes damage.  Used for damage stats
	**/
	void PlayerDamaged( int DamageAmount );

	/**
	* Called when the player damages AI.  Used for damage stats.
	**/
	void AIDamagedByPlayer( int DamageAmount );

	/**
	 * greebo: Gets called if the player gets healed by something
	 */
	void HealthReceivedByPlayer(int amount);

	/**
	* Called by the inventory when a player picks up or drops an item.
	*
	* Entity is the ent being picked up, bPicked up is true if picked up, false if dropped
	* TypeName should be set to the name of the item's type.  E.g., for loot, loot_gems, loot_gold...
	*
	* For loot items, ItemName is automatically set to "loot_gems", "loot_gold", etc.
	* For non-loot items, ItemName is the name of the inventory item (e.g., healthpotion)
	* NOTE: ItemName gets tested when using the "group" specifier.
	*
	* For non-loot items, value should be set to the number of stacked items if non-unique, or 1 if unique
	* For loot items, value should be the current _total_ value of that loot group.
	* For loot items, OverallVal should be set to the overall loot count
	* Ent is the actual entity picked up/dropped.  If NULL, default entity properties will be used.
	**/
	void InventoryCallback( idEntity *ent, idStr ItemName, int value, int OverallVal = 1, bool bPickedUp = true );

	/**
	* Called when AI are alerted
	* The alert value is the alert state, e.g. state 0 = no significant alert, 5 = combat
	**/
	void AlertCallback(idEntity *Alerted, idEntity *Alerter, int AlertVal);

	int GetFoundLoot( void );
	void ChangeFoundLoot( int amount );

	/**
	 * greebo: This adds a given amount of loot to the total amount available in the mission.
	 * The total loot value is interesting at the end of the mission where the player wants to
	 * see how much loot he/she missed.
	 */
	void AddMissionLoot(int lootType, int amount);

	/**
	* Parse the objective data on an entity and add it to the objectives system
	* Called by CTarget_AddObjectives
	* This may be done during gameplay to add new objectives
	* Returns the index of the LAST objective added, for later addressing
	**/
	int AddObjsFromEnt( idEntity *ent );
	int AddObjsFromDict(const idDict& dict);

	/**
	 * greebo: Load the objectives directly from the given map file.
	 * This is called by the main menu SDK code.
	 **/
	void LoadDirectlyFromMapFile(idMapFile* mapFile);

	/**
	 * greebo: Loads the named map file. After this call, it's ensured
	 * that the m_mapFile member holds the named map. No action is taken
	 * when the member already held the map with that name to avoid
	 * loading the same data twice.
	 *
	 * Note: the caller must not free the map using "delete".
	 */
	idMapFile* LoadMap(const idStr& mapFileName);

	/**
	 * greebo: This updates the given GUI with the current
	 *         missiondata (objectives state). Called by gameLocal on demand of the main menu.
	 *
	 * @ui: the GUI to be updated.
	 */
	void UpdateGUIState(idUserInterface* ui);

	/**
	 * greebo: Gets called when the main menu is active and a "cmd" string is pending.
	 *         This watches out for any objectives-related commands and interprets them.
	 */
	void HandleMainMenuCommands(const idStr& cmd, idUserInterface* gui);

	// greebo: Call this to trigger an objective update next time the main menu is shown
	void ClearGUIState();

	/**
	 * greebo: Updates the statistics in the given GUI.
	 *
	 * @gui: The GUI to be updated.
	 * @listDefName: the name of the listDef in the GUI, which should be filled with the values.
	 */
	void UpdateStatisticsGUI(idUserInterface* gui, const idStr& listDefName);

	

// Events
	/**
	* The following are called internally when an objective completes or fails
	**/
	void Event_ObjectiveComplete( int ObjIndex );
	void Event_ObjectiveFailed( int ObjIndex );

	// A new objective has been added or a previously hidden one has been shown
	void Event_NewObjective();

	void Event_MissionComplete( void );
	void Event_MissionFailed( void );

	// greebo: This is the general mission end event, regardless whether failed or completed.
	void Event_MissionEnd();

protected:
	/**
	 * greebo: Tells the missiondata class to remember playerteam. As mission statistics
	 * are populated after the map has been closed, there's no way to know the actual
	 * team of the player, hence this function is used to let the missiondata know.
	 */
	void SetPlayerTeam(int team);

	/**
	* Sets a given component state.  
	* Internally used version: Doesn't check/report index validity
	* Uses "internal" index (starts at 0 instead of 1)
	**/
	void SetComponentState( int ObjIndex, int CompIndex, bool bState );

	/**
	* Set component state when indexed by a pointer to a component
	**/
	void SetComponentState( CObjectiveComponent *pComp, bool bState );

	/**
	* Do the numerical comparison
	* Get the number to compare, either provided or from stats, depending on component type
	* e.g., for AI events, get the number from stats.
	* For item events, the number will be passed in by the callback from the inventory.
	**/
	bool	EvaluateObjective
		(
			CObjectiveComponent *pComp,
			SObjEntParms *EntDat1,
			SObjEntParms *EntDat2,
			bool bBoolArg
		);

	/**
	* Reads the specification type from the objective component,
	* then tests if that specification on EntDat matches that in the component.
	* The index determines which specification of the component to check,
	* since some components have two objectives. ind=0 => first, ind=1 => second
	* A component has a max of two specificaton checks, so ind should never be > 1.
	**/
	bool	MatchSpec( CObjectiveComponent *pComp, SObjEntParms *EntDat, int ind );

	/**
	* Internal function used to check success or failure logic
	* 
	* If bObjComp is true, we are evaluating at the objective level 
	* and ObjNum need not be specified
	*
	* If bObjComp is false, we are evaluating at the component level, 
	* and ObjNum should be specified
	**/
	bool EvalBoolLogic( SBoolParseNode *input, bool bObjComp, int ObjNum = 0 );

	/**
	* Internal function used to parse boolean logic into a conditional matrix
	**/
	bool ParseLogicStr( idStr *input, SBoolParseNode *output );

	/**
	* Parse the success and failure logic strings if they are non-empty
	* Returns true if parsing succeeded
	**/
	bool ParseLogicStrs( void );

protected:
	/**
	* Set to true if any of the objective states have changed and objectives need updating
	**/
	bool m_bObjsNeedUpdate;

	/**
	* List of current objectives
	**/
	idList<CObjective> m_Objectives;

	/**
	* Pointers to objective components that are centrally clocked
	* Components that fall under this domain are:
	* CUSTOM_CLOCKED, DISTANCE, and TIMER
	**/
	idList<CObjectiveComponent *> m_ClockedComponents;

	/**
	* Object holding all mission stats relating to AI, damage to player and AI
	* Loot stats are maintained by the inventory
	* TODO: Also put in a persistent stats object 
	**/
	SMissionStats m_Stats;

	/**
	* Hash indices to store string->enum conversions for objective component type and
	* specification method type.
	* Used for parsing objectives from spawnargs.
	**/
	idHashIndex m_CompTypeHash;
	idHashIndex m_SpecTypeHash;

	/**
	* Success and failure logic strings for overall mission.  Used to reload the parse matrix on save/restore
	**/
	idStr m_SuccessLogicStr;
	idStr m_FailureLogicStr;

	/**
	* Success and failure boolean parsing matrices for overall mission
	**/
	SBoolParseNode m_SuccessLogic;
	SBoolParseNode m_FailureLogic;

	// true if the main menu GUI is up to date
	bool m_MissionDataLoadedIntoGUI; 

	// parsed map for use by Difficulty screen
	idMapFile*	m_mapFile;

	// The team number of the player, needed for the statistics GUI
	int			m_PlayerTeam;

	// This gets read out right at "mission complete" time, is 0 before
	unsigned int	m_TotalGamePlayTime;

}; // CMissionData

// Helper entity for objective locations
class CObjectiveLocation : public idEntity
{
public:
	CLASS_PROTOTYPE( CObjectiveLocation );
	
	CObjectiveLocation( void );

	void Think( void );
	void Spawn( void );

	void Save( idSaveGame *savefile ) const;
	void Restore( idRestoreGame *savefile );

protected:
	/**
	* Clock interval [seconds]
	**/
	int m_Interval;

	int m_TimeStamp;

	/**
	* List of entity names that intersected bounds in previous clock tick
	**/
	idList< idEntityPtr<idEntity> >	m_EntsInBounds;
	/**
	* Objective system: Location's objective group name for objective checks
	**/
	idStr		m_ObjectiveGroup;

private:
	idClipModel *		clipModel;


}; // CObjectiveLocation

#endif // MISSIONDATA_H
