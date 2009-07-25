/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef __AI_MEMORY_H__
#define __AI_MEMORY_H__

#include "../idlib/precompiled.h"
#include "../BinaryFrobMover.h"
#include "../FrobDoor.h"
#include "DoorInfo.h"
#include "../../game/ai/ai.h"

namespace ai
{

#define AIUSE_WEAPON			"AIUSE_WEAPON"
#define AIUSE_LIGHTSOURCE		"AIUSE_LIGHTSOURCE"
#define AIUSE_BLOOD_EVIDENCE	"AIUSE_BLOOD_EVIDENCE"
#define AIUSE_SEAT				"AIUSE_SEAT"
#define AIUSE_COOK				"AIUSE_COOK"
#define AIUSE_EAT				"AIUSE_EAT"
#define AIUSE_PET				"AIUSE_PET"
#define AIUSE_MONSTER			"AIUSE_MONSTER"  // a random or caged monster, not a pet
#define AIUSE_UNDEAD			"AIUSE_UNDEAD" // An undead creature
#define AIUSE_CATTLE			"AIUSE_CATTLE"
#define AIUSE_PERSON			"AIUSE_PERSON"
#define AIUSE_PEST				"AIUSE_PEST"
#define AIUSE_DRINK			"AIUSE_DRINK"
#define AIUSE_DOOR				"AIUSE_DOOR"
#define AIUSE_ELEVATOR			"AIUSE_ELEVATOR"
#define AIUSE_MISSING_ITEM_MARKER "AIUSE_MISSING_ITEM_MARKER"
#define AIUSE_BROKEN_ITEM		"AIUSE_BROKEN_ITEM"

//----------------------------------------------------------------------------------------
// The following key and values are used for identifying types of lights
#define AIUSE_LIGHTTYPE_KEY		"lightType"
#define AIUSE_LIGHTTYPE_TORCH	"AIUSE_LIGHTTYPE_TORCH"
#define AIUSE_LIGHTTYPE_GASLAMP	 "AIUSE_LIGHTTYPE_GASLAMP"
#define AIUSE_LIGHTTYPE_ELECTRIC "AIUSE_LIGHTTYPE_ELECTRIC"
#define AIUSE_LIGHTTYPE_MAGIC	 "AIUSE_LIGHTTYPE_MAGIC"
#define AIUSE_LIGHTTYPE_AMBIENT	 "AIUSE_LIGHTTYPE_AMBIENT"

//----------------------------------------------------------------------------------------
// The following key is used to identify the name of the switch entity used to turn on
// a AIUSE_LIGHTTYPE_ELECTRIC light.
#define AIUSE_LIGHTSWITCH_NAME_KEY	"switchName"

//----------------------------------------------------------------------------------------
// The following defines a key that should be non-0 if the device should be on
#define AIUSE_SHOULDBEON_KEY		"shouldBeOn"


// SZ: Minimum count evidence of intruders to turn on all lights encountered
#define MIN_EVIDENCE_OF_INTRUDERS_TO_TURN_ON_ALL_LIGHTS 5
// angua: The AI starts searching after encountering a switched off light 
// only if it is already suspicious
#define MIN_EVIDENCE_OF_INTRUDERS_TO_SEARCH_ON_LIGHT_OFF 3
// SZ: Minimum count of evidence of intruders to communicate suspicion to others
#define MIN_EVIDENCE_OF_INTRUDERS_TO_COMMUNICATE_SUSPICION 3

// SZ: Someone hearing a distress call won't bother to shout that it is coming to their assisitance unless
// it is at least this far away. This is to simulate more natural human behaivior.
#define MIN_DISTANCE_TO_ISSUER_TO_SHOUT_COMING_TO_ASSISTANCE 200

// Considered cause radius around a tactile event
#define TACTILE_ALERT_RADIUS 10.0f
#define TACTILE_SEARCH_VOLUME idVec3(40,40,40)

// Considered cause radius around a visual event
#define VISUAL_ALERT_RADIUS 25.0f
#define VISUAL_SEARCH_VOLUME idVec3(100,100,100)

// Considered cause radius around an audio event
#define AUDIO_ALERT_RADIUS 50.0f
#define AUDIO_ALERT_FUZZINESS 100.0f
#define AUDIO_SEARCH_VOLUME idVec3(300,300,200)

// Area searched around last sighting after losing an enemy
#define LOST_ENEMY_ALERT_RADIUS 200.0
#define LOST_ENEMY_SEARCH_VOLUME idVec3(200, 200, 200.0)

enum EAlertClass 
{
	EAlertNone,
	EAlertVisual,
	EAlertTactile,
	EAlertAudio,
	EAlertClassCount
};

enum EAlertType
{
	EAlertTypeNone,
	EAlertTypeSuspicious,
	EAlertTypeEnemy,
	EAlertTypeWeapon,
	EAlertTypeDeadPerson,
	EAlertTypeUnconsciousPerson,
	EAlertTypeBlood,
	EAlertTypeLightSource,
	EAlertTypeMissingItem,
	EAlertTypeBrokenItem,
	EAlertTypeDoor,
	EAlertTypeDamage,
	EAlertTypeCount
};

// The alert index the AI is in
enum EAlertState {
	ERelaxed = 0,
	EObservant,
	ESuspicious,
	EInvestigating,
	EAgitatedSearching,
	ECombat,
	EAlertStateNum
};

const char* const AlertStateNames[EAlertStateNum] = 
{
	"Relaxed",
	"Observant",
	"Suspicious",
	"Investigating",
	"AgitatedSearching",
	"Combat"
};

#define MINIMUM_SECONDS_BETWEEN_STIMULUS_BARKS 15000 // milliseconds

// SZ: Maximum amount of time since last visual or audio contact with a friendly person to use
// group stimulous barks, in seconds
#define MAX_FRIEND_SIGHTING_SECONDS_FOR_ACCOMPANIED_ALERT_BARK 10.0f

// TODO: Parameterize these as darkmod globals
#define HIDING_OBJECT_HEIGHT 0.35f
#define MAX_SPOTS_PER_SEARCH_CALL 100

// The maximum time the AI is able to follow the enemy although it's invisible
#define MAX_BLIND_CHASE_TIME 3000

/**
 * greebo: This class acts as container for all kinds of state variables.
 */
class Memory
{
public:
	// The owning AI
	idAI* owner;

	// The alert state we're currently in
	EAlertState	alertState;

	// The path entity we're supposed to be heading to
	idEntityPtr<idPathCorner> currentPath;

	// Our last path entity
	idEntityPtr<idPathCorner> lastPath;

	// The game time, the AlertLevel was last increased.
	int lastAlertRiseTime;

	// The last time the AI has been barking when patrolling
	int lastPatrolChatTime;

	int	lastTimeFriendlyAISeen;

	// This is the last time the enemy was visible
	int	lastTimeEnemySeen;

	// The last time a visual stim made the AI bark
	int lastTimeVisualStimBark;

	/*!
	* This variable indicates the number of out of place things that the
	* AI has witness, such as sounds, missing items, open doors, torches gone
	* out etc..
	*/
	int countEvidenceOfIntruders;

	// Random head turning
	int nextHeadTurnCheckTime;
	bool currentlyHeadTurning;
	int headTurnEndTime;

	idVec3 idlePosition;
	float idleYaw;

	// angua: whether the AI should play idle animations
	bool playIdleAnimations;

	// TRUE if enemies have been seen
	bool enemiesHaveBeenSeen;

	// TRUE if the AI knows that items have been stolen
	bool itemsHaveBeenStolen;

	// TRUE if the AI has found something broken
	bool itemsHaveBeenBroken;

	// TRUE if the AI has found a dead or unconscious person
	bool unconsciousPeopleHaveBeenFound;
	bool deadPeopleHaveBeenFound;

	// position of alert causing stimulus
	idVec3 alertPos;

	// Type of alert (visual, tactile, audio)
	EAlertClass alertClass;

	// Source of the alert (enemy, weapon, blood, dead person, etc.)
	EAlertType alertType;

	// radius of alert causing stimulus (depends on the type and distance)
	float alertRadius;

	// This is true if the original alert position is to be searched
	bool stimulusLocationItselfShouldBeSearched;

	// Set this to TRUE if stimulus location itself should be closely investigated (kneel down)
	bool investigateStimulusLocationClosely;

	// This flag indicates if the last alert is due to a communication message
	bool alertedDueToCommunication;

	// Position of the last alert causing stimulus which was searched.
    // This is used to compare new stimuli to the previous stimuli searched
    // to determine if a new search is necessary
	idVec3 lastAlertPosSearched;

	// greebo: This is the position of the alert that was used to set up a hiding spot search.
	idVec3 alertSearchCenter;

	// A search area vector that is m_alertRadius on each side
	idVec3 alertSearchVolume;

	// An area within the search volume that is to be ignored. It is used for expanding
	// radius searches that don't re-search the inner points.
	idVec3 alertSearchExclusionVolume;

	// The last position the enemy was seen
	// greebo: Note: Currently this is filled in before fleeing only.
	idVec3 lastEnemyPos;

	// This is set to TRUE by the sensory routines to indicate whether
	// the AI is in the position to damage the player.
	// This flag is mostly for caching purposes so that the subsystem tasks
	// don't need to query idAI::CanHitEnemy() independently.
	bool canHitEnemy;
	// ishtvan: Whether we will be able to hit the enemy in the future if we
	// start a melee attack right now.
	// If the AI checks this, the CanHitEnemy query sets it as well as canHitEnemy
	bool willBeAbleToHitEnemy;

	/**
	* When true, an enemy can potentially hit us with a melee attack in the near future
	**/
	bool canBeHitByEnemy;

	/*!
	* These hold the current spot search target, regardless of whether
	* or not it is a hiding spot search or some other sort of spot search
	*/
	idVec3 currentSearchSpot;

	/*!
	* This flag indicates if a hiding spot test was started
	* @author SophisticatedZombie
	*/
	bool hidingSpotTestStarted;

	/*!
	* This flag idnicates if a hiding spot was chosen
	*/
	bool hidingSpotSearchDone;

	/**
	 * greebo: TRUE if a hiding spot search has been started and 
	 * the AI has searched all of them already. 
	 */
	bool noMoreHidingSpots;

	/**
	 * greebo: This is queried by the SearchStates and indicates a new
	 *         stimulus to be considered.
	 */
	bool restartSearchForHidingSpots;

	// This counts the number of frames we have been thinking, in case
	// we have a problem with hiding spot searches not returning
	int hidingSpotThinkFrameCount;

	int firstChosenHidingSpotIndex;
	int currentChosenHidingSpotIndex;
	idVec3 chosenHidingSpot;

	// True if the AI is currently investigating a hiding spot (walking to it, for instance).
	bool hidingSpotInvestigationInProgress;

	// True if fleeing is done, false if fleeing is in progress
	bool fleeingDone;

	// angua: The last position of the AI before it takes cover, so it can return to it later.
	idVec3 positionBeforeTakingCover;

	// Maps doors to info structures
	typedef std::map<CFrobDoor*, DoorInfoPtr> DoorInfoMap;
	// This maps AAS area numbers to door info structures
	typedef std::map<int, DoorInfoPtr> AreaToDoorInfoMap;

	// Variables related to door opening/closing process
	struct DoorRelatedVariables
	{
		// The door we're currently handling (e.g. during HandleDoorTask)
		idEntityPtr<CFrobDoor> currentDoor;

		// This maps CFrobDoor* pointers to door info structures
		DoorInfoMap doorInfo;

		// This allows for quick lookup of info structures via area number
		AreaToDoorInfoMap areaDoorInfoMap;
	} doorRelated;

	struct GreetingInfo
	{
		// The last time the associated AI was greeted by the owner
		int lastGreetingTime;

		// The last time the actor was met and considered for greeting
		// the actual greeting doesn't have to be performed
		// This is used to "ignore" visual stims for greeting for a while
		int lastConsiderTime;

		GreetingInfo() :
			lastGreetingTime(-1),
			lastConsiderTime(-1)
		{}
	};

	typedef std::map<idActor*, GreetingInfo> ActorGreetingInfoMap;
	ActorGreetingInfoMap greetingInfo;

	// Pass the owning AI to the constructor
	Memory(idAI* owningAI);

	// Save/Restore routines
	void Save(idSaveGame* savefile) const;
	void Restore(idRestoreGame* savefile);

	/**
	 * greebo: Returns the door info structure for the given door. 
	 * This will create a new info structure if it doesn't exist yet, so the reference is always valid.
	 */
	DoorInfo& GetDoorInfo(CFrobDoor* door);

	// Similar to above, but use the area number as input argument, can return NULL
	DoorInfoPtr GetDoorInfo(int areaNum);

	// Returns the greeting info structure for the given actor
	GreetingInfo& GetGreetingInfo(idActor* actor);
};

} // namespace ai

#endif /*__AI_MEMORY_H__*/
