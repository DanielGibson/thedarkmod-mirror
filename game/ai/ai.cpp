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

#include "../../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id$", init_version);

#include "../game_local.h"
#include "../../DarkMod/Relations.h"
#include "../../DarkMod/MissionData.h"
#include "../../DarkMod/StimResponse/StimResponseCollection.h"
#include "../../DarkMod/DarkModGlobals.h"
#include "../../DarkMod/PlayerData.h"
#include "../../DarkMod/sndProp.h"
#include "../../DarkMod/EscapePointManager.h"

// For handling the opening of doors and other binary Frob movers
#include "../../DarkMod/BinaryFrobMover.h"
#include "../../DarkMod/FrobDoor.h"
#include "../../DarkMod/FrobDoorHandle.h"
#include "tdmAASFindEscape.h"


//TODO: Move these to AI def:

// Visual detection parameters

/**
* amount of time to normalize by for the % check, in seconds
*  this gets modified by 1 / the cvar dm_ai_sight.
**/
static const float s_VisNormtime = 0.2f;

/**
* In pitch darkness, player is invisible
**/
// full bright minimum distance is 0
static const float s_VisFDMin = 0.0f;

// full bright maximum distance is zero (because probability is always zero anyway)
static const float s_VisFDMax = 0.0f;

// TODO: Move this to def file or INI
static const float s_AITactDist = 1.0f;

const float s_DOOM_TO_METERS = 0.0254f;

// TDM: Maximum flee distance for any AI
const float MAX_FLEE_DISTANCE = 10000.0f;

class CRelations;
class CsndProp;
class CDarkModPlayer;
class CMissionData;

static const char *moveCommandString[ NUM_MOVE_COMMANDS ] = {
	"MOVE_NONE",
	"MOVE_FACE_ENEMY",
	"MOVE_FACE_ENTITY",
	"MOVE_TO_ENEMY",
	"MOVE_TO_ENEMYHEIGHT",
	"MOVE_TO_ENTITY",
	"MOVE_OUT_OF_RANGE",
	"MOVE_TO_ATTACK_POSITION",
	"MOVE_TO_COVER",
	"MOVE_TO_POSITION",
	"MOVE_TO_POSITION_DIRECT",
	"MOVE_SLIDE_TO_POSITION",
	"MOVE_WANDER"
};

/*
=====================
idMoveState::idMoveState
=====================
*/
idMoveState::idMoveState() {
	moveType			= MOVETYPE_ANIM;
	moveCommand			= MOVE_NONE;
	moveStatus			= MOVE_STATUS_DONE;
	moveDest.Zero();
	moveDir.Set( 1.0f, 0.0f, 0.0f );
	goalEntity			= NULL;
	goalEntityOrigin.Zero();
	toAreaNum			= 0;
	startTime			= 0;
	duration			= 0;
	speed				= 0.0f;
	range				= 0.0f;
	wanderYaw			= 0;
	nextWanderTime		= 0;
	blockTime			= 0;
	obstacle			= NULL;
	lastMoveOrigin		= vec3_origin;
	lastMoveTime		= 0;
	anim				= 0;
}

/*
=====================
idMoveState::Save
=====================
*/
void idMoveState::Save( idSaveGame *savefile ) const {
	savefile->WriteInt( (int)moveType );
	savefile->WriteInt( (int)moveCommand );
	savefile->WriteInt( (int)moveStatus );
	savefile->WriteVec3( moveDest );
	savefile->WriteVec3( moveDir );
	goalEntity.Save( savefile );
	savefile->WriteVec3( goalEntityOrigin );
	savefile->WriteInt( toAreaNum );
	savefile->WriteInt( startTime );
	savefile->WriteInt( duration );
	savefile->WriteFloat( speed );
	savefile->WriteFloat( range );
	savefile->WriteFloat( wanderYaw );
	savefile->WriteInt( nextWanderTime );
	savefile->WriteInt( blockTime );
	obstacle.Save( savefile );
	savefile->WriteVec3( lastMoveOrigin );
	savefile->WriteInt( lastMoveTime );
	savefile->WriteInt( anim );
}

/*
=====================
idMoveState::Restore
=====================
*/
void idMoveState::Restore( idRestoreGame *savefile ) {
	savefile->ReadInt( (int &)moveType );
	savefile->ReadInt( (int &)moveCommand );
	savefile->ReadInt( (int &)moveStatus );
	savefile->ReadVec3( moveDest );
	savefile->ReadVec3( moveDir );
	goalEntity.Restore( savefile );
	savefile->ReadVec3( goalEntityOrigin );
	savefile->ReadInt( toAreaNum );
	savefile->ReadInt( startTime );
	savefile->ReadInt( duration );
	savefile->ReadFloat( speed );
	savefile->ReadFloat( range );
	savefile->ReadFloat( wanderYaw );
	savefile->ReadInt( nextWanderTime );
	savefile->ReadInt( blockTime );
	obstacle.Restore( savefile );
	savefile->ReadVec3( lastMoveOrigin );
	savefile->ReadInt( lastMoveTime );
	savefile->ReadInt( anim );
}

/*
============
idAASFindCover::idAASFindCover
============
*/
idAASFindCover::idAASFindCover( const idActor* hidingActor, const idEntity* hideFromEnt, const idVec3 &hideFromPos ) {
	this->hidingActor = hidingActor; // This should not be NULL
	this->hideFromEnt = hideFromEnt; // May be NULL
	this->hideFromPos = hideFromPos;
}

/*
============
idAASFindCover::~idAASFindCover
============
*/
idAASFindCover::~idAASFindCover() {
}

/*
============
idAASFindCover::TestArea
============
*/
bool idAASFindCover::TestArea( const idAAS *aas, int areaNum )
{
	idVec3	areaCenter;
	trace_t	trace, trace2;

	if (areaNum == aas->PointAreaNum(hidingActor->GetPhysics()->GetOrigin()))
	{
		// We're in this AAS area; assume that our current position can never be cover.
		// (If it was, we probably wouldn't be trying to move into cover.)
		return false;
	}

	// Get location of feet
	// Assumes they're at the centre of the bounding box in the X and Y axes,
	// but at the bottom in the Z axis.
	idBounds bounds = hidingActor->GetPhysics()->GetAbsBounds();
	idVec3 feet = (bounds[0] + bounds[1]) * 0.5f;
	feet.z = bounds[0].z;

	// Get location to trace to
	areaCenter = aas->AreaCenter( areaNum );

	// Adjust areaCenter to factor in height of AI, so we trace from the estimated eye position
	areaCenter += hidingActor->GetEyePosition() - feet;

	gameLocal.clip.TracePoint(trace, hideFromPos, areaCenter, MASK_OPAQUE, hideFromEnt);
	if (trace.fraction < 1.0f)
	{
		// The trace was interrupted, so this location is probably cover.
		//gameRenderWorld->DebugLine( colorGreen, hideFromPos, areaCenter, 5000, true);

		// But before we say for certain, let's look at the floor as well.
		areaCenter = aas->AreaCenter( areaNum );
		gameLocal.clip.TracePoint(trace2, hideFromPos, areaCenter, MASK_OPAQUE, hideFromEnt);
		if (trace2.fraction < 1.0f)
		{
			// Yes, the feet are hidden too, so this is almost certainly cover
			//gameRenderWorld->DebugLine( colorGreen, hideFromPos, areaCenter, 5000, true);
			return true;
		}

		// Oops; the head is hidden but the feet are not, so this isn't very good cover at all.
		//gameRenderWorld->DebugLine( colorRed, hideFromPos, areaCenter, 5000, true);
		return false;
	}

	// The trace found a clear path, so this location is not cover.
	//gameRenderWorld->DebugLine( colorRed, hideFromPos, areaCenter, 5000, true);
	return false;
}

/*
============
idAASFindAreaOutOfRange::idAASFindAreaOutOfRange
============
*/
idAASFindAreaOutOfRange::idAASFindAreaOutOfRange( const idVec3 &targetPos, float maxDist ) {
	this->targetPos		= targetPos;
	this->maxDistSqr	= maxDist * maxDist;
}

/*
============
idAASFindAreaOutOfRange::TestArea
============
*/
bool idAASFindAreaOutOfRange::TestArea( const idAAS *aas, int areaNum ) {
	const idVec3 &areaCenter = aas->AreaCenter( areaNum );
	trace_t	trace;
	float dist;

	dist = ( targetPos.ToVec2() - areaCenter.ToVec2() ).LengthSqr();

	if ( ( maxDistSqr > 0.0f ) && ( dist < maxDistSqr ) ) {
		return false;
	}

	gameLocal.clip.TracePoint( trace, targetPos, areaCenter + idVec3( 0.0f, 0.0f, 1.0f ), MASK_OPAQUE, NULL );
	if ( trace.fraction < 1.0f ) {
		return false;
	}

	return true;
}

/*
============
idAASFindAttackPosition::idAASFindAttackPosition
============
*/
idAASFindAttackPosition::idAASFindAttackPosition( const idAI *self, const idMat3 &gravityAxis, idEntity *target, const idVec3 &targetPos, const idVec3 &fireOffset ) {
	int	numPVSAreas;

	this->target		= target;
	this->targetPos		= targetPos;
	this->fireOffset	= fireOffset;
	this->self			= self;
	this->gravityAxis	= gravityAxis;

	excludeBounds		= idBounds( idVec3( -64.0, -64.0f, -8.0f ), idVec3( 64.0, 64.0f, 64.0f ) );
	excludeBounds.TranslateSelf( self->GetPhysics()->GetOrigin() );

	// setup PVS
	idBounds bounds( targetPos - idVec3( 16, 16, 0 ), targetPos + idVec3( 16, 16, 64 ) );
	numPVSAreas = gameLocal.pvs.GetPVSAreas( bounds, PVSAreas, idEntity::MAX_PVS_AREAS );
	targetPVS	= gameLocal.pvs.SetupCurrentPVS( PVSAreas, numPVSAreas );
}

/*
============
idAASFindAttackPosition::~idAASFindAttackPosition
============
*/
idAASFindAttackPosition::~idAASFindAttackPosition() {
	gameLocal.pvs.FreeCurrentPVS( targetPVS );
}

/*
============
idAASFindAttackPosition::TestArea
============
*/
bool idAASFindAttackPosition::TestArea( const idAAS *aas, int areaNum ) {
	idVec3	dir;
	idVec3	local_dir;
	idVec3	fromPos;
	idMat3	axis;
	idVec3	areaCenter;
	int		numPVSAreas;
	int		PVSAreas[ idEntity::MAX_PVS_AREAS ];

	areaCenter = aas->AreaCenter( areaNum );
	areaCenter[ 2 ] += 1.0f;

	if ( excludeBounds.ContainsPoint( areaCenter ) ) {
		// too close to where we already are
		return false;
	}

	numPVSAreas = gameLocal.pvs.GetPVSAreas( idBounds( areaCenter ).Expand( 16.0f ), PVSAreas, idEntity::MAX_PVS_AREAS );
	if ( !gameLocal.pvs.InCurrentPVS( targetPVS, PVSAreas, numPVSAreas ) ) {
		return false;
	}

	// calculate the world transform of the launch position
	dir = targetPos - areaCenter;
	gravityAxis.ProjectVector( dir, local_dir );
	local_dir.z = 0.0f;
	local_dir.ToVec2().Normalize();
	axis = local_dir.ToMat3();
	fromPos = areaCenter + fireOffset * axis;

	return self->GetAimDir( fromPos, target, self, dir );
}


/*
============
idAASFindObservationPosition::idAASFindObservationPosition
============
*/
idAASFindObservationPosition::idAASFindObservationPosition( const idAI *self, const idMat3 &gravityAxis, const idVec3 &targetPos, const idVec3 &eyeOffset, float maxDistanceFromWhichToObserve )
{
	int	numPVSAreas;

	this->targetPos		= targetPos;
	this->eyeOffset		= eyeOffset;
	this->self			= self;
	this->gravityAxis	= gravityAxis;
	this->maxObservationDistance = maxDistanceFromWhichToObserve;
	this->b_haveBestGoal = false;


	// setup PVS
	idBounds bounds( targetPos - idVec3( 16, 16, 0 ), targetPos + idVec3( 16, 16, 64 ) );
	numPVSAreas = gameLocal.pvs.GetPVSAreas( bounds, PVSAreas, idEntity::MAX_PVS_AREAS );
	targetPVS	= gameLocal.pvs.SetupCurrentPVS( PVSAreas, numPVSAreas );
}

/*
============
idAASFindObservationPosition::~idAASFindObservationPosition
============
*/
idAASFindObservationPosition::~idAASFindObservationPosition() {
	gameLocal.pvs.FreeCurrentPVS( targetPVS );
}

/*
============
idAASFindObservationPosition::TestArea
============
*/
bool idAASFindObservationPosition::TestArea( const idAAS *aas, int areaNum )
{
	idVec3	dir;
	idVec3	local_dir;
	idVec3	fromPos;
	idMat3	axis;
	idVec3	areaCenter;

	areaCenter = aas->AreaCenter( areaNum );
	areaCenter[ 2 ] += 1.0f;

	/*
	numPVSAreas = gameLocal.pvs.GetPVSAreas( idBounds( areaCenter ).Expand( 16.0f ), PVSAreas, idEntity::MAX_PVS_AREAS );
	if ( !gameLocal.pvs.InCurrentPVS( targetPVS, PVSAreas, numPVSAreas ) ) {
		return false;
	}
	*/

	// calculate the world transform of the view position
	dir = targetPos - areaCenter;
	gravityAxis.ProjectVector( dir, local_dir );
	local_dir.z = 0.0f;
	local_dir.ToVec2().Normalize();
	axis = local_dir.ToMat3();

	fromPos = areaCenter + eyeOffset * axis;

	// Run trace
	trace_t results;
	gameLocal.clip.TracePoint( results, fromPos, targetPos, MASK_SOLID, self );
	if (  results.fraction >= 1.0f )
	{
		// What is the observation distance?
		float distance = (fromPos - targetPos).Length();

		// Remember best result, even if outside max distance allowed
		if ((!b_haveBestGoal) || (distance < bestGoalDistance))
		{
			b_haveBestGoal = true;
			bestGoalDistance = distance;
			bestGoal.areaNum = areaNum;
			bestGoal.origin = areaCenter;
		}
		if (distance > maxObservationDistance)
		{
			// Can't use this point, its too far
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}

}

//------------------------------------------------------------------------------

bool idAASFindObservationPosition::getBestGoalResult
(
	float& out_bestGoalDistance,
	aasGoal_t& out_bestGoal
)
{
	if (b_haveBestGoal)
	{
		out_bestGoalDistance = bestGoalDistance;
		out_bestGoal = bestGoal;
		return true;
	}
	else
	{
		return false;
	}
}


/*
=====================
idAI::idAI
=====================
*/
idAI::idAI() {
	aas					= NULL;
	travelFlags			= TFL_WALK|TFL_AIR;

	kickForce			= 2048.0f;
	ignore_obstacles	= false;
	blockedRadius		= 0.0f;
	blockedMoveTime		= 750;
	blockedAttackTime	= 750;
	turnRate			= 360.0f;
	turnVel				= 0.0f;
	anim_turn_yaw		= 0.0f;
	anim_turn_amount	= 0.0f;
	anim_turn_angles	= 0.0f;
	fly_offset			= 0;
	fly_seek_scale		= 1.0f;
	fly_roll_scale		= 0.0f;
	fly_roll_max		= 0.0f;
	fly_roll			= 0.0f;
	fly_pitch_scale		= 0.0f;
	fly_pitch_max		= 0.0f;
	fly_pitch			= 0.0f;
	allowMove			= false;
	allowHiddenMovement	= false;
	fly_speed			= 0.0f;
	fly_bob_strength	= 0.0f;
	fly_bob_vert		= 0.0f;
	fly_bob_horz		= 0.0f;
	lastHitCheckResult	= false;
	lastHitCheckTime	= 0;
	lastAttackTime		= 0;
	melee_range			= 0.0f;
	projectile_height_to_distance_ratio = 1.0f;
	projectileDef		= NULL;
	projectile			= NULL;
	projectileClipModel	= NULL;
	projectileRadius	= 0.0f;
	projectileVelocity	= vec3_origin;
	projectileGravity	= vec3_origin;
	projectileSpeed		= 0.0f;
	chat_snd			= NULL;
	chat_min			= 0;
	chat_max			= 0;
	chat_time			= 0;
	talk_state			= TALK_NEVER;
	talkTarget			= NULL;

	particles.Clear();
	restartParticles	= true;
	useBoneAxis			= false;

	wakeOnFlashlight	= false;
	memset( &worldMuzzleFlash, 0, sizeof ( worldMuzzleFlash ) );
	worldMuzzleFlashHandle = -1;

	enemy				= NULL;
	lastVisibleEnemyPos.Zero();
	lastVisibleEnemyEyeOffset.Zero();
	lastVisibleReachableEnemyPos.Zero();
	lastReachableEnemyPos.Zero();
	shrivel_rate		= 0.0f;
	shrivel_start		= 0;
	fl.neverDormant		= false;		// AI's can go dormant
	current_yaw			= 0.0f;
	ideal_yaw			= 0.0f;

	num_cinematics		= 0;
	current_cinematic	= 0;

	allowEyeFocus		= true;
	allowPain			= true;
	allowJointMod		= true;
	focusEntity			= NULL;
	focusTime			= 0;
	alignHeadTime		= 0;
	forceAlignHeadTime	= 0;

	currentFocusPos.Zero();
	eyeAng.Zero();
	lookAng.Zero();
	destLookAng.Zero();
	lookMin.Zero();
	lookMax.Zero();

	eyeMin.Zero();
	eyeMax.Zero();
	muzzleFlashEnd		= 0;
	flashTime			= 0;
	flashJointWorld		= INVALID_JOINT;

	focusJoint			= INVALID_JOINT;
	orientationJoint	= INVALID_JOINT;
	flyTiltJoint		= INVALID_JOINT;

	eyeVerticalOffset	= 0.0f;
	eyeHorizontalOffset = 0.0f;
	eyeFocusRate		= 0.0f;
	headFocusRate		= 0.0f;
	focusAlignTime		= 0;

	m_SoundDir.Zero();
	m_LastSight.Zero();
	m_AlertNumThisFrame = 0.0f;
	m_AlertedByActor = NULL;

	m_TactAlertEnt = NULL;
	m_AlertGraceActor = NULL;
	m_AlertGraceStart = 0;
	m_AlertGraceTime = 0;
	m_AlertGraceThresh = 0;
	m_AlertGraceCount = 0;
	m_AlertGraceCountLimit = 0;
	m_AudThreshold = 0.0f;

	/**
	* Darkmod: No hiding spot search by default
	*/
	m_HidingSpotSearchHandle = NULL_HIDING_SPOT_SEARCH_HANDLE;

	m_bCanDrown = true;
	m_AirCheckTimer = 0;
	m_AirTics = 0;
	m_AirTicksMax = 0;
	m_HeadBodyID = 0;
	m_HeadJointID = INVALID_JOINT;
	m_MouthOffset = vec3_zero;

	m_bCanBeKnockedOut = true;
	m_KoOffset = vec3_zero;

	m_lipSyncActive		= false;
}

/*
=====================
idAI::~idAI
=====================
*/
idAI::~idAI() {
	delete projectileClipModel;
	DeconstructScriptObject();
	scriptObject.Free();
	if ( worldMuzzleFlashHandle != -1 ) {
		gameRenderWorld->FreeLightDef( worldMuzzleFlashHandle );
		worldMuzzleFlashHandle = -1;
	}

	/**
	* Darkmod: Get rid of current hiding spot search if there is one.
	*/
	destroyCurrentHidingSpotSearch();

}

/*
=====================
idAI::Save
=====================
*/
void idAI::Save( idSaveGame *savefile ) const {
	int i;

	savefile->WriteInt( travelFlags );
	move.Save( savefile );
	savedMove.Save( savefile );
	savefile->WriteFloat( kickForce );
	savefile->WriteBool( ignore_obstacles );
	savefile->WriteFloat( blockedRadius );
	savefile->WriteInt( blockedMoveTime );
	savefile->WriteInt( blockedAttackTime );

	savefile->WriteFloat( ideal_yaw );
	savefile->WriteFloat( current_yaw );
	savefile->WriteFloat( turnRate );
	savefile->WriteFloat( turnVel );
	savefile->WriteFloat( anim_turn_yaw );
	savefile->WriteFloat( anim_turn_amount );
	savefile->WriteFloat( anim_turn_angles );

	savefile->WriteStaticObject( physicsObj );

	savefile->WriteFloat( fly_speed );
	savefile->WriteFloat( fly_bob_strength );
	savefile->WriteFloat( fly_bob_vert );
	savefile->WriteFloat( fly_bob_horz );
	savefile->WriteInt( fly_offset );
	savefile->WriteFloat( fly_seek_scale );
	savefile->WriteFloat( fly_roll_scale );
	savefile->WriteFloat( fly_roll_max );
	savefile->WriteFloat( fly_roll );
	savefile->WriteFloat( fly_pitch_scale );
	savefile->WriteFloat( fly_pitch_max );
	savefile->WriteFloat( fly_pitch );

	savefile->WriteBool( allowMove );
	savefile->WriteBool( allowHiddenMovement );
	savefile->WriteBool( disableGravity );
	savefile->WriteBool( m_bAFPushMoveables );

	savefile->WriteBool( lastHitCheckResult );
	savefile->WriteInt( lastHitCheckTime );
	savefile->WriteInt( lastAttackTime );
	savefile->WriteFloat( melee_range );
	savefile->WriteFloat( projectile_height_to_distance_ratio );

	savefile->WriteInt( missileLaunchOffset.Num() );
	for( i = 0; i < missileLaunchOffset.Num(); i++ ) {
		savefile->WriteVec3( missileLaunchOffset[ i ] );
	}

	idStr projectileName;
	spawnArgs.GetString( "def_projectile", "", projectileName );
	savefile->WriteString( projectileName );
	savefile->WriteFloat( projectileRadius );
	savefile->WriteFloat( projectileSpeed );
	savefile->WriteVec3( projectileVelocity );
	savefile->WriteVec3( projectileGravity );
	projectile.Save( savefile );
	savefile->WriteString( attack );

	savefile->WriteSoundShader( chat_snd );
	savefile->WriteInt( chat_min );
	savefile->WriteInt( chat_max );
	savefile->WriteInt( chat_time );
	savefile->WriteInt( talk_state );
	talkTarget.Save( savefile );

	savefile->WriteInt( num_cinematics );
	savefile->WriteInt( current_cinematic );

	savefile->WriteBool( allowJointMod );
	focusEntity.Save( savefile );
	savefile->WriteVec3( currentFocusPos );
	savefile->WriteInt( focusTime );
	savefile->WriteInt( alignHeadTime );
	savefile->WriteInt( forceAlignHeadTime );
	savefile->WriteAngles( eyeAng );
	savefile->WriteAngles( lookAng );
	savefile->WriteAngles( destLookAng );
	savefile->WriteAngles( lookMin );
	savefile->WriteAngles( lookMax );

	savefile->WriteInt( lookJoints.Num() );
	for( i = 0; i < lookJoints.Num(); i++ ) {
		savefile->WriteJoint( lookJoints[ i ] );
		savefile->WriteAngles( lookJointAngles[ i ] );
	}

	savefile->WriteFloat( shrivel_rate );
	savefile->WriteInt( shrivel_start );

	savefile->WriteInt( particles.Num() );
	for  ( i = 0; i < particles.Num(); i++ ) {
		savefile->WriteParticle( particles[i].particle );
		savefile->WriteInt( particles[i].time );
		savefile->WriteJoint( particles[i].joint );
	}
	savefile->WriteBool( restartParticles );
	savefile->WriteBool( useBoneAxis );

	enemy.Save( savefile );
	savefile->WriteVec3( lastVisibleEnemyPos );
	savefile->WriteVec3( lastVisibleEnemyEyeOffset );
	savefile->WriteVec3( lastVisibleReachableEnemyPos );
	savefile->WriteVec3( lastReachableEnemyPos );
	savefile->WriteBool( wakeOnFlashlight );

	savefile->WriteAngles( eyeMin );
	savefile->WriteAngles( eyeMax );

	savefile->WriteFloat( eyeVerticalOffset );
	savefile->WriteFloat( eyeHorizontalOffset );
	savefile->WriteFloat( eyeFocusRate );
	savefile->WriteFloat( headFocusRate );
	savefile->WriteInt( focusAlignTime );

	savefile->WriteJoint( flashJointWorld );
	savefile->WriteInt( muzzleFlashEnd );

	savefile->WriteJoint( focusJoint );
	savefile->WriteJoint( orientationJoint );
	savefile->WriteJoint( flyTiltJoint );

	// TDM Alerts:
	savefile->WriteInt( m_Acuities.Num() );
	for( i = 0; i < m_Acuities.Num(); i++ )
	{
		savefile->WriteFloat( m_Acuities[ i ] );
	}
	savefile->WriteFloat( m_AudThreshold );
	savefile->WriteVec3( m_SoundDir );
	savefile->WriteVec3( m_LastSight );
	savefile->WriteFloat( m_AlertNumThisFrame );

	m_AlertedByActor.Save( savefile );
	m_TactAlertEnt.Save( savefile );
	m_AlertGraceActor.Save( savefile );
	savefile->WriteInt( m_AlertGraceStart );
	savefile->WriteInt( m_AlertGraceTime );
	savefile->WriteFloat( m_AlertGraceThresh );
	savefile->WriteInt( m_AlertGraceCount );
	savefile->WriteInt( m_AlertGraceCountLimit );

	savefile->WriteBool( GetPhysics() == static_cast<const idPhysics *>(&physicsObj) );

	savefile->WriteFloat(m_VisDistMax);

	DM_LOG(LC_AI, LT_DEBUG).LogString("Saved hiding spot search with id %d\r", HidingSpotSearchCollection.getSearchId(m_HidingSpotSearchHandle));
	savefile->WriteInt(HidingSpotSearchCollection.getSearchId(m_HidingSpotSearchHandle));
	m_hidingSpots.Save(savefile);

	savefile->WriteInt(m_AirCheckTimer);
	savefile->WriteBool(m_bCanDrown);
	savefile->WriteInt(m_HeadBodyID);
	savefile->WriteJoint(m_HeadJointID);
	savefile->WriteInt(m_AirTics);
	savefile->WriteInt(m_AirTicksMax);
	savefile->WriteInt(m_AirCheckInterval);

	savefile->WriteVec3(m_MouthOffset);
	savefile->WriteBool(m_bCanBeKnockedOut);
	savefile->WriteVec3(m_KoOffset);

	savefile->WriteFloat(thresh_1);
	savefile->WriteFloat(thresh_2);
	savefile->WriteFloat(thresh_3);
	savefile->WriteFloat(thresh_combat);

	savefile->WriteFloat(m_gracetime_1);
	savefile->WriteFloat(m_gracetime_2);
	savefile->WriteFloat(m_gracetime_3);
	savefile->WriteFloat(m_gracefrac_1);
	savefile->WriteFloat(m_gracefrac_2);
	savefile->WriteFloat(m_gracefrac_3);
	savefile->WriteFloat(m_gracecount_1);
	savefile->WriteFloat(m_gracecount_2);
	savefile->WriteFloat(m_gracecount_3);

	savefile->WriteFloat(atime1);
	savefile->WriteFloat(atime2);
	savefile->WriteFloat(atime3);

	savefile->WriteString(m_killedTask.c_str());
	savefile->WriteInt(m_killedTaskPriority);
	savefile->WriteString(m_knockedOutTask.c_str());
	savefile->WriteInt(m_knockedOutTaskPriority);
}

/*
=====================
idAI::Restore
=====================
*/
void idAI::Restore( idRestoreGame *savefile ) {
	bool		restorePhysics;
	int			i;
	int			num;
	idBounds	bounds;

	savefile->ReadInt( travelFlags );
	move.Restore( savefile );
	savedMove.Restore( savefile );
	savefile->ReadFloat( kickForce );
	savefile->ReadBool( ignore_obstacles );
	savefile->ReadFloat( blockedRadius );
	savefile->ReadInt( blockedMoveTime );
	savefile->ReadInt( blockedAttackTime );

	savefile->ReadFloat( ideal_yaw );
	savefile->ReadFloat( current_yaw );
	savefile->ReadFloat( turnRate );
	savefile->ReadFloat( turnVel );
	savefile->ReadFloat( anim_turn_yaw );
	savefile->ReadFloat( anim_turn_amount );
	savefile->ReadFloat( anim_turn_angles );

	savefile->ReadStaticObject( physicsObj );

	savefile->ReadFloat( fly_speed );
	savefile->ReadFloat( fly_bob_strength );
	savefile->ReadFloat( fly_bob_vert );
	savefile->ReadFloat( fly_bob_horz );
	savefile->ReadInt( fly_offset );
	savefile->ReadFloat( fly_seek_scale );
	savefile->ReadFloat( fly_roll_scale );
	savefile->ReadFloat( fly_roll_max );
	savefile->ReadFloat( fly_roll );
	savefile->ReadFloat( fly_pitch_scale );
	savefile->ReadFloat( fly_pitch_max );
	savefile->ReadFloat( fly_pitch );

	savefile->ReadBool( allowMove );
	savefile->ReadBool( allowHiddenMovement );
	savefile->ReadBool( disableGravity );
	savefile->ReadBool( m_bAFPushMoveables );

	savefile->ReadBool( lastHitCheckResult );
	savefile->ReadInt( lastHitCheckTime );
	savefile->ReadInt( lastAttackTime );
	savefile->ReadFloat( melee_range );
	savefile->ReadFloat( projectile_height_to_distance_ratio );

	savefile->ReadInt( num );
	missileLaunchOffset.SetGranularity( 1 );
	missileLaunchOffset.SetNum( num );
	for( i = 0; i < num; i++ ) {
		savefile->ReadVec3( missileLaunchOffset[ i ] );
	}

	idStr projectileName;
	savefile->ReadString( projectileName );
	if ( projectileName.Length() ) {
		projectileDef = gameLocal.FindEntityDefDict( projectileName );
	} else {
		projectileDef = NULL;
	}
	savefile->ReadFloat( projectileRadius );
	savefile->ReadFloat( projectileSpeed );
	savefile->ReadVec3( projectileVelocity );
	savefile->ReadVec3( projectileGravity );
	projectile.Restore( savefile );
	savefile->ReadString( attack );

	savefile->ReadSoundShader( chat_snd );
	savefile->ReadInt( chat_min );
	savefile->ReadInt( chat_max );
	savefile->ReadInt( chat_time );
	savefile->ReadInt( i );
	talk_state = static_cast<talkState_t>( i );
	talkTarget.Restore( savefile );

	savefile->ReadInt( num_cinematics );
	savefile->ReadInt( current_cinematic );

	savefile->ReadBool( allowJointMod );
	focusEntity.Restore( savefile );
	savefile->ReadVec3( currentFocusPos );
	savefile->ReadInt( focusTime );
	savefile->ReadInt( alignHeadTime );
	savefile->ReadInt( forceAlignHeadTime );
	savefile->ReadAngles( eyeAng );
	savefile->ReadAngles( lookAng );
	savefile->ReadAngles( destLookAng );
	savefile->ReadAngles( lookMin );
	savefile->ReadAngles( lookMax );

	savefile->ReadInt( num );
	lookJoints.SetGranularity( 1 );
	lookJoints.SetNum( num );
	lookJointAngles.SetGranularity( 1 );
	lookJointAngles.SetNum( num );
	for( i = 0; i < num; i++ ) {
		savefile->ReadJoint( lookJoints[ i ] );
		savefile->ReadAngles( lookJointAngles[ i ] );
	}

	savefile->ReadFloat( shrivel_rate );
	savefile->ReadInt( shrivel_start );

	savefile->ReadInt( num );
	particles.SetNum( num );
	for  ( i = 0; i < particles.Num(); i++ ) {
		savefile->ReadParticle( particles[i].particle );
		savefile->ReadInt( particles[i].time );
		savefile->ReadJoint( particles[i].joint );
	}
	savefile->ReadBool( restartParticles );
	savefile->ReadBool( useBoneAxis );

	enemy.Restore( savefile );
	savefile->ReadVec3( lastVisibleEnemyPos );
	savefile->ReadVec3( lastVisibleEnemyEyeOffset );
	savefile->ReadVec3( lastVisibleReachableEnemyPos );
	savefile->ReadVec3( lastReachableEnemyPos );

	savefile->ReadBool( wakeOnFlashlight );

	savefile->ReadAngles( eyeMin );
	savefile->ReadAngles( eyeMax );

	savefile->ReadFloat( eyeVerticalOffset );
	savefile->ReadFloat( eyeHorizontalOffset );
	savefile->ReadFloat( eyeFocusRate );
	savefile->ReadFloat( headFocusRate );
	savefile->ReadInt( focusAlignTime );

	savefile->ReadJoint( flashJointWorld );
	savefile->ReadInt( muzzleFlashEnd );

	savefile->ReadJoint( focusJoint );
	savefile->ReadJoint( orientationJoint );
	savefile->ReadJoint( flyTiltJoint );

	// TDM Alerts:
	savefile->ReadInt( num );
	m_Acuities.SetNum( num );
	for( i = 0; i < num; i++ )
	{
		savefile->ReadFloat( m_Acuities[ i ] );
	}
	savefile->ReadFloat( m_AudThreshold );
	savefile->ReadVec3( m_SoundDir );
	savefile->ReadVec3( m_LastSight );
	savefile->ReadFloat( m_AlertNumThisFrame );

	m_AlertedByActor.Restore( savefile );
	m_TactAlertEnt.Restore( savefile );
	m_AlertGraceActor.Restore( savefile );
	savefile->ReadInt( m_AlertGraceStart );
	savefile->ReadInt( m_AlertGraceTime );
	savefile->ReadFloat( m_AlertGraceThresh );
	savefile->ReadInt( m_AlertGraceCount );
	savefile->ReadInt( m_AlertGraceCountLimit );

	savefile->ReadBool( restorePhysics );

	// Set the AAS if the character has the correct gravity vector
	idVec3 gravity = spawnArgs.GetVector( "gravityDir", "0 0 -1" );
	gravity *= g_gravity.GetFloat();
	if ( gravity == gameLocal.GetGravity() ) {
		SetAAS();
	}

	savefile->ReadFloat(m_VisDistMax);

	int searchId;
	savefile->ReadInt(searchId);
	DM_LOG(LC_AI, LT_DEBUG).LogString("Restored hiding spot search with id %d (entity %s)\r", searchId, name.c_str());
	m_HidingSpotSearchHandle = HidingSpotSearchCollection.getSearchHandle(searchId);
	if (searchId != -1 && m_HidingSpotSearchHandle == NULL_HIDING_SPOT_SEARCH_HANDLE)
	{
		DM_LOG(LC_AI, LT_DEBUG).LogString("Warning! Could not resolve hiding spot search with id %d (entity %s)\r", searchId, name.c_str());
	}
	m_hidingSpots.Restore(savefile);

	savefile->ReadInt(m_AirCheckTimer);
	savefile->ReadBool(m_bCanDrown);
	savefile->ReadInt(m_HeadBodyID);
	savefile->ReadJoint(m_HeadJointID);
	savefile->ReadInt(m_AirTics);
	savefile->ReadInt(m_AirTicksMax);
	savefile->ReadInt(m_AirCheckInterval);

	savefile->ReadVec3(m_MouthOffset);
	savefile->ReadBool(m_bCanBeKnockedOut);
	savefile->ReadVec3(m_KoOffset);

	savefile->ReadFloat(thresh_1);
	savefile->ReadFloat(thresh_2);
	savefile->ReadFloat(thresh_3);
	savefile->ReadFloat(thresh_combat);

	savefile->ReadFloat(m_gracetime_1);
	savefile->ReadFloat(m_gracetime_2);
	savefile->ReadFloat(m_gracetime_3);
	savefile->ReadFloat(m_gracefrac_1);
	savefile->ReadFloat(m_gracefrac_2);
	savefile->ReadFloat(m_gracefrac_3);
	savefile->ReadInt(m_gracecount_1);
	savefile->ReadInt(m_gracecount_2);
	savefile->ReadInt(m_gracecount_3);

	savefile->ReadFloat(atime1);
	savefile->ReadFloat(atime2);
	savefile->ReadFloat(atime3);

	savefile->ReadString(m_killedTask);
	savefile->ReadInt(m_killedTaskPriority);
	savefile->ReadString(m_knockedOutTask);
	savefile->ReadInt(m_knockedOutTaskPriority);

	SetCombatModel();
	LinkCombat();

	InitMuzzleFlash();

	// Link the script variables back to the scriptobject
	LinkScriptVariables();

	if ( restorePhysics ) {
		RestorePhysics( &physicsObj );
	}
}

/*
=====================
idAI::Spawn
=====================
*/
void idAI::Spawn( void )
{
	const char			*jointname;
	const idKeyValue	*kv;
	idStr				jointName;
	idAngles			jointScale;
	jointHandle_t		joint;
	idVec3				local_dir;
	bool				talks;

	if ( !g_monsters.GetBool() ) {
		PostEventMS( &EV_Remove, 0 );
		return;
	}

	spawnArgs.GetInt(	"team",					"1",		team );
	spawnArgs.GetInt(	"rank",					"0",		rank );
	spawnArgs.GetInt(	"fly_offset",			"0",		fly_offset );
	spawnArgs.GetFloat( "fly_speed",			"100",		fly_speed );
	spawnArgs.GetFloat( "fly_bob_strength",		"50",		fly_bob_strength );
	spawnArgs.GetFloat( "fly_bob_vert",			"2",		fly_bob_horz );
	spawnArgs.GetFloat( "fly_bob_horz",			"2.7",		fly_bob_vert );
	spawnArgs.GetFloat( "fly_seek_scale",		"4",		fly_seek_scale );
	spawnArgs.GetFloat( "fly_roll_scale",		"90",		fly_roll_scale );
	spawnArgs.GetFloat( "fly_roll_max",			"60",		fly_roll_max );
	spawnArgs.GetFloat( "fly_pitch_scale",		"45",		fly_pitch_scale );
	spawnArgs.GetFloat( "fly_pitch_max",		"30",		fly_pitch_max );

	spawnArgs.GetFloat( "melee_range",			"64",		melee_range );
	spawnArgs.GetFloat( "projectile_height_to_distance_ratio",	"1", projectile_height_to_distance_ratio );

	spawnArgs.GetFloat( "turn_rate",			"360",		turnRate );

	spawnArgs.GetBool( "talks",					"0",		talks );

	//// DarkMod: Alert level parameters
	// The default values of these spawnargs are normally set in tdm_ai_base.def, so the default values
	// here are somewhat superfluous. It's better than having defaults of 0 here though.
	spawnArgs.GetFloat( "alert_thresh1",		"1.5",		thresh_1 );
	spawnArgs.GetFloat( "alert_thresh2",		"8",		thresh_2 );
	spawnArgs.GetFloat( "alert_thresh3",		"18",		thresh_3 );
	spawnArgs.GetFloat( "alert_thresh_combat",	"23",		thresh_combat );
	// Grace period info for each alert level
	spawnArgs.GetFloat( "alert_gracetime1",		"2",		m_gracetime_1 );
	spawnArgs.GetFloat( "alert_gracetime2",		"3.5",		m_gracetime_2 );
	spawnArgs.GetFloat( "alert_gracetime3",		"2",		m_gracetime_3 );
	spawnArgs.GetFloat( "alert_gracefrac1",		"1.2",		m_gracefrac_1 );
	spawnArgs.GetFloat( "alert_gracefrac2",		"1.2",		m_gracefrac_2 );
	spawnArgs.GetFloat( "alert_gracefrac3",		"1.0",		m_gracefrac_3 );
	spawnArgs.GetInt  ( "alert_gracecount1",	"5",		m_gracecount_1 );
	spawnArgs.GetInt  ( "alert_gracecount2",	"5",		m_gracecount_2 );
	spawnArgs.GetInt  ( "alert_gracecount3",	"4",		m_gracecount_3 );
	// De-alert times for each alert level
	spawnArgs.GetFloat( "alert_time1",			"4",		atime1 );
	spawnArgs.GetFloat( "alert_time2",			"30",		atime2 );
	spawnArgs.GetFloat( "alert_time3",			"120",		atime3 );

	// DarkMod: Get the movement type audible volumes from the spawnargs
	spawnArgs.GetFloat( "stepvol_walk",			"0",		m_stepvol_walk );
	spawnArgs.GetFloat( "stepvol_run",			"0",		m_stepvol_run );
	spawnArgs.GetFloat( "stepvol_creep",		"0",		m_stepvol_creep );

	spawnArgs.GetFloat( "stepvol_crouch_walk",			"0",		m_stepvol_crouch_walk );
	spawnArgs.GetFloat( "stepvol_run",			"0",		m_stepvol_crouch_run );
	spawnArgs.GetFloat( "stepvol_creep",		"0",		m_stepvol_crouch_creep );

	if ( spawnArgs.GetString( "npc_name", NULL ) != NULL ) {
		if ( talks ) {
			talk_state = TALK_OK;
		} else {
			talk_state = TALK_BUSY;
		}
	} else {
		talk_state = TALK_NEVER;
	}

	spawnArgs.GetBool( "animate_z",				"0",		disableGravity );
	spawnArgs.GetFloat( "kick_force",			"4096",		kickForce );
	spawnArgs.GetBool( "ignore_obstacles",		"0",		ignore_obstacles );
	spawnArgs.GetFloat( "blockedRadius",		"-1",		blockedRadius );
	spawnArgs.GetInt( "blockedMoveTime",		"750",		blockedMoveTime );
	spawnArgs.GetInt( "blockedAttackTime",		"750",		blockedAttackTime );

	// DarkMod: Set the AI acuities from the spawnargs.

	m_Acuities.Clear();
	for( int ind=0; ind < g_Global.m_AcuityNames.Num(); ind++)
	{
		float tempFloat;
		tempFloat = spawnArgs.GetFloat( va("acuity_%s", g_Global.m_AcuityNames[ind].c_str()), "100" );
		m_Acuities.Append( tempFloat );
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Acuities Array: index %d, name %s, value %f\r", ind, g_Global.m_AcuityNames[ind].c_str(), m_Acuities[ind]);
	}

	spawnArgs.GetFloat("alert_aud_thresh", va("%f",gameLocal.m_sndProp->m_SndGlobals.DefaultThreshold), m_AudThreshold );

	spawnArgs.GetInt(	"num_cinematics",		"0",		num_cinematics );
	current_cinematic = 0;

	LinkScriptVariables();

	/**
	* Initialize Darkmod AI vars
	**/
	AI_ALERTED = false;
	AI_CROUCH = false;
	AI_RUN = false;
	AI_CREEP = false;

	AI_HEARDSOUND = false;
	AI_VISALERT = false;
	AI_TACTALERT = false;
	AI_ALERTED_BY_PLAYER = false;

	fl.takedamage		= !spawnArgs.GetBool( "noDamage" );
	enemy				= NULL;
	allowMove			= true;
	allowHiddenMovement = false;

	animator.RemoveOriginOffset( true );

	// create combat collision hull for exact collision detection
	SetCombatModel();

	lookMin	= spawnArgs.GetAngles( "look_min", "-80 -75 0" );
	lookMax	= spawnArgs.GetAngles( "look_max", "80 75 0" );

	lookJoints.SetGranularity( 1 );
	lookJointAngles.SetGranularity( 1 );
	kv = spawnArgs.MatchPrefix( "look_joint", NULL );
	while( kv ) {
		jointName = kv->GetKey();
		jointName.StripLeadingOnce( "look_joint " );
		joint = animator.GetJointHandle( jointName );
		if ( joint == INVALID_JOINT ) {
			gameLocal.Warning( "Unknown look_joint '%s' on entity %s", jointName.c_str(), name.c_str() );
		} else {
			jointScale = spawnArgs.GetAngles( kv->GetKey(), "0 0 0" );
			jointScale.roll = 0.0f;

			// if no scale on any component, then don't bother adding it.  this may be done to
			// zero out rotation from an inherited entitydef.
			if ( jointScale != ang_zero ) {
				lookJoints.Append( joint );
				lookJointAngles.Append( jointScale );
			}
		}
		kv = spawnArgs.MatchPrefix( "look_joint", kv );
	}

	// calculate joint positions on attack frames so we can do proper "can hit" tests
	CalculateAttackOffsets();

	eyeMin				= spawnArgs.GetAngles( "eye_turn_min", "-10 -30 0" );
	eyeMax				= spawnArgs.GetAngles( "eye_turn_max", "10 30 0" );
	eyeVerticalOffset	= spawnArgs.GetFloat( "eye_verticle_offset", "5" );
	eyeHorizontalOffset = spawnArgs.GetFloat( "eye_horizontal_offset", "-8" );
	eyeFocusRate		= spawnArgs.GetFloat( "eye_focus_rate", "0.5" );
	headFocusRate		= spawnArgs.GetFloat( "head_focus_rate", "0.1" );
	focusAlignTime		= SEC2MS( spawnArgs.GetFloat( "focus_align_time", "1" ) );

	// DarkMod: State of mind, allow the FM author to set initial values
	AI_AlertNum			= spawnArgs.GetFloat( "alert_initial", "0" );
	stateOfMind_b_enemiesHaveBeenSeen = spawnArgs.GetBool ("stateOfMind_b_enemiesHaveBeenSeen");
	stateOfMind_b_itemsHaveBeenStolen = spawnArgs.GetBool ("stateOfMind_b_itemsHaveBeenStolen");
	stateOfMind_count_evidenceOfIntruders = spawnArgs.GetFloat ("stateOfMind_count_evidenceOfIntruders", "0.0");

	/**
	* This tracks how much indirect evidence of an intruder or intruders the AI
	* has accumulated itself or heard about from other AIs.
	* It is used for stateful communication with other AIs and can also influence behaivior.
	**/
	idScriptFloat			stateOfMind_count_evidenceOfIntruders;



	flashJointWorld = animator.GetJointHandle( "flash" );

	if ( head.GetEntity() ) {
		idAnimator *headAnimator = head.GetEntity()->GetAnimator();

		jointname = spawnArgs.GetString( "bone_focus" );
		if ( *jointname ) {
			focusJoint = headAnimator->GetJointHandle( jointname );
			if ( focusJoint == INVALID_JOINT ) {
				gameLocal.Warning( "Joint '%s' not found on head on '%s'", jointname, name.c_str() );
			}
		}
	} else {
		jointname = spawnArgs.GetString( "bone_focus" );
		if ( *jointname ) {
			focusJoint = animator.GetJointHandle( jointname );
			if ( focusJoint == INVALID_JOINT ) {
				gameLocal.Warning( "Joint '%s' not found on '%s'", jointname, name.c_str() );
			}
		}
	}

	jointname = spawnArgs.GetString( "bone_orientation" );
	if ( *jointname ) {
		orientationJoint = animator.GetJointHandle( jointname );
		if ( orientationJoint == INVALID_JOINT ) {
			gameLocal.Warning( "Joint '%s' not found on '%s'", jointname, name.c_str() );
		}
	}

	jointname = spawnArgs.GetString( "bone_flytilt" );
	if ( *jointname ) {
		flyTiltJoint = animator.GetJointHandle( jointname );
		if ( flyTiltJoint == INVALID_JOINT ) {
			gameLocal.Warning( "Joint '%s' not found on '%s'", jointname, name.c_str() );
		}
	}

	InitMuzzleFlash();

	physicsObj.SetSelf( this );
	physicsObj.SetClipModel( new idClipModel( GetPhysics()->GetClipModel() ), 1.0f );
	physicsObj.SetMass( spawnArgs.GetFloat( "mass", "100" ) );

	if ( spawnArgs.GetBool( "big_monster" ) ) {
		physicsObj.SetContents( 0 );
		physicsObj.SetClipMask( MASK_MONSTERSOLID & ~CONTENTS_BODY );
	} else {
		if ( use_combat_bbox ) {
			physicsObj.SetContents( CONTENTS_BODY|CONTENTS_SOLID );
		} else {
			physicsObj.SetContents( CONTENTS_BODY );
		}
		physicsObj.SetClipMask( MASK_MONSTERSOLID );
	}

// SR CONTENTS_RESPONSE fix:
	if( m_StimResponseColl->HasResponse() )
		physicsObj.SetContents( physicsObj.GetContents() | CONTENTS_RESPONSE );

	// move up to make sure the monster is at least an epsilon above the floor
	physicsObj.SetOrigin( GetPhysics()->GetOrigin() + idVec3( 0, 0, CM_CLIP_EPSILON ) );

	if ( num_cinematics ) {
		physicsObj.SetGravity( vec3_origin );
	} else {
		idVec3 gravity = spawnArgs.GetVector( "gravityDir", "0 0 -1" );
		gravity *= g_gravity.GetFloat();
		physicsObj.SetGravity( gravity );
	}

	SetPhysics( &physicsObj );

	physicsObj.GetGravityAxis().ProjectVector( viewAxis[ 0 ], local_dir );
	current_yaw		= local_dir.ToYaw();
	ideal_yaw		= idMath::AngleNormalize180( current_yaw );

	move.blockTime = 0;

	SetAAS();

	projectile		= NULL;
	projectileDef	= NULL;
	projectileClipModel	= NULL;
	idStr projectileName;
	if ( spawnArgs.GetString( "def_projectile", "", projectileName ) && projectileName.Length() ) {
		projectileDef = gameLocal.FindEntityDefDict( projectileName );
		CreateProjectile( vec3_origin, viewAxis[ 0 ] );
		projectileRadius	= projectile.GetEntity()->GetPhysics()->GetClipModel()->GetBounds().GetRadius();
		projectileVelocity	= idProjectile::GetVelocity( projectileDef );
		projectileGravity	= idProjectile::GetGravity( projectileDef );
		projectileSpeed		= projectileVelocity.Length();
		delete projectile.GetEntity();
		projectile = NULL;
	}

	particles.Clear();
	restartParticles = true;
	useBoneAxis = spawnArgs.GetBool( "useBoneAxis" );
	SpawnParticles( "smokeParticleSystem" );

	if ( num_cinematics || spawnArgs.GetBool( "hide" ) || spawnArgs.GetBool( "teleport" ) || spawnArgs.GetBool( "trigger_anim" ) ) {
		fl.takedamage = false;
		physicsObj.SetContents( 0 );
		physicsObj.GetClipModel()->Unlink();
		Hide();
	} else {
		// play a looping ambient sound if we have one
		StartSound( "snd_ambient", SND_CHANNEL_AMBIENT, 0, false, NULL );
	}

	if ( health <= 0 ) {
		gameLocal.Warning( "entity '%s' doesn't have health set", name.c_str() );
		health = 1;
	}

	// set up monster chatter
	SetChatSound();

	// Dark Mod: set up drowning
	m_MouthOffset = spawnArgs.GetVector("mouth_offset");
	if( !head.GetEntity() && af.IsLoaded() )
	{
		const char *headName = spawnArgs.GetString("head_bodyname", "head");
		// this will call gameLocal.Error if the joint name is wrong
		m_HeadBodyID = af.GetPhysics()->GetBodyId( headName );
	}

	// set up drowning timer (add a random bit to make it asynchronous w/ respect to other AI)
	m_bCanDrown = spawnArgs.GetBool( "can_drown", "1" );
	m_AirCheckTimer = gameLocal.time + gameLocal.random.RandomInt( 8000 );
	m_AirTicksMax = spawnArgs.GetInt( "max_air_tics", "5" );
	m_AirTics = m_AirTicksMax;
	m_AirCheckInterval = (int) 1000.0f * spawnArgs.GetFloat( "air_check_interval", "4.0" );
	// end drowning setup

	// Set up KOing and FOV
	const char *HeadJointName = spawnArgs.GetString("head_jointname", "Head");
	m_bCanBeKnockedOut = !( spawnArgs.GetBool("ko_immune", "0") );

	m_HeadJointID = animator.GetJointHandle(HeadJointName);
	if( m_HeadJointID == INVALID_JOINT )
	{
		DM_LOG(LC_AI, LT_ERROR)LOGSTRING("Invalid head joint for joint %s on AI %s \r", HeadJointName, name.c_str());
	}

	m_KoOffset = spawnArgs.GetVector("ko_spot_offset");
	// end KO setup

	BecomeActive( TH_THINK );

	// init the move variables
	StopMove( MOVE_STATUS_DONE );
}

/*
===================
idAI::InitMuzzleFlash
===================
*/
void idAI::InitMuzzleFlash( void ) {
	const char			*shader;
	idVec3				flashColor;

	spawnArgs.GetString( "mtr_flashShader", "muzzleflash", &shader );
	spawnArgs.GetVector( "flashColor", "0 0 0", flashColor );
	float flashRadius = spawnArgs.GetFloat( "flashRadius" );
	flashTime = SEC2MS( spawnArgs.GetFloat( "flashTime", "0.25" ) );

	memset( &worldMuzzleFlash, 0, sizeof ( worldMuzzleFlash ) );

	worldMuzzleFlash.pointLight = true;
	worldMuzzleFlash.shader = declManager->FindMaterial( shader, false );
	worldMuzzleFlash.shaderParms[ SHADERPARM_RED ] = flashColor[0];
	worldMuzzleFlash.shaderParms[ SHADERPARM_GREEN ] = flashColor[1];
	worldMuzzleFlash.shaderParms[ SHADERPARM_BLUE ] = flashColor[2];
	worldMuzzleFlash.shaderParms[ SHADERPARM_ALPHA ] = 1.0f;
	worldMuzzleFlash.shaderParms[ SHADERPARM_TIMESCALE ] = 1.0f;
	worldMuzzleFlash.lightRadius[0] = flashRadius;
	worldMuzzleFlash.lightRadius[1]	= flashRadius;
	worldMuzzleFlash.lightRadius[2]	= flashRadius;

	worldMuzzleFlashHandle = -1;
}

/*
===================
idAI::List_f
===================
*/
void idAI::List_f( const idCmdArgs &args ) {
	int		e;
	idAI	*check;
	int		count;
	const char *statename;

	count = 0;

	gameLocal.Printf( "%-4s  %-20s %s\n", " Num", "EntityDef", "Name" );
	gameLocal.Printf( "------------------------------------------------\n" );
	for( e = 0; e < MAX_GENTITIES; e++ ) {
		check = static_cast<idAI *>(gameLocal.entities[ e ]);
		if ( !check || !check->IsType( idAI::Type ) ) {
			continue;
		}

		if ( check->state ) {
			statename = check->state->Name();
		} else {
			statename = "NULL state";
		}

		gameLocal.Printf( "%4i: %-20s %-20s %s  move: %d\n", e, check->GetEntityDefName(), check->name.c_str(), statename, check->allowMove );
		count++;
	}

	gameLocal.Printf( "...%d monsters\n", count );
}

/*
================
idAI::DormantBegin

called when entity becomes dormant
================
*/
void idAI::DormantBegin( void ) {
	// since dormant happens on a timer, we wont get to update particles to
	// hidden through the think loop, but we need to hide them though.
	if ( particles.Num() ) {
		for ( int i = 0; i < particles.Num(); i++ ) {
			particles[i].time = 0;
		}
	}

	if ( enemyNode.InList() ) {
		// remove ourselves from the enemy's enemylist
		enemyNode.Remove();
	}
	idActor::DormantBegin();
}

/*
================
idAI::DormantEnd

called when entity wakes from being dormant
================
*/
void idAI::DormantEnd( void ) {
	if ( enemy.GetEntity() && !enemyNode.InList() ) {
		// let our enemy know we're back on the trail
		enemyNode.AddToEnd( enemy.GetEntity()->enemyList );
	}

	if ( particles.Num() ) {
		for ( int i = 0; i < particles.Num(); i++ ) {
			particles[i].time = gameLocal.time;
		}
	}

	idActor::DormantEnd();
}

/*
=====================
idAI::Think
=====================
*/
void idAI::Think( void ) 
{
	// if we are completely closed off from the player, don't do anything at all
	bool outsidePVS = CheckDormant();
	if (outsidePVS && cv_ai_opt_disable.GetBool()) {
		return;
	}
	
	idVec3 oldOrigin;
	idVec3 oldVelocity;

	// save old origin and velocity for crashlanding
	oldOrigin = physicsObj.GetOrigin();
	oldVelocity = physicsObj.GetLinearVelocity();

	if ( thinkFlags & TH_THINK ) {
		// clear out the enemy when he dies or is hidden
		idActor *enemyEnt = enemy.GetEntity();
		if ( enemyEnt ) {
			if ( enemyEnt->health <= 0 ) {
				EnemyDead();
			}
		}

		current_yaw += deltaViewAngles.yaw;
		ideal_yaw = idMath::AngleNormalize180( ideal_yaw + deltaViewAngles.yaw );
		deltaViewAngles.Zero();
		viewAxis = idAngles( 0, current_yaw, 0 ).ToMat3();

		// TDM: Fake lipsync
		if ( m_lipSyncActive && GetSoundEmitter() && !AI_DEAD && !AI_KNOCKEDOUT )
		{
			if (gameLocal.time < m_lipSyncEndTimer )
			{
				//int totalFrames = animator.NumFrames(m_lipSyncAnim);
				// FIXME: animator.NumFrames is returning strange values.
				// So for now the frame count of the animation is hardcoded
				// at 20.
				int frame = 20*GetSoundEmitter()->CurrentAmplitude();
				headAnim.SetFrame( m_lipSyncAnim, frame );
			}
			else
			{
				// We're done; stop the animation
				StopLipSync();
			}
		}

		// Look for enemies
		if ( !(outsidePVS && cv_ai_opt_novisualscan.GetBool()) )
		{
			idActor* actor = this->VisualScan();
			if (actor) SetEnemy(actor);
		}

		// Check for tactile alert due to AI movement
		CheckTactile();

		// Check if drowning
		if( m_bCanDrown && gameLocal.time > m_AirCheckTimer )
			UpdateAir();

		if ( num_cinematics ) {
			if ( !IsHidden() && torsoAnim.AnimDone( 0 ) ) {
				PlayCinematic();
			}
			RunPhysics();
		} else if ( !allowHiddenMovement && IsHidden() ) {
			// hidden monsters
			UpdateAIScript();
		} else {
			// clear the ik before we do anything else so the skeleton doesn't get updated twice
			walkIK.ClearJointMods();

			switch( move.moveType ) {
			case MOVETYPE_DEAD :
				// dead monsters
				UpdateAIScript();
				DeadMove();
				break;

			case MOVETYPE_FLY :
				// flying monsters
				UpdateEnemyPosition();
				UpdateAIScript();
				FlyMove();
				PlayChatter();
				CheckBlink();
				break;

			case MOVETYPE_STATIC :
				// static monsters
				UpdateEnemyPosition();
				UpdateAIScript();
				StaticMove();
				PlayChatter();
				CheckBlink();
				break;

			case MOVETYPE_ANIM :
				// animation based movement
				UpdateEnemyPosition();
				UpdateAIScript();
				AnimMove();
				PlayChatter();
				CheckBlink();
				break;

			case MOVETYPE_SLIDE :
				// velocity based movement
				UpdateEnemyPosition();
				UpdateAIScript();
				SlideMove();
				PlayChatter();
				CheckBlink();
				break;
			}
		}

		// clear pain flag so that we recieve any damage between now and the next time we run the script
		AI_PAIN = false;
		AI_SPECIAL_DAMAGE = 0;
		AI_PUSHED = false;

	} else if ( thinkFlags & TH_PHYSICS ) {
		RunPhysics();
	}

	if ( m_bAFPushMoveables )
	{
		PushWithAF();
	}

	if ( fl.hidden && allowHiddenMovement ) {
		// UpdateAnimation won't call frame commands when hidden, so call them here when we allow hidden movement
		animator.ServiceAnims( gameLocal.previousTime, gameLocal.time );
	}
/*	this still draws in retail builds.. not sure why.. don't care at this point.
	if ( !aas && developer.GetBool() && !fl.hidden && !num_cinematics ) {
		gameRenderWorld->DrawText( "No AAS", physicsObj.GetAbsBounds().GetCenter(), 0.1f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, gameLocal.msec );
	}
*/

	UpdateMuzzleFlash();
	UpdateAnimation();
	UpdateParticles();
	Present();
	UpdateDamageEffects();
	LinkCombat();

	// DarkMod: Show debug info
	if( cv_ai_ko_show.GetBool() )
	{
		KnockoutDebugDraw();
	}

	if( cv_ai_fov_show.GetBool() )
	{
		FOVDebugDraw();
	}

	if( cv_ai_state_show.GetBool() )
	{
		const char *statename;
		if( state )
			statename = state->Name();
		else
			statename = "NULL State";

		gameRenderWorld->DrawText( statename, (GetEyePosition() - physicsObj.GetGravityNormal()*15.0f), 0.25f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, gameLocal.msec );
	}

	if ( cv_ai_task_show.GetBool())
	{
		idStr str = idStr::FormatNumber(taskPriority) + "   ";
		str += idStr(task);
		gameRenderWorld->DrawText( str, (GetEyePosition() - physicsObj.GetGravityNormal()*15.0f), 0.25f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, gameLocal.msec );
		if (m_TaskQueue != NULL)
		{
			gameRenderWorld->DrawText( m_TaskQueue->DebuggingInfo().c_str(), (GetEyePosition() - physicsObj.GetGravityNormal()*10.0f), 0.20f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, gameLocal.msec );
		}
	}

	if( cv_ai_alertnum_show.GetBool() )
	{
		gameRenderWorld->DrawText( va("Alert: %f; Index: %d", (float) AI_AlertNum, (int)AI_AlertIndex), (GetEyePosition() - physicsObj.GetGravityNormal()*32.0f), 0.25f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, gameLocal.msec );
	}
	if ( health > 0 )
	{
		idActor::CrashLand( physicsObj, oldOrigin, oldVelocity );
	}
}

/***********************************************************************

	AI script state management

***********************************************************************/

/*
=====================
idAI::LinkScriptVariables
=====================
*/
void idAI::LinkScriptVariables( void )
{
	AI_TALK.LinkTo(				scriptObject, "AI_TALK" );
	AI_DAMAGE.LinkTo(			scriptObject, "AI_DAMAGE" );
	AI_PAIN.LinkTo(				scriptObject, "AI_PAIN" );
	AI_SPECIAL_DAMAGE.LinkTo(	scriptObject, "AI_SPECIAL_DAMAGE" );
	AI_DEAD.LinkTo(				scriptObject, "AI_DEAD" );
	AI_KNOCKEDOUT.LinkTo(		scriptObject, "AI_KNOCKEDOUT" );
	AI_ENEMY_VISIBLE.LinkTo(	scriptObject, "AI_ENEMY_VISIBLE" );
	AI_ENEMY_IN_FOV.LinkTo(		scriptObject, "AI_ENEMY_IN_FOV" );
	AI_ENEMY_DEAD.LinkTo(		scriptObject, "AI_ENEMY_DEAD" );
	AI_MOVE_DONE.LinkTo(		scriptObject, "AI_MOVE_DONE" );
	AI_ONGROUND.LinkTo(			scriptObject, "AI_ONGROUND" );
	AI_ACTIVATED.LinkTo(		scriptObject, "AI_ACTIVATED" );
	AI_FORWARD.LinkTo(			scriptObject, "AI_FORWARD" );
	AI_JUMP.LinkTo(				scriptObject, "AI_JUMP" );
	AI_BLOCKED.LinkTo(			scriptObject, "AI_BLOCKED" );
	AI_DEST_UNREACHABLE.LinkTo( scriptObject, "AI_DEST_UNREACHABLE" );
	AI_HIT_ENEMY.LinkTo(		scriptObject, "AI_HIT_ENEMY" );
	AI_OBSTACLE_IN_PATH.LinkTo(	scriptObject, "AI_OBSTACLE_IN_PATH" );
	AI_PUSHED.LinkTo(			scriptObject, "AI_PUSHED" );

	//this is only set in a given frame
	AI_ALERTED.LinkTo(			scriptObject, "AI_ALERTED" );

	AI_AlertNum.LinkTo(			scriptObject, "AI_AlertNum" );
	AI_AlertIndex.LinkTo(			scriptObject, "AI_AlertIndex" );

	AI_lastAlertPosSearched.LinkTo(			scriptObject, "AI_lastAlertPosSearched");
	AI_chancePerSecond_RandomLookAroundWhileIdle.LinkTo(scriptObject, "AI_chancePerSecond_RandomLookAroundWhileIdle");
	AI_timeOfLastStimulusBark.LinkTo(		scriptObject, "AI_timeOfLastStimulusBark");
	AI_currentAlertLevelDuration.LinkTo(	scriptObject, "AI_currentAlertLevelDuration");
	AI_currentAlertLevelStartTime.LinkTo(	scriptObject, "AI_currentAlertLevelStartTime");

	AI_bMeleeWeapDrawn.LinkTo( scriptObject, "AI_bMeleeWeapDrawn" );
	AI_bRangedWeapDrawn.LinkTo( scriptObject, "AI_bRangedWeapDrawn" );

	stateOfMind_b_enemiesHaveBeenSeen.LinkTo ( scriptObject, "stateOfMind_b_enemiesHaveBeenSeen" );
	stateOfMind_b_itemsHaveBeenStolen.LinkTo ( scriptObject, "stateOfMind_b_itemsHaveBeenStolen" );
	stateOfMind_count_evidenceOfIntruders.LinkTo ( scriptObject, "stateOfMind_count_evidenceOfIntruders" );


	//these are set until unset by the script
	AI_HEARDSOUND.LinkTo(		scriptObject, "AI_HEARDSOUND");
	AI_VISALERT.LinkTo(			scriptObject, "AI_VISALERT");
	AI_TACTALERT.LinkTo(		scriptObject, "AI_TACTALERT");
	AI_ALERTED_BY_PLAYER.LinkTo(	scriptObject, "AI_ALERTED_BY_PLAYER");

	AI_CROUCH.LinkTo(			scriptObject, "AI_CROUCH");
	AI_RUN.LinkTo(				scriptObject, "AI_RUN");
	AI_CREEP.LinkTo(			scriptObject, "AI_CREEP");
}

/*
=====================
idAI::UpdateAIScript
=====================
*/
void idAI::UpdateAIScript( void )
{
	UpdateScript();

	// clear the hit enemy flag so we catch the next time we hit someone
	AI_HIT_ENEMY = false;

	if ( allowHiddenMovement || !IsHidden() ) {
		// update the animstate if we're not hidden
		UpdateAnimState();
	}

	// Clear DarkMod per frame vars now that the script was updated.
	AI_ALERTED = false;
	m_AlertNumThisFrame = 0;
	m_AlertedByActor = NULL;
}

/***********************************************************************

	navigation

***********************************************************************/

/*
============
idAI::KickObstacles
============
*/
void idAI::KickObstacles( const idVec3 &dir, float force, idEntity *alwaysKick ) {
	int i, numListedClipModels;
	idBounds clipBounds;
	idEntity *obEnt;
	idClipModel *clipModel;
	idClipModel *clipModelList[ MAX_GENTITIES ];
	int clipmask;
	idVec3 org;
	idVec3 forceVec;
	idVec3 delta;
	idVec2 perpendicular;

	org = physicsObj.GetOrigin();

	// find all possible obstacles
	clipBounds = physicsObj.GetAbsBounds();
	clipBounds.TranslateSelf( dir * 32.0f );
	clipBounds.ExpandSelf( 8.0f );
	clipBounds.AddPoint( org );
	clipmask = physicsObj.GetClipMask();
	numListedClipModels = gameLocal.clip.ClipModelsTouchingBounds( clipBounds, clipmask, clipModelList, MAX_GENTITIES );
	for ( i = 0; i < numListedClipModels; i++ ) {
		clipModel = clipModelList[i];
		obEnt = clipModel->GetEntity();
		if ( obEnt == alwaysKick ) {
			// we'll kick this one outside the loop
			continue;
		}

		if ( !clipModel->IsTraceModel() ) {
			continue;
		}

		if ( obEnt->IsType( idMoveable::Type ) && obEnt->GetPhysics()->IsPushable() ) {
			delta = obEnt->GetPhysics()->GetOrigin() - org;
			delta.NormalizeFast();
			perpendicular.x = -delta.y;
			perpendicular.y = delta.x;
			delta.z += 0.5f;
			delta.ToVec2() += perpendicular * gameLocal.random.CRandomFloat() * 0.5f;
			forceVec = delta * force * obEnt->GetPhysics()->GetMass();
			obEnt->ApplyImpulse( this, 0, obEnt->GetPhysics()->GetOrigin(), forceVec );
		}
	}

	if ( alwaysKick ) {
		delta = alwaysKick->GetPhysics()->GetOrigin() - org;
		delta.NormalizeFast();
		perpendicular.x = -delta.y;
		perpendicular.y = delta.x;
		delta.z += 0.5f;
		delta.ToVec2() += perpendicular * gameLocal.random.CRandomFloat() * 0.5f;
		forceVec = delta * force * alwaysKick->GetPhysics()->GetMass();
		alwaysKick->ApplyImpulse( this, 0, alwaysKick->GetPhysics()->GetOrigin(), forceVec );
	}
}

/*
============
ValidForBounds
============
*/
bool ValidForBounds( const idAASSettings *settings, const idBounds &bounds ) {
	int i;

	for ( i = 0; i < 3; i++ ) {
		if ( bounds[0][i] < settings->boundingBoxes[0][0][i] ) {
			return false;
		}
		if ( bounds[1][i] > settings->boundingBoxes[0][1][i] ) {
			return false;
		}
	}
	return true;
}

/*
=====================
idAI::SetAAS
=====================
*/
void idAI::SetAAS( void ) {
	idStr use_aas;

	spawnArgs.GetString( "use_aas", NULL, use_aas );
	aas = gameLocal.GetAAS( use_aas );
	if ( aas ) {
		const idAASSettings *settings = aas->GetSettings();
		if ( settings ) {
			if ( !ValidForBounds( settings, physicsObj.GetBounds() ) ) {
				gameLocal.Error( "%s cannot use use_aas %s\n", name.c_str(), use_aas.c_str() );
			}
			float height = settings->maxStepHeight;
			physicsObj.SetMaxStepHeight( height );
			return;
		} else {
			aas = NULL;
		}
	}
	gameLocal.Printf( "WARNING: %s has no AAS file\n", name.c_str() );
}

/*
=====================
idAI::DrawRoute
=====================
*/
void idAI::DrawRoute( void ) const {
	if ( aas && move.toAreaNum && move.moveCommand != MOVE_NONE && move.moveCommand != MOVE_WANDER && move.moveCommand != MOVE_FACE_ENEMY && move.moveCommand != MOVE_FACE_ENTITY && move.moveCommand != MOVE_TO_POSITION_DIRECT && move.moveCommand != MOVE_VECTOR ) 
	{
		if ( move.moveType == MOVETYPE_FLY ) {
			aas->ShowFlyPath( physicsObj.GetOrigin(), move.toAreaNum, move.moveDest );
		} else {
			aas->ShowWalkPath( physicsObj.GetOrigin(), move.toAreaNum, move.moveDest );
		}
	}
}

/*
=====================
idAI::ReachedPos
=====================
*/
bool idAI::ReachedPos( const idVec3 &pos, const moveCommand_t moveCommand ) const {
	if ( move.moveType == MOVETYPE_SLIDE ) {
		idBounds bnds( idVec3( -4, -4.0f, -8.0f ), idVec3( 4.0f, 4.0f, 64.0f ) );
		bnds.TranslateSelf( physicsObj.GetOrigin() );
		if ( bnds.ContainsPoint( pos ) ) {
			return true;
		}
	} else {
		if ( ( moveCommand == MOVE_TO_ENEMY ) || ( moveCommand == MOVE_TO_ENTITY ) ) {
			if ( physicsObj.GetAbsBounds().IntersectsBounds( idBounds( pos ).Expand( 8.0f ) ) ) {
				return true;
			}
		} else {
			//idBounds bnds( idVec3( -16.0, -16.0f, -8.0f ), idVec3( 16.0, 16.0f, 64.0f ) );
			// SZ: We are using AAS48 for our characters so we are changing this to 24 in each direction
			// greebo: TODO: We are using AAS32 as of August 2007
			idBounds bnds( idVec3( -24.0, -24.0f, -8.0f ), idVec3( 24.0, 24.0f, 128.0f ) );

			bnds.TranslateSelf( physicsObj.GetOrigin() );

			/*
			// SZ: Padding height so that we reached it if we are underneath it and can't fly
			// SZ: Not good, could be ledge above us. Better to detect this case and set move target to
			// ground underneath object
			*/
			/*
			if (move.moveType != MOVETYPE_FLY)
			{
				idVec3 abovePoint;
				abovePoint = physicsObj.GetOrigin();
				abovePoint -= physicsObj.GetGravityNormal() * 300.0;
				bnds.AddPoint (abovePoint);
			}
			*/

			if ( bnds.ContainsPoint( pos ) ) {
				return true;
			}
		}
	}
	return false;
}

/*
=====================
idAI::PointReachableAreaNum
=====================
*/
int idAI::PointReachableAreaNum( const idVec3 &pos, const float boundsScale ) const {
	int areaNum;
	idVec3 size;
	idBounds bounds;

	if ( !aas ) {
		return 0;
	}

	size = aas->GetSettings()->boundingBoxes[0][1] * boundsScale;
	bounds[0] = -size;
	size.z = 32.0f;
	bounds[1] = size;

	if ( move.moveType == MOVETYPE_FLY ) {
		areaNum = aas->PointReachableAreaNum( pos, bounds, AREA_REACHABLE_WALK | AREA_REACHABLE_FLY );
	} else {
		areaNum = aas->PointReachableAreaNum( pos, bounds, AREA_REACHABLE_WALK );
	}

	return areaNum;
}

/*
=====================
idAI::PathToGoal
=====================
*/
bool idAI::PathToGoal( aasPath_t &path, int areaNum, const idVec3 &origin, int goalAreaNum, const idVec3 &goalOrigin ) const {
	idVec3 org;
	idVec3 goal;

	if ( !aas ) {
		return false;
	}

	org = origin;
	aas->PushPointIntoAreaNum( areaNum, org );
	if ( !areaNum ) {
		return false;
	}

	goal = goalOrigin;
	aas->PushPointIntoAreaNum( goalAreaNum, goal );
	if ( !goalAreaNum ) {
		return false;
	}

	if ( move.moveType == MOVETYPE_FLY ) {
		return aas->FlyPathToGoal( path, areaNum, org, goalAreaNum, goal, travelFlags );
	} else {
		return aas->WalkPathToGoal( path, areaNum, org, goalAreaNum, goal, travelFlags );
	}
}

/*
=====================
idAI::TravelDistance

Returns the approximate travel distance from one position to the goal, or if no AAS, the straight line distance.

This is feakin' slow, so it's not good to do it too many times per frame.  It also is slower the further you
are from the goal, so try to break the goals up into shorter distances.
=====================
*/
float idAI::TravelDistance( const idVec3 &start, const idVec3 &end ) const {
	int			fromArea;
	int			toArea;
	float		dist;
	idVec2		delta;
	aasPath_t	path;

	if ( !aas ) {
		// no aas, so just take the straight line distance
		delta = end.ToVec2() - start.ToVec2();
		dist = delta.LengthFast();

		if ( ai_debugMove.GetBool() ) {
			gameRenderWorld->DebugLine( colorBlue, start, end, gameLocal.msec, false );
			gameRenderWorld->DrawText( va( "%d", ( int )dist ), ( start + end ) * 0.5f, 0.1f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3() );
		}

		return dist;
	}

	fromArea = PointReachableAreaNum( start );
	toArea = PointReachableAreaNum( end );

	if ( !fromArea || !toArea ) {
		// can't seem to get there
		return -1;
	}

	if ( fromArea == toArea ) {
		// same area, so just take the straight line distance
		delta = end.ToVec2() - start.ToVec2();
		dist = delta.LengthFast();

		if ( ai_debugMove.GetBool() ) {
			gameRenderWorld->DebugLine( colorBlue, start, end, gameLocal.msec, false );
			gameRenderWorld->DrawText( va( "%d", ( int )dist ), ( start + end ) * 0.5f, 0.1f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3() );
		}

		return dist;
	}

	idReachability *reach;
	int travelTime;
	if ( !aas->RouteToGoalArea( fromArea, start, toArea, travelFlags, travelTime, &reach ) ) {
		return -1;
	}

	if ( ai_debugMove.GetBool() ) {
		if ( move.moveType == MOVETYPE_FLY ) {
			aas->ShowFlyPath( start, toArea, end );
		} else {
			aas->ShowWalkPath( start, toArea, end );
		}
	}

	return travelTime;
}

/*
=====================
idAI::StopMove
=====================
*/
void idAI::StopMove( moveStatus_t status ) {
	AI_MOVE_DONE		= true;
	AI_FORWARD			= false;
	move.moveCommand	= MOVE_NONE;
	move.moveStatus		= status;
	move.toAreaNum		= 0;
	move.goalEntity		= NULL;
	move.moveDest		= physicsObj.GetOrigin();
	AI_DEST_UNREACHABLE	= false;
	AI_OBSTACLE_IN_PATH = false;
	AI_BLOCKED			= false;
	move.startTime		= gameLocal.time;
	move.duration		= 0;
	move.range			= 0.0f;
	move.speed			= 0.0f;
	move.anim			= 0;
	move.moveDir.Zero();
	move.lastMoveOrigin.Zero();
	move.lastMoveTime	= gameLocal.time;
}

/*
=====================
idAI::FaceEnemy

Continually face the enemy's last known position.  MoveDone is always true in this case.
=====================
*/
bool idAI::FaceEnemy( void ) {
	idActor *enemyEnt = enemy.GetEntity();
	if ( !enemyEnt ) {
		StopMove( MOVE_STATUS_DEST_NOT_FOUND );
		return false;
	}

	TurnToward( lastVisibleEnemyPos );
	move.goalEntity		= enemyEnt;
	move.moveDest		= physicsObj.GetOrigin();
	move.moveCommand	= MOVE_FACE_ENEMY;
	move.moveStatus		= MOVE_STATUS_WAITING;
	move.startTime		= gameLocal.time;
	move.speed			= 0.0f;
	AI_MOVE_DONE		= true;
	AI_FORWARD			= false;
	AI_DEST_UNREACHABLE = false;

	return true;
}

/*
=====================
idAI::FaceEntity

Continually face the entity position.  MoveDone is always true in this case.
=====================
*/
bool idAI::FaceEntity( idEntity *ent ) {
	if ( !ent ) {
		StopMove( MOVE_STATUS_DEST_NOT_FOUND );
		return false;
	}

	idVec3 entityOrg = ent->GetPhysics()->GetOrigin();
	TurnToward( entityOrg );
	move.goalEntity		= ent;
	move.moveDest		= physicsObj.GetOrigin();
	move.moveCommand	= MOVE_FACE_ENTITY;
	move.moveStatus		= MOVE_STATUS_WAITING;
	move.startTime		= gameLocal.time;
	move.speed			= 0.0f;
	AI_MOVE_DONE		= true;
	AI_FORWARD			= false;
	AI_DEST_UNREACHABLE = false;

	return true;
}

/*
=====================
idAI::DirectMoveToPosition
=====================
*/
bool idAI::DirectMoveToPosition( const idVec3 &pos ) {
	if ( ReachedPos( pos, move.moveCommand ) ) {
		StopMove( MOVE_STATUS_DONE );
		return true;
	}

	move.moveDest		= pos;
	move.goalEntity		= NULL;
	move.moveCommand	= MOVE_TO_POSITION_DIRECT;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.startTime		= gameLocal.time;
	move.speed			= fly_speed;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= true;

	if ( move.moveType == MOVETYPE_FLY ) {
		idVec3 dir = pos - physicsObj.GetOrigin();
		dir.Normalize();
		dir *= fly_speed;
		physicsObj.SetLinearVelocity( dir );
	}

	return true;
}

/*
=====================
idAI::MoveToEnemyHeight
=====================
*/
bool idAI::MoveToEnemyHeight( void ) {
	idActor	*enemyEnt = enemy.GetEntity();

	if ( !enemyEnt || ( move.moveType != MOVETYPE_FLY ) ) {
		StopMove( MOVE_STATUS_DEST_NOT_FOUND );
		return false;
	}

	move.moveDest.z		= lastVisibleEnemyPos.z + enemyEnt->EyeOffset().z + fly_offset;
	move.goalEntity		= enemyEnt;
	move.moveCommand	= MOVE_TO_ENEMYHEIGHT;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.startTime		= gameLocal.time;
	move.speed			= 0.0f;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= false;

	return true;
}

/*
=====================
idAI::MoveToEnemy
=====================
*/
bool idAI::MoveToEnemy( void ) {
	int			areaNum;
	aasPath_t	path;
	idActor		*enemyEnt = enemy.GetEntity();

	if ( !enemyEnt ) {
		StopMove( MOVE_STATUS_DEST_NOT_FOUND );
		return false;
	}

	if ( ReachedPos( lastVisibleReachableEnemyPos, MOVE_TO_ENEMY ) ) {
		if ( !ReachedPos( lastVisibleEnemyPos, MOVE_TO_ENEMY ) || !AI_ENEMY_VISIBLE ) {
			StopMove( MOVE_STATUS_DEST_UNREACHABLE );
			AI_DEST_UNREACHABLE = true;
			return false;
		}
		StopMove( MOVE_STATUS_DONE );
		return true;
	}

	idVec3 pos = lastVisibleReachableEnemyPos;

	move.toAreaNum = 0;
	if ( aas ) {
		move.toAreaNum = PointReachableAreaNum( pos );
		aas->PushPointIntoAreaNum( move.toAreaNum, pos );

		areaNum	= PointReachableAreaNum( physicsObj.GetOrigin() );
		if ( !PathToGoal( path, areaNum, physicsObj.GetOrigin(), move.toAreaNum, pos ) ) {
			AI_DEST_UNREACHABLE = true;
			return false;
		}
	}

	if ( !move.toAreaNum ) {
		// if only trying to update the enemy position
		if ( move.moveCommand == MOVE_TO_ENEMY ) {
			if ( !aas ) {
				// keep the move destination up to date for wandering
				move.moveDest = pos;
			}
			return false;
		}

		if ( !NewWanderDir( pos ) ) {
			StopMove( MOVE_STATUS_DEST_UNREACHABLE );
			AI_DEST_UNREACHABLE = true;
			return false;
		}
	}

	if ( move.moveCommand != MOVE_TO_ENEMY ) {
		move.moveCommand	= MOVE_TO_ENEMY;
		move.startTime		= gameLocal.time;
	}

	move.moveDest		= pos;
	move.goalEntity		= enemyEnt;
	move.speed			= fly_speed;
	move.moveStatus		= MOVE_STATUS_MOVING;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= true;

	return true;
}

/*
=====================
idAI::MoveToEntity
=====================
*/
bool idAI::MoveToEntity( idEntity *ent ) {
	int			areaNum;
	aasPath_t	path;
	idVec3		pos;

	if ( !ent ) {
		StopMove( MOVE_STATUS_DEST_NOT_FOUND );
		return false;
	}

	pos = ent->GetPhysics()->GetOrigin();
	if ( ( move.moveType != MOVETYPE_FLY ) && ( ( move.moveCommand != MOVE_TO_ENTITY ) || ( move.goalEntityOrigin != pos ) ) ) {
		ent->GetFloorPos( 64.0f, pos );
	}

	if ( ReachedPos( pos, MOVE_TO_ENTITY ) ) {
		StopMove( MOVE_STATUS_DONE );
		return true;
	}

	move.toAreaNum = 0;
	if ( aas ) {
		move.toAreaNum = PointReachableAreaNum( pos );
		aas->PushPointIntoAreaNum( move.toAreaNum, pos );

		areaNum	= PointReachableAreaNum( physicsObj.GetOrigin() );
		if ( !PathToGoal( path, areaNum, physicsObj.GetOrigin(), move.toAreaNum, pos ) ) {
			AI_DEST_UNREACHABLE = true;
			return false;
		}
	}

	if ( !move.toAreaNum ) {
		// if only trying to update the entity position
		if ( move.moveCommand == MOVE_TO_ENTITY ) {
			if ( !aas ) {
				// keep the move destination up to date for wandering
				move.moveDest = pos;
			}
			return false;
		}

		if ( !NewWanderDir( pos ) ) {
			StopMove( MOVE_STATUS_DEST_UNREACHABLE );
			AI_DEST_UNREACHABLE = true;
			return false;
		}
	}

	if ( ( move.moveCommand != MOVE_TO_ENTITY ) || ( move.goalEntity.GetEntity() != ent ) ) {
		move.startTime		= gameLocal.time;
		move.goalEntity		= ent;
		move.moveCommand	= MOVE_TO_ENTITY;
	}

	move.moveDest			= pos;
	move.goalEntityOrigin	= ent->GetPhysics()->GetOrigin();
	move.moveStatus			= MOVE_STATUS_MOVING;
	move.speed				= fly_speed;
	AI_MOVE_DONE			= false;
	AI_DEST_UNREACHABLE		= false;
	AI_FORWARD				= true;

	return true;
}

bool idAI::Flee(idEntity* entityToFleeFrom, int algorithm, int distanceOption)
{
	EscapePointAlgorithm algorithmType = static_cast<EscapePointAlgorithm>(algorithm);
	
	if ( !aas || !entityToFleeFrom ) {
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	// The current AI origin
	const idVec3& org = physicsObj.GetOrigin();

	// These two will hold the travel destination info
	idVec3 moveDest;
	int moveAreaNum(-1);

	if (algorithmType != FIND_AAS_AREA_FAR_FROM_THREAT)
	{
		// Use the EscapePointManager to locate a pathFlee entity

		// Setup the escape conditions
		EscapeConditions conditions;
		
		conditions.fromEntity = entityToFleeFrom;
		conditions.aas = aas;
		conditions.fromPosition = org;
		conditions.self = this;
		conditions.distanceOption = static_cast<EscapeDistanceOption>(distanceOption);
		conditions.algorithm = algorithmType;
		conditions.minDistanceToThreat = 400.0f;

		// Request the escape goal from the manager
		EscapeGoal goal = gameLocal.m_EscapePointManager->GetEscapeGoal(conditions);

		if (goal.escapePointId == -1)
		{
			// Invalid escape point id returned
			return false;
		}

		// Get the actual point (this should never be NULL)
		EscapePoint* targetPoint = gameLocal.m_EscapePointManager->GetEscapePoint(goal.escapePointId);

		moveDest = targetPoint->origin;
		moveAreaNum = targetPoint->areaNum;
	}
	else 
	{
		// algorithm == FIND_AAS_AREA_FAR_FROM_THREAT

		int	areaNum = PointReachableAreaNum(org);
		
		// consider the entity the monster is getting close to as an obstacle
		aasObstacle_t obstacle;
		obstacle.absBounds = entityToFleeFrom->GetPhysics()->GetAbsBounds();

		idVec3 pos;

		if ( entityToFleeFrom == enemy.GetEntity() ) {
			pos = lastVisibleEnemyPos;
		} else {
			pos = entityToFleeFrom->GetPhysics()->GetOrigin();
		}

		// Setup the evaluator class
		tdmAASFindEscape findEscapeArea(pos, org, distanceOption, 100);
		aasGoal_t dummy;
		aas->FindNearestGoal(dummy, areaNum, org, pos, travelFlags, &obstacle, 1, findEscapeArea);

		aasGoal_t& goal = findEscapeArea.GetEscapeGoal();

		if (goal.areaNum == -1)
		{
			// Invalid escape point id returned
			return false;
		}

		moveDest = goal.origin;
		moveAreaNum = goal.areaNum;
	}

	if ( ReachedPos( moveDest, move.moveCommand ) ) {
		StopMove( MOVE_STATUS_DONE );
		return true;
	}

	move.moveDest		= moveDest;
	move.toAreaNum		= moveAreaNum;
	move.goalEntity		= entityToFleeFrom;
	move.moveCommand	= MOVE_FLEE;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.range			= MAX_FLEE_DISTANCE;
	move.speed			= fly_speed;
	move.startTime		= gameLocal.time;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= true;

	return true;
}

/*
=====================
idAI::MoveOutOfRange
=====================
*/
bool idAI::MoveOutOfRange( idEntity *ent, float range ) {
	int				areaNum;
	aasObstacle_t	obstacle;
	aasGoal_t		goal;
	idBounds		bounds;
	idVec3			pos;

	if ( !aas || !ent ) {
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	const idVec3 &org = physicsObj.GetOrigin();
	areaNum	= PointReachableAreaNum( org );

	// consider the entity the monster is getting close to as an obstacle
	obstacle.absBounds = ent->GetPhysics()->GetAbsBounds();

	if ( ent == enemy.GetEntity() ) {
		pos = lastVisibleEnemyPos;
	} else {
		pos = ent->GetPhysics()->GetOrigin();
	}

	idAASFindAreaOutOfRange findGoal( pos, range );
	if ( !aas->FindNearestGoal( goal, areaNum, org, pos, travelFlags, &obstacle, 1, findGoal ) ) {
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	DM_LOG(LC_AI, LT_DEBUG).LogString("Best fleeing location is: %f %f %f in area %d\r", goal.origin.x, goal.origin.y, goal.origin.z, goal.areaNum);

	if ( ReachedPos( goal.origin, move.moveCommand ) ) {
		StopMove( MOVE_STATUS_DONE );
		return true;
	}

	move.moveDest		= goal.origin;
	move.toAreaNum		= goal.areaNum;
	move.goalEntity		= ent;
	move.moveCommand	= MOVE_OUT_OF_RANGE;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.range			= range;
	move.speed			= fly_speed;
	move.startTime		= gameLocal.time;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= true;

	return true;
}

/*
=====================
idAI::MoveToAttackPosition
=====================
*/
bool idAI::MoveToAttackPosition( idEntity *ent, int attack_anim ) {
	int				areaNum;
	aasObstacle_t	obstacle;
	aasGoal_t		goal;
	idBounds		bounds;
	idVec3			pos;

	if ( !aas || !ent ) {
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	const idVec3 &org = physicsObj.GetOrigin();
	areaNum	= PointReachableAreaNum( org );

	// consider the entity the monster is getting close to as an obstacle
	obstacle.absBounds = ent->GetPhysics()->GetAbsBounds();

	if ( ent == enemy.GetEntity() ) {
		pos = lastVisibleEnemyPos;
	} else {
		pos = ent->GetPhysics()->GetOrigin();
	}

	idAASFindAttackPosition findGoal( this, physicsObj.GetGravityAxis(), ent, pos, missileLaunchOffset[ attack_anim ] );
	if ( !aas->FindNearestGoal( goal, areaNum, org, pos, travelFlags, &obstacle, 1, findGoal ) ) {
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	move.moveDest		= goal.origin;
	move.toAreaNum		= goal.areaNum;
	move.goalEntity		= ent;
	move.moveCommand	= MOVE_TO_ATTACK_POSITION;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.speed			= fly_speed;
	move.startTime		= gameLocal.time;
	move.anim			= attack_anim;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= true;

	return true;
}

/*
=====================
idAI::MoveToPosition
=====================
*/
bool idAI::MoveToPosition( const idVec3 &pos ) {
	idVec3		org;
	int			areaNum;
	aasPath_t	path;

	if ( ReachedPos( pos, move.moveCommand ) ) {
		StopMove( MOVE_STATUS_DONE );
		return true;
	}

	org = pos;
	move.toAreaNum = 0;
	if ( aas ) {
		move.toAreaNum = PointReachableAreaNum( org );
		aas->PushPointIntoAreaNum( move.toAreaNum, org );

		areaNum	= PointReachableAreaNum( physicsObj.GetOrigin() );
		if ( !PathToGoal( path, areaNum, physicsObj.GetOrigin(), move.toAreaNum, org ) ) {
			StopMove( MOVE_STATUS_DEST_UNREACHABLE );
			AI_DEST_UNREACHABLE = true;
			return false;
		}
	}

	if ( !move.toAreaNum && !NewWanderDir( org ) ) {
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	move.moveDest		= org;
	move.goalEntity		= NULL;
	move.moveCommand	= MOVE_TO_POSITION;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.startTime		= gameLocal.time;
	move.speed			= fly_speed;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= true;

	return true;
}

/*
=====================
idAI::MoveToCover
=====================
*/
bool idAI::MoveToCover( idEntity *hideFromEnt, const idVec3 &hideFromPos ) {
	int				areaNum;
	aasObstacle_t	obstacle;
	aasGoal_t		hideGoal;
	idBounds		bounds;
	//common->Printf("MoveToCover called... ");

	if ( !aas || !hideFromEnt ) {
		common->Printf("MoveToCover failed: null aas or entity\n");
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	const idVec3 &org = physicsObj.GetOrigin();
	areaNum	= PointReachableAreaNum( org );

	// consider the entity the monster tries to hide from as an obstacle
	obstacle.absBounds = hideFromEnt->GetPhysics()->GetAbsBounds();

	idAASFindCover findCover( this, hideFromEnt, hideFromPos );
	if ( !aas->FindNearestGoal( hideGoal, areaNum, org, hideFromPos, travelFlags, &obstacle, 1, findCover, spawnArgs.GetInt("taking_cover_max_cost") ) ) {
		//common->Printf("MoveToCover failed: destination unreachable\n");
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	if ( ReachedPos( hideGoal.origin, move.moveCommand ) ) {
		//common->Printf("MoveToCover succeeded: Already at hide position\n");
		StopMove( MOVE_STATUS_DONE );
		return true;
	}

	move.moveDest		= hideGoal.origin;
	move.toAreaNum		= hideGoal.areaNum;
	move.goalEntity		= hideFromEnt;
	move.moveCommand	= MOVE_TO_COVER;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.startTime		= gameLocal.time;
	move.speed			= fly_speed;
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= true;
	//common->Printf("MoveToCover succeeded: Now moving into cover\n");

	return true;
}

/*
=====================
idAI::SlideToPosition
=====================
*/
bool idAI::SlideToPosition( const idVec3 &pos, float time ) {
	StopMove( MOVE_STATUS_DONE );

	move.moveDest		= pos;
	move.goalEntity		= NULL;
	move.moveCommand	= MOVE_SLIDE_TO_POSITION;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.startTime		= gameLocal.time;
	move.duration		= idPhysics::SnapTimeToPhysicsFrame( SEC2MS( time ) );
	AI_MOVE_DONE		= false;
	AI_DEST_UNREACHABLE = false;
	AI_FORWARD			= false;

	if ( move.duration > 0 ) {
		move.moveDir = ( pos - physicsObj.GetOrigin() ) / MS2SEC( move.duration );
		if ( move.moveType != MOVETYPE_FLY ) {
			move.moveDir.z = 0.0f;
		}
		move.speed = move.moveDir.LengthFast();
	}

	return true;
}

/*
=====================
idAI::WanderAround
=====================
*/
bool idAI::WanderAround( void ) {
	StopMove( MOVE_STATUS_DONE );

	move.moveDest = physicsObj.GetOrigin() + viewAxis[ 0 ] * physicsObj.GetGravityAxis() * 256.0f;
	if ( !NewWanderDir( move.moveDest ) ) {
		StopMove( MOVE_STATUS_DEST_UNREACHABLE );
		AI_DEST_UNREACHABLE = true;
		return false;
	}

	move.moveCommand	= MOVE_WANDER;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.startTime		= gameLocal.time;
	move.speed			= fly_speed;
	AI_MOVE_DONE		= false;
	AI_FORWARD			= true;

	return true;
}

/*
=====================
idAI::MoveDone
=====================
*/
bool idAI::MoveDone( void ) const {
	return ( move.moveCommand == MOVE_NONE );
}

/*
================
idAI::StepDirection
================
*/
bool idAI::StepDirection( float dir ) {
	predictedPath_t path;
	idVec3 org;

	move.wanderYaw = dir;
	move.moveDir = idAngles( 0, move.wanderYaw, 0 ).ToForward();

	org = physicsObj.GetOrigin();

	idAI::PredictPath( this, aas, org, move.moveDir * 48.0f, 1000, 1000, ( move.moveType == MOVETYPE_FLY ) ? SE_BLOCKED : ( SE_ENTER_OBSTACLE | SE_BLOCKED | SE_ENTER_LEDGE_AREA ), path );

	if ( path.blockingEntity && ( ( move.moveCommand == MOVE_TO_ENEMY ) || ( move.moveCommand == MOVE_TO_ENTITY ) ) && ( path.blockingEntity == move.goalEntity.GetEntity() ) ) {
		// don't report being blocked if we ran into our goal entity
		return true;
	}

	// SZ: January 7, 2006: Wandering uses this, and currently it fails the test if it would bump into another AI.
	// If we are wandering, we want to make sure we can still bump into the player or enemy AIs, both of which are idActor
	// based.
	if (path.blockingEntity && (move.moveCommand == MOVE_WANDER))
	{
		// What type of entity is it?
		if
		(
			(path.blockingEntity->IsType(idActor::Type) )
		)
		{
			// Bump into enemies all you want while wandering
			int otherTeam = ((idActor*) (path.blockingEntity))->team;
			if (gameLocal.m_RelationsManager->IsEnemy( team, otherTeam ))
			{
				return true;
			}
		}

	} // End wandering case

	if ( ( move.moveType == MOVETYPE_FLY ) && ( path.endEvent == SE_BLOCKED ) ) {
		float z;

		move.moveDir = path.endVelocity * 1.0f / 48.0f;

		// trace down to the floor and see if we can go forward
		idAI::PredictPath( this, aas, org, idVec3( 0.0f, 0.0f, -1024.0f ), 1000, 1000, SE_BLOCKED, path );

		idVec3 floorPos = path.endPos;
		idAI::PredictPath( this, aas, floorPos, move.moveDir * 48.0f, 1000, 1000, SE_BLOCKED, path );
		if ( !path.endEvent ) {
			move.moveDir.z = -1.0f;
			return true;
		}

		// trace up to see if we can go over something and go forward
		idAI::PredictPath( this, aas, org, idVec3( 0.0f, 0.0f, 256.0f ), 1000, 1000, SE_BLOCKED, path );

		idVec3 ceilingPos = path.endPos;

		for( z = org.z; z <= ceilingPos.z + 64.0f; z += 64.0f ) {
			idVec3 start;
			if ( z <= ceilingPos.z ) {
				start.x = org.x;
				start.y = org.y;
                start.z = z;
			} else {
				start = ceilingPos;
			}
			idAI::PredictPath( this, aas, start, move.moveDir * 48.0f, 1000, 1000, SE_BLOCKED, path );
			if ( !path.endEvent ) {
				move.moveDir.z = 1.0f;
				return true;
			}
		}
		return false;
	}

	return ( path.endEvent == 0 );
}
/*
================
idAI::MoveAlongVector
================
*/
bool idAI::MoveAlongVector( float yaw ) 
{
	StopMove( MOVE_STATUS_DONE );
	move.moveDir = idAngles( 0, yaw, 0 ).ToForward();
	move.moveDest = physicsObj.GetOrigin() + move.moveDir * 256.0f;

	move.moveCommand	= MOVE_VECTOR;
	move.moveStatus		= MOVE_STATUS_MOVING;
	move.startTime		= gameLocal.time;
	move.speed			= fly_speed;
	AI_MOVE_DONE		= false;
	AI_FORWARD			= true;

	return true;
}
/*
================
idAI::NewWanderDir
================
*/
bool idAI::NewWanderDir( const idVec3 &dest ) {
	float	deltax, deltay;
	float	d[ 3 ];
	float	tdir, olddir, turnaround;

	move.nextWanderTime = gameLocal.time + ( gameLocal.random.RandomFloat() * 500 + 500 );

	olddir = idMath::AngleNormalize360( ( int )( current_yaw / 45 ) * 45 );
	turnaround = idMath::AngleNormalize360( olddir - 180 );

	idVec3 org = physicsObj.GetOrigin();
	deltax = dest.x - org.x;
	deltay = dest.y - org.y;
	if ( deltax > 10 ) {
		d[ 1 ]= 0;
	} else if ( deltax < -10 ) {
		d[ 1 ] = 180;
	} else {
		d[ 1 ] = DI_NODIR;
	}

	if ( deltay < -10 ) {
		d[ 2 ] = 270;
	} else if ( deltay > 10 ) {
		d[ 2 ] = 90;
	} else {
		d[ 2 ] = DI_NODIR;
	}

	// try direct route
	if ( d[ 1 ] != DI_NODIR && d[ 2 ] != DI_NODIR ) {
		if ( d[ 1 ] == 0 ) {
			tdir = d[ 2 ] == 90 ? 45 : 315;
		} else {
			tdir = d[ 2 ] == 90 ? 135 : 215;
		}

		if ( tdir != turnaround && StepDirection( tdir ) ) {
			return true;
		}
	}

	// try other directions
	if ( ( gameLocal.random.RandomInt() & 1 ) || abs( deltay ) > abs( deltax ) ) {
		tdir = d[ 1 ];
		d[ 1 ] = d[ 2 ];
		d[ 2 ] = tdir;
	}

	if ( d[ 1 ] != DI_NODIR && d[ 1 ] != turnaround && StepDirection( d[1] ) ) {
		return true;
	}

	if ( d[ 2 ] != DI_NODIR && d[ 2 ] != turnaround	&& StepDirection( d[ 2 ] ) ) {
		return true;
	}

	// there is no direct path to the player, so pick another direction
	if ( olddir != DI_NODIR && StepDirection( olddir ) ) {
		return true;
	}

	 // randomly determine direction of search
	if ( gameLocal.random.RandomInt() & 1 ) {
		for( tdir = 0; tdir <= 315; tdir += 45 ) {
			if ( tdir != turnaround && StepDirection( tdir ) ) {
                return true;
			}
		}
	} else {
		for ( tdir = 315; tdir >= 0; tdir -= 45 ) {
			if ( tdir != turnaround && StepDirection( tdir ) ) {
				return true;
			}
		}
	}

	if ( turnaround != DI_NODIR && StepDirection( turnaround ) ) {
		return true;
	}

	// can't move
	StopMove( MOVE_STATUS_DEST_UNREACHABLE );
	return false;
}

/*
=====================
idAI::GetMovePos
=====================
*/
bool idAI::GetMovePos( idVec3 &seekPos ) {
	int			areaNum;
	aasPath_t	path;
	bool		result;
	idVec3		org;

	org = physicsObj.GetOrigin();
	seekPos = org;

	switch( move.moveCommand ) {
	case MOVE_NONE :
		seekPos = move.moveDest;
		return false;
		break;

	case MOVE_FACE_ENEMY :
	case MOVE_FACE_ENTITY :
		seekPos = move.moveDest;
		return false;
		break;

	case MOVE_TO_POSITION_DIRECT :
		seekPos = move.moveDest;
		if ( ReachedPos( move.moveDest, move.moveCommand ) ) {
			StopMove( MOVE_STATUS_DONE );
		}
		return false;
		break;

	case MOVE_SLIDE_TO_POSITION :
		seekPos = org;
		return false;
		break;

	case MOVE_VECTOR :
		seekPos = move.moveDest;
		return true;
		break;
	default:
		break; // Handled below (note the returns in all cases above)
		// (default case added to suppress GCC warnings)
	}

	if ( move.moveCommand == MOVE_TO_ENTITY ) {
		MoveToEntity( move.goalEntity.GetEntity() );
	}

	move.moveStatus = MOVE_STATUS_MOVING;
	result = false;
	if ( gameLocal.time > move.blockTime ) {
		if ( move.moveCommand == MOVE_WANDER ) {
			move.moveDest = org + viewAxis[ 0 ] * physicsObj.GetGravityAxis() * 256.0f;
		} else {
			if ( ReachedPos( move.moveDest, move.moveCommand ) ) {
				StopMove( MOVE_STATUS_DONE );
				seekPos	= org;
				return false;
			}
		}

		if ( aas && move.toAreaNum ) {
			areaNum	= PointReachableAreaNum( org );
			if ( PathToGoal( path, areaNum, org, move.toAreaNum, move.moveDest ) ) {
				seekPos = path.moveGoal;
				result = true;
				move.nextWanderTime = 0;
			} else {
				AI_DEST_UNREACHABLE = true;
			}
		}
	}

	if ( !result ) {
		// wander around
		if ( ( gameLocal.time > move.nextWanderTime ) || !StepDirection( move.wanderYaw ) ) {
			result = NewWanderDir( move.moveDest );
			if ( !result ) {
				StopMove( MOVE_STATUS_DEST_UNREACHABLE );
				AI_DEST_UNREACHABLE = true;
				seekPos	= org;
				return false;
			}
		} else {
			result = true;
		}

		seekPos = org + move.moveDir * 2048.0f;
		if ( ai_debugMove.GetBool() ) {
			gameRenderWorld->DebugLine( colorYellow, org, seekPos, gameLocal.msec, true );
		}
	} else {
		AI_DEST_UNREACHABLE = false;
	}

	if ( result && ( ai_debugMove.GetBool() ) ) {
		gameRenderWorld->DebugLine( colorCyan, physicsObj.GetOrigin(), seekPos );
	}

	return result;
}


/*
=====================
The Dark Mod
idAI::CanSee virtual override
=====================
*/
bool idAI::CanSee( idEntity *ent, bool useFOV ) const
{
	// Test if it is occluded, and use field of vision in the check (true as second parameter)
	bool cansee = idActor::CanSee( ent, useFOV );

	// Also consider lighting and visual acuity of AI
	if (cansee)
	{
		cansee = !IsEntityHiddenByDarkness(ent);
	}

	// Return result
	return cansee;

}

/*
=====================
The Dark Mod
idAI::CanSeeExt

This metohd can ignore lighting conditions and/or field of vision.
=====================
*/
bool idAI::CanSeeExt( idEntity *ent, bool useFOV, bool useLighting ) const
{
	// Test if it is occluded
	bool cansee = idActor::CanSee( ent, useFOV );

	if ((cansee) && (useLighting))
	{
		cansee = !IsEntityHiddenByDarkness(ent);
	}

	// Return result
	return cansee;

}

/*
=====================
The Dark Mod
idAI::CanSeePositionExt

This metohd can ignore lighting conditions and/or field of vision.
=====================
*/
bool idAI::CanSeePositionExt( idVec3 position, bool useFOV, bool useLighting )
{
	trace_t		tr;
	idVec3		eye;
	bool canSee;

	if ( useFOV && !CheckFOV( position ) )
	{
		return false;
	}

	idVec3 ownOrigin = physicsObj.GetOrigin();

	canSee = EntityCanSeePos (this, ownOrigin, position);

	if (canSee && useLighting)
	{
		idVec3 bottomPoint = position;
		idVec3 topPoint = position - (physicsObj.GetGravityNormal() * 32.0);
		float maxDistanceToObserve = getMaximumObservationDistance
		(
			bottomPoint,
			topPoint,
			NULL
		);

		if ((position - ownOrigin).Length() > maxDistanceToObserve)
		{

			canSee = false;
		}

		// Draw debug graphic?
		if (cv_ai_visdist_show.GetFloat() > 1.0)
		{
			idVec3 midPoint = bottomPoint + ((topPoint-bottomPoint) / 2.0);
			idVec3 observeFrom = GetEyePosition();

			if (!canSee)
			{
				idVec4 markerColor (1.0, 0.0, 0.0, 0.0);
				idVec4 markerColor2 (1.0, 0.0, 1.0, 0.0);
				idVec3 arrowLength = midPoint - observeFrom;
				arrowLength.Normalize();
				arrowLength *= maxDistanceToObserve;

				// Distance we could see
				gameRenderWorld->DebugArrow
				(
					markerColor,
					observeFrom,
					observeFrom + arrowLength,
					2,
					cv_ai_visdist_show.GetInteger()
				);


				// Gap to where we want to see
				gameRenderWorld->DebugArrow
				(
					markerColor2,
					observeFrom + arrowLength,
					midPoint,
					2,
					cv_ai_visdist_show.GetInteger()
				);
			}
			else
			{
				idVec4 markerColor (0.0, 1.0, 0.0, 0.0);

				// We can see there
				gameRenderWorld->DebugArrow
				(
					markerColor,
					observeFrom,
					midPoint,
					2,
					cv_ai_visdist_show.GetInteger()
				);
			}

		}


	}

	return canSee;

}




/*
=====================
idAI::EntityCanSeePos
=====================
*/
bool idAI::EntityCanSeePos( idActor *actor, const idVec3 &actorOrigin, const idVec3 &pos ) {
	idVec3 eye, point;
	trace_t results;
	pvsHandle_t handle;

	handle = gameLocal.pvs.SetupCurrentPVS( actor->GetPVSAreas(), actor->GetNumPVSAreas() );

	if ( !gameLocal.pvs.InCurrentPVS( handle, GetPVSAreas(), GetNumPVSAreas() ) ) {
		gameLocal.pvs.FreeCurrentPVS( handle );
		return false;
	}

	gameLocal.pvs.FreeCurrentPVS( handle );

	eye = actorOrigin + actor->EyeOffset();

	point = pos;
	point[2] += 1.0f;

	physicsObj.DisableClip();

	gameLocal.clip.TracePoint( results, eye, point, MASK_SOLID, actor );
	if ( results.fraction >= 1.0f || ( gameLocal.GetTraceEntity( results ) == this ) ) {
		physicsObj.EnableClip();
		return true;
	}

	const idBounds &bounds = physicsObj.GetBounds();
	point[2] += bounds[1][2] - bounds[0][2];

	gameLocal.clip.TracePoint( results, eye, point, MASK_SOLID, actor );
	physicsObj.EnableClip();
	if ( results.fraction >= 1.0f || ( gameLocal.GetTraceEntity( results ) == this ) ) {
		return true;
	}
	return false;
}

/*
=====================
idAI::BlockedFailSafe
=====================
*/
void idAI::BlockedFailSafe( void ) {
	if ( !ai_blockedFailSafe.GetBool() || blockedRadius < 0.0f ) {
		return;
	}
	if ( !physicsObj.OnGround() || enemy.GetEntity() == NULL ||
			( physicsObj.GetOrigin() - move.lastMoveOrigin ).LengthSqr() > Square( blockedRadius ) ) {
		move.lastMoveOrigin = physicsObj.GetOrigin();
		move.lastMoveTime = gameLocal.time;
	}
	if ( move.lastMoveTime < gameLocal.time - blockedMoveTime ) {
		if ( lastAttackTime < gameLocal.time - blockedAttackTime ) {
			AI_BLOCKED = true;
			move.lastMoveTime = gameLocal.time;
		}
	}
}

/***********************************************************************

	turning

***********************************************************************/

/*
=====================
idAI::Turn
=====================
*/
void idAI::Turn( void ) {
	float diff;
	float diff2;
	float turnAmount;
	animFlags_t animflags;

	if ( !turnRate ) {
		return;
	}

	// check if the animator has marker this anim as non-turning
	if ( !legsAnim.Disabled() && !legsAnim.AnimDone( 0 ) ) {
		animflags = legsAnim.GetAnimFlags();
	} else {
		animflags = torsoAnim.GetAnimFlags();
	}
	if ( animflags.ai_no_turn ) {
		return;
	}

	if ( anim_turn_angles && animflags.anim_turn ) {
		idMat3 rotateAxis;

		// set the blend between no turn and full turn
		float frac = anim_turn_amount / anim_turn_angles;
		animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 0, 1.0f - frac );
		animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 1, frac );
		animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 0, 1.0f - frac );
		animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 1, frac );

		// get the total rotation from the start of the anim
		animator.GetDeltaRotation( 0, gameLocal.time, rotateAxis );
		current_yaw = idMath::AngleNormalize180( anim_turn_yaw + rotateAxis[ 0 ].ToYaw() );
	} else {
		diff = idMath::AngleNormalize180( ideal_yaw - current_yaw );
		turnVel += AI_TURN_SCALE * diff * MS2SEC( gameLocal.msec );
		if ( turnVel > turnRate ) {
			turnVel = turnRate;
		} else if ( turnVel < -turnRate ) {
			turnVel = -turnRate;
		}
		turnAmount = turnVel * MS2SEC( gameLocal.msec );
		if ( ( diff >= 0.0f ) && ( turnAmount >= diff ) ) {
			turnVel = diff / MS2SEC( gameLocal.msec );
			turnAmount = diff;
		} else if ( ( diff <= 0.0f ) && ( turnAmount <= diff ) ) {
			turnVel = diff / MS2SEC( gameLocal.msec );
			turnAmount = diff;
		}
		current_yaw += turnAmount;
		current_yaw = idMath::AngleNormalize180( current_yaw );
		diff2 = idMath::AngleNormalize180( ideal_yaw - current_yaw );
		if ( idMath::Fabs( diff2 ) < 0.1f ) {
			current_yaw = ideal_yaw;
		}
	}

	viewAxis = idAngles( 0, current_yaw, 0 ).ToMat3();

	if ( ai_debugMove.GetBool() ) {
		const idVec3 &org = physicsObj.GetOrigin();
		gameRenderWorld->DebugLine( colorRed, org, org + idAngles( 0, ideal_yaw, 0 ).ToForward() * 64, gameLocal.msec );
		gameRenderWorld->DebugLine( colorGreen, org, org + idAngles( 0, current_yaw, 0 ).ToForward() * 48, gameLocal.msec );
		gameRenderWorld->DebugLine( colorYellow, org, org + idAngles( 0, current_yaw + turnVel, 0 ).ToForward() * 32, gameLocal.msec );
	}
}

/*
=====================
idAI::FacingIdeal
=====================
*/
bool idAI::FacingIdeal( void ) {
	float diff;

	if ( !turnRate ) {
		return true;
	}

	diff = idMath::AngleNormalize180( current_yaw - ideal_yaw );
	if ( idMath::Fabs( diff ) < 0.01f ) {
		// force it to be exact
		current_yaw = ideal_yaw;
		return true;
	}

	return false;
}

/*
=====================
idAI::TurnToward
=====================
*/
bool idAI::TurnToward( float yaw ) {
	ideal_yaw = idMath::AngleNormalize180( yaw );
	bool result = FacingIdeal();
	return result;
}

/*
=====================
idAI::TurnToward
=====================
*/
bool idAI::TurnToward( const idVec3 &pos ) {
	idVec3 dir;
	idVec3 local_dir;
	float lengthSqr;

	dir = pos - physicsObj.GetOrigin();
	physicsObj.GetGravityAxis().ProjectVector( dir, local_dir );
	local_dir.z = 0.0f;
	lengthSqr = local_dir.LengthSqr();
	if ( lengthSqr > Square( 2.0f ) || ( lengthSqr > Square( 0.1f ) && enemy.GetEntity() == NULL ) ) {
		ideal_yaw = idMath::AngleNormalize180( local_dir.ToYaw() );
	}

	bool result = FacingIdeal();
	return result;
}

/***********************************************************************

	Movement

***********************************************************************/

/*
================
idAI::ApplyImpulse
================
*/
void idAI::ApplyImpulse( idEntity *ent, int id, const idVec3 &point, const idVec3 &impulse ) {
	// FIXME: Jim take a look at this and see if this is a reasonable thing to do
	// instead of a spawnArg flag.. Sabaoth is the only slide monster ( and should be the only one for D3 )
	// and we don't want him taking physics impulses as it can knock him off the path
	if ( move.moveType != MOVETYPE_STATIC && move.moveType != MOVETYPE_SLIDE ) {
		idActor::ApplyImpulse( ent, id, point, impulse );
	}
}

/*
=====================
idAI::GetMoveDelta
=====================
*/
void idAI::GetMoveDelta( const idMat3 &oldaxis, const idMat3 &axis, idVec3 &delta ) {
	idVec3 oldModelOrigin;
	idVec3 modelOrigin;

	animator.GetDelta( gameLocal.time - gameLocal.msec, gameLocal.time, delta );
	delta = axis * delta;

	if ( modelOffset != vec3_zero ) {
		// the pivot of the monster's model is around its origin, and not around the bounding
		// box's origin, so we have to compensate for this when the model is offset so that
		// the monster still appears to rotate around it's origin.
		oldModelOrigin = modelOffset * oldaxis;
		modelOrigin = modelOffset * axis;
		delta += oldModelOrigin - modelOrigin;
	}

	delta *= physicsObj.GetGravityAxis();
}

/*
=====================
idAI::CheckObstacleAvoidance
=====================
*/
void idAI::CheckObstacleAvoidance( const idVec3 &goalPos, idVec3 &newPos ) {
	idEntity		*obstacle;
	obstaclePath_t	path;
	idVec3			dir;
	float			dist;
	bool			foundPath;

	// If there is an obstacle, this is the distance from it we should stop to
	// take action
	float stopDistance = 0.0f;

	if ( ignore_obstacles ) {
		newPos = goalPos;
		move.obstacle = NULL;
		return;
	}

	const idVec3 &origin = physicsObj.GetOrigin();

	obstacle = NULL;
	AI_OBSTACLE_IN_PATH = false;
	foundPath = FindPathAroundObstacles( &physicsObj, aas, enemy.GetEntity(), origin, goalPos, path );
	if ( ai_showObstacleAvoidance.GetBool() ) {
		gameRenderWorld->DebugLine( colorBlue, goalPos + idVec3( 1.0f, 1.0f, 0.0f ), goalPos + idVec3( 1.0f, 1.0f, 64.0f ), gameLocal.msec );
		gameRenderWorld->DebugLine( foundPath ? colorYellow : colorRed, path.seekPos, path.seekPos + idVec3( 0.0f, 0.0f, 64.0f ), gameLocal.msec );
	}

	if ( !foundPath ) {
		// couldn't get around obstacles
		if ( path.firstObstacle )
		{
			AI_OBSTACLE_IN_PATH = true;

			// If its a door handle, switch the obstacle to the door so we don't get all hung
			// up on door handles
			if (path.firstObstacle->IsType (CFrobDoorHandle::Type))
			{
				// Make the obstacle the door itself
				path.firstObstacle = ((CFrobDoorHandle*) (path.firstObstacle))->GetDoor();
			}


			/* SZ: Further distance for Binary Frob Movers (eg: doors) */
			if (path.firstObstacle->IsType (CBinaryFrobMover::Type))
			{
				// Calculate distance far enough away that we won't hit swinging door
				// opening toward us
				idVec3 delta;
				idVec3 gravity;
				idVec3 sizePerpGrav;

				idBounds avoidBounds = path.firstObstacle->GetPhysics()->GetBounds();
				delta.x = avoidBounds[0][1] - avoidBounds[0][0];
				delta.y = avoidBounds[1][1] - avoidBounds[1][0];
				delta.z = avoidBounds[2][1] - avoidBounds[2][0];

				gravity = gameLocal.GetGravity();
				gravity.Normalize();

				sizePerpGrav = gravity.Cross (delta);

				stopDistance = sizePerpGrav.Length();

				// The door becomes an active dynamic pathing obstacle when we
				// reach that distance (we will open the door at that point)
				if ( physicsObj.GetAbsBounds().Expand( stopDistance).IntersectsBounds( path.firstObstacle->GetPhysics()->GetAbsBounds() ) )
				{
					obstacle = path.firstObstacle;
				}
			}
			else
			{
				if ( physicsObj.GetAbsBounds().Expand( 2.0f ).IntersectsBounds( path.firstObstacle->GetPhysics()->GetAbsBounds() ) )
				{
					obstacle = path.firstObstacle;
				}
			}
		}
		else if ( path.startPosObstacle )
		{
			AI_OBSTACLE_IN_PATH = true;
			if ( physicsObj.GetAbsBounds().Expand( 2.0f ).IntersectsBounds( path.startPosObstacle->GetPhysics()->GetAbsBounds() ) ) {
				obstacle = path.startPosObstacle;
			}
		} else
		{
			// Blocked by wall
			move.moveStatus = MOVE_STATUS_BLOCKED_BY_WALL;
		}
#if 0
	} else if ( path.startPosObstacle ) {
		// check if we're past where the our origin was pushed out of the obstacle
		dir = goalPos - origin;
		dir.Normalize();
		dist = ( path.seekPos - origin ) * dir;
		if ( dist < 1.0f ) {
			AI_OBSTACLE_IN_PATH = true;
			obstacle = path.startPosObstacle;
		}
#endif
	}
	else if ( path.seekPosObstacle )
	{
		// if the AI is very close to the path.seekPos already and path.seekPosObstacle != NULL
		// then we want to push the path.seekPosObstacle entity out of the way
		AI_OBSTACLE_IN_PATH = true;

		// check if we're past where the goalPos was pushed out of the obstacle
		dir = goalPos - origin;
		dir.Normalize();
		dist = ( path.seekPos - origin ) * dir;
		if ( dist < 1.0f ) {
			obstacle = path.seekPosObstacle;
		}
	}

	// if we had an obstacle, set our move status based on the type, and kick it out of the way if it's a moveable
	if ( obstacle )
	{
		if ( obstacle->IsType( idActor::Type ) )
		{
			// monsters aren't kickable
			if ( obstacle == enemy.GetEntity() )
			{
				move.moveStatus = MOVE_STATUS_BLOCKED_BY_ENEMY;
			} else
			{
				move.moveStatus = MOVE_STATUS_BLOCKED_BY_MONSTER;
			}
		}
		else
		{
			// If its a door handle, switch the obstacle to the door so we don't get all hung
			// up on door handles
			if (obstacle->IsType (CFrobDoorHandle::Type))
			{
				// Make the obstacle the door itself
				obstacle = ((CFrobDoorHandle*) (obstacle))->GetDoor();

				// Calculate distance far enough away that we won't hit swinging door
				// opening toward us
				idVec3 delta;
				idVec3 gravity;
				idVec3 sizePerpGrav;

				idBounds avoidBounds = obstacle->GetPhysics()->GetBounds();
				delta.x = avoidBounds[0][1] - avoidBounds[0][0];
				delta.y = avoidBounds[1][1] - avoidBounds[1][0];
				delta.z = avoidBounds[2][1] - avoidBounds[2][0];

				gravity = gameLocal.GetGravity();
				gravity.Normalize();

				sizePerpGrav = gravity.Cross (delta);

				stopDistance = sizePerpGrav.Length();
			}

			// Handle doors
			if (obstacle->IsType (CFrobDoor::Type))
			{
				// Try to open doors
				CFrobDoor* p_door = (CFrobDoor*) obstacle;
				if (!p_door->isOpen())
				{
					// If it is not interrupted and not changing state
					if ( (!p_door->isChangingState()) || (p_door->wasInterrupted() ) )
					{
						bool b_canOpen = true;
						if (p_door->isLocked())
						{
							// TODO: Call script to see if I have this key. For now
							// answer is always yes.

						}

						// Open the door
						if (b_canOpen)
						{
							p_door->OpenDoor(false);
						}

					} // Door isn't changing state or was interrupted mid state-change

				} // Door isn't open

				newPos = obstacle->GetPhysics()->GetOrigin();
				idVec3 obstacleDelta = obstacle->GetPhysics()->GetOrigin() -
					GetPhysics()->GetOrigin();

				obstacleDelta.Normalize();
				obstacleDelta *= stopDistance;

				newPos = obstacle->GetPhysics()->GetOrigin() - obstacleDelta;

				//newPos = path.seekPos;
				move.obstacle = obstacle;
				//move.moveStatus = MOVE_STATUS_BLOCKED_BY_OBJECT;

			}
			else
			{
				// try kicking the object out of the way
				//move.moveStatus = MOVE_STATUS_BLOCKED_BY_OBJECT;
				//newPos = obstacle->GetPhysics()->GetOrigin();

				// Try backing away
				newPos = obstacle->GetPhysics()->GetOrigin();
				idVec3 obstacleDelta = obstacle->GetPhysics()->GetOrigin() -
					GetPhysics()->GetOrigin();

				obstacleDelta.Normalize();
				obstacleDelta *= 128.0;

				newPos = obstacle->GetPhysics()->GetOrigin() - obstacleDelta;
				move.obstacle = obstacle;
				move.moveStatus = MOVE_STATUS_BLOCKED_BY_OBJECT;
			}
		}

		move.obstacle = obstacle;

	}
	else
	{
		newPos = path.seekPos;
		move.obstacle = NULL;
	}
}

/*
=====================
idAI::DeadMove
=====================
*/
void idAI::DeadMove( void ) {
	idVec3				delta;
	monsterMoveResult_t	moveResult;

	idVec3 org = physicsObj.GetOrigin();

	GetMoveDelta( viewAxis, viewAxis, delta );
	physicsObj.SetDelta( delta );

	RunPhysics();

	moveResult = physicsObj.GetMoveResult();
	AI_ONGROUND = physicsObj.OnGround();
}

/*
=====================
idAI::AnimMove
=====================
*/
void idAI::AnimMove( void ) {

	idVec3				goalPos;
	idVec3				delta;
	idVec3				goalDelta;
	float				goalDist;
	monsterMoveResult_t	moveResult;
	idVec3				newDest;

	idVec3 oldorigin = physicsObj.GetOrigin();
	idMat3 oldaxis = viewAxis;

	AI_BLOCKED = false;

	if ( move.moveCommand < NUM_NONMOVING_COMMANDS ){
		move.lastMoveOrigin.Zero();
		move.lastMoveTime = gameLocal.time;
	}

	move.obstacle = NULL;
	if ( ( move.moveCommand == MOVE_FACE_ENEMY ) && enemy.GetEntity() ) {
		TurnToward( lastVisibleEnemyPos );
		goalPos = oldorigin;
	} else if ( ( move.moveCommand == MOVE_FACE_ENTITY ) && move.goalEntity.GetEntity() ) {
		TurnToward( move.goalEntity.GetEntity()->GetPhysics()->GetOrigin() );
		goalPos = oldorigin;
	} else if ( GetMovePos( goalPos ) ) 
	{
		if ( (move.moveCommand != MOVE_WANDER) || move.moveCommand != MOVE_VECTOR ) 
		{
			CheckObstacleAvoidance( goalPos, newDest );
			TurnToward( newDest );
		} else {
			TurnToward( goalPos );
		}
	}

	Turn();

	if ( move.moveCommand == MOVE_SLIDE_TO_POSITION ) {
		if ( gameLocal.time < move.startTime + move.duration ) {
			goalPos = move.moveDest - move.moveDir * MS2SEC( move.startTime + move.duration - gameLocal.time );
			delta = goalPos - oldorigin;
			delta.z = 0.0f;
		} else {
			delta = move.moveDest - oldorigin;
			delta.z = 0.0f;
			StopMove( MOVE_STATUS_DONE );
		}
	} else if ( allowMove ) {
		GetMoveDelta( oldaxis, viewAxis, delta );
	} else {
		delta.Zero();
	}

	if ( move.moveCommand == MOVE_TO_POSITION ) {
		goalDelta = move.moveDest - oldorigin;
		goalDist = goalDelta.LengthFast();
		if ( goalDist < delta.LengthFast() ) {
			delta = goalDelta;
		}
	}

	physicsObj.SetDelta( delta );
	physicsObj.ForceDeltaMove( disableGravity );

	RunPhysics();

	if ( ai_debugMove.GetBool() ) {
		gameRenderWorld->DebugLine( colorCyan, oldorigin, physicsObj.GetOrigin(), 5000 );
	}

	moveResult = physicsObj.GetMoveResult();
	if ( !m_bAFPushMoveables && attack.Length() && TestMelee() ) {
		DirectDamage( attack, enemy.GetEntity() );
	} else {
		idEntity *blockEnt = physicsObj.GetSlideMoveEntity();
		if ( blockEnt && blockEnt->IsType( idMoveable::Type ) && blockEnt->GetPhysics()->IsPushable() ) {
			KickObstacles( viewAxis[ 0 ], kickForce, blockEnt );
		}
	}

	BlockedFailSafe();

	AI_ONGROUND = physicsObj.OnGround();

	idVec3 org = physicsObj.GetOrigin();
	if ( oldorigin != org ) {
		TouchTriggers();
	}

	if ( ai_debugMove.GetBool() ) {
		gameRenderWorld->DebugBounds( colorMagenta, physicsObj.GetBounds(), org, gameLocal.msec );
		gameRenderWorld->DebugBounds( colorMagenta, physicsObj.GetBounds(), move.moveDest, gameLocal.msec );
		gameRenderWorld->DebugLine( colorYellow, org + EyeOffset(), org + EyeOffset() + viewAxis[ 0 ] * physicsObj.GetGravityAxis() * 16.0f, gameLocal.msec, true );
		DrawRoute();
	}
}

/*
=====================
Seek
=====================
*/
idVec3 Seek( idVec3 &vel, const idVec3 &org, const idVec3 &goal, float prediction ) {
	idVec3 predictedPos;
	idVec3 goalDelta;
	idVec3 seekVel;

	// predict our position
	predictedPos = org + vel * prediction;
	goalDelta = goal - predictedPos;
	seekVel = goalDelta * MS2SEC( gameLocal.msec );

	return seekVel;
}

/*
=====================
idAI::SlideMove
=====================
*/
void idAI::SlideMove( void ) {
	idVec3				goalPos;
	idVec3				delta;
	idVec3				goalDelta;
	float				goalDist;
	monsterMoveResult_t	moveResult;
	idVec3				newDest;

	idVec3 oldorigin = physicsObj.GetOrigin();
	idMat3 oldaxis = viewAxis;

	AI_BLOCKED = false;

	if ( move.moveCommand < NUM_NONMOVING_COMMANDS ){
		move.lastMoveOrigin.Zero();
		move.lastMoveTime = gameLocal.time;
	}

	move.obstacle = NULL;
	if ( ( move.moveCommand == MOVE_FACE_ENEMY ) && enemy.GetEntity() ) {
		TurnToward( lastVisibleEnemyPos );
		goalPos = move.moveDest;
	} else if ( ( move.moveCommand == MOVE_FACE_ENTITY ) && move.goalEntity.GetEntity() ) {
		TurnToward( move.goalEntity.GetEntity()->GetPhysics()->GetOrigin() );
		goalPos = move.moveDest;
	} else if ( GetMovePos( goalPos ) ) {
		CheckObstacleAvoidance( goalPos, newDest );
		TurnToward( newDest );
		goalPos = newDest;
	}

	if ( move.moveCommand == MOVE_SLIDE_TO_POSITION ) {
		if ( gameLocal.time < move.startTime + move.duration ) {
			goalPos = move.moveDest - move.moveDir * MS2SEC( move.startTime + move.duration - gameLocal.time );
		} else {
			goalPos = move.moveDest;
			StopMove( MOVE_STATUS_DONE );
		}
	}

	if ( move.moveCommand == MOVE_TO_POSITION ) {
		goalDelta = move.moveDest - oldorigin;
		goalDist = goalDelta.LengthFast();
		if ( goalDist < delta.LengthFast() ) {
			delta = goalDelta;
		}
	}

	idVec3 vel = physicsObj.GetLinearVelocity();
	float z = vel.z;
	idVec3  predictedPos = oldorigin + vel * AI_SEEK_PREDICTION;

	// seek the goal position
	goalDelta = goalPos - predictedPos;
	vel -= vel * AI_FLY_DAMPENING * MS2SEC( gameLocal.msec );
	vel += goalDelta * MS2SEC( gameLocal.msec );

	// cap our speed
	vel.Truncate( fly_speed );
	vel.z = z;
	physicsObj.SetLinearVelocity( vel );
	physicsObj.UseVelocityMove( true );
	RunPhysics();

	if ( ( move.moveCommand == MOVE_FACE_ENEMY ) && enemy.GetEntity() ) {
		TurnToward( lastVisibleEnemyPos );
	} else if ( ( move.moveCommand == MOVE_FACE_ENTITY ) && move.goalEntity.GetEntity() ) {
		TurnToward( move.goalEntity.GetEntity()->GetPhysics()->GetOrigin() );
	} else if ( move.moveCommand != MOVE_NONE ) {
		if ( vel.ToVec2().LengthSqr() > 0.1f ) {
			TurnToward( vel.ToYaw() );
		}
	}
	Turn();

	if ( ai_debugMove.GetBool() ) {
		gameRenderWorld->DebugLine( colorCyan, oldorigin, physicsObj.GetOrigin(), 5000 );
	}

	moveResult = physicsObj.GetMoveResult();
	if ( !m_bAFPushMoveables && attack.Length() && TestMelee() ) {
		DirectDamage( attack, enemy.GetEntity() );
	} else {
		idEntity *blockEnt = physicsObj.GetSlideMoveEntity();
		if ( blockEnt && blockEnt->IsType( idMoveable::Type ) && blockEnt->GetPhysics()->IsPushable() ) {
			KickObstacles( viewAxis[ 0 ], kickForce, blockEnt );
		}
	}

	BlockedFailSafe();

	AI_ONGROUND = physicsObj.OnGround();

	idVec3 org = physicsObj.GetOrigin();
	if ( oldorigin != org ) {
		TouchTriggers();
	}

	if ( ai_debugMove.GetBool() ) {
		gameRenderWorld->DebugBounds( colorMagenta, physicsObj.GetBounds(), org, gameLocal.msec );
		gameRenderWorld->DebugBounds( colorMagenta, physicsObj.GetBounds(), move.moveDest, gameLocal.msec );
		gameRenderWorld->DebugLine( colorYellow, org + EyeOffset(), org + EyeOffset() + viewAxis[ 0 ] * physicsObj.GetGravityAxis() * 16.0f, gameLocal.msec, true );
		DrawRoute();
	}
}

/*
=====================
idAI::AdjustFlyingAngles
=====================
*/
void idAI::AdjustFlyingAngles( void ) {
	idVec3	vel;
	float 	speed;
	float 	roll;
	float 	pitch;

	vel = physicsObj.GetLinearVelocity();

	speed = vel.Length();
	if ( speed < 5.0f ) {
		roll = 0.0f;
		pitch = 0.0f;
	} else {
		roll = vel * viewAxis[ 1 ] * -fly_roll_scale / fly_speed;
		if ( roll > fly_roll_max ) {
			roll = fly_roll_max;
		} else if ( roll < -fly_roll_max ) {
			roll = -fly_roll_max;
		}

		pitch = vel * viewAxis[ 2 ] * -fly_pitch_scale / fly_speed;
		if ( pitch > fly_pitch_max ) {
			pitch = fly_pitch_max;
		} else if ( pitch < -fly_pitch_max ) {
			pitch = -fly_pitch_max;
		}
	}

	fly_roll = fly_roll * 0.95f + roll * 0.05f;
	fly_pitch = fly_pitch * 0.95f + pitch * 0.05f;

	if ( flyTiltJoint != INVALID_JOINT ) {
		animator.SetJointAxis( flyTiltJoint, JOINTMOD_WORLD, idAngles( fly_pitch, 0.0f, fly_roll ).ToMat3() );
	} else {
		viewAxis = idAngles( fly_pitch, current_yaw, fly_roll ).ToMat3();
	}
}

/*
=====================
idAI::AddFlyBob
=====================
*/
void idAI::AddFlyBob( idVec3 &vel ) {
	idVec3	fly_bob_add;
	float	t;

	if ( fly_bob_strength ) {
		t = MS2SEC( gameLocal.time + entityNumber * 497 );
		fly_bob_add = ( viewAxis[ 1 ] * idMath::Sin16( t * fly_bob_horz ) + viewAxis[ 2 ] * idMath::Sin16( t * fly_bob_vert ) ) * fly_bob_strength;
		vel += fly_bob_add * MS2SEC( gameLocal.msec );
		if ( ai_debugMove.GetBool() ) {
			const idVec3 &origin = physicsObj.GetOrigin();
			gameRenderWorld->DebugArrow( colorOrange, origin, origin + fly_bob_add, 0 );
		}
	}
}

/*
=====================
idAI::AdjustFlyHeight
=====================
*/
void idAI::AdjustFlyHeight( idVec3 &vel, const idVec3 &goalPos ) {
	const idVec3	&origin = physicsObj.GetOrigin();
	predictedPath_t path;
	idVec3			end;
	idVec3			dest;
	trace_t			trace;
	idActor			*enemyEnt;
	bool			goLower;

	// make sure we're not flying too high to get through doors
	goLower = false;
	if ( origin.z > goalPos.z ) {
		dest = goalPos;
		dest.z = origin.z + 128.0f;
		idAI::PredictPath( this, aas, goalPos, dest - origin, 1000, 1000, SE_BLOCKED, path );
		if ( path.endPos.z < origin.z ) {
			idVec3 addVel = Seek( vel, origin, path.endPos, AI_SEEK_PREDICTION );
			vel.z += addVel.z;
			goLower = true;
		}

		if ( ai_debugMove.GetBool() ) {
			gameRenderWorld->DebugBounds( goLower ? colorRed : colorGreen, physicsObj.GetBounds(), path.endPos, gameLocal.msec );
		}
	}

	if ( !goLower ) {
		// make sure we don't fly too low
		end = origin;

		enemyEnt = enemy.GetEntity();
		if ( enemyEnt ) {
			end.z = lastVisibleEnemyPos.z + lastVisibleEnemyEyeOffset.z + fly_offset;
		} else {
			// just use the default eye height for the player
			end.z = goalPos.z + DEFAULT_FLY_OFFSET + fly_offset;
		}

		gameLocal.clip.Translation( trace, origin, end, physicsObj.GetClipModel(), mat3_identity, MASK_MONSTERSOLID, this );
		vel += Seek( vel, origin, trace.endpos, AI_SEEK_PREDICTION );
	}
}

/*
=====================
idAI::FlySeekGoal
=====================
*/
void idAI::FlySeekGoal( idVec3 &vel, idVec3 &goalPos ) {
	idVec3 seekVel;

	// seek the goal position
	seekVel = Seek( vel, physicsObj.GetOrigin(), goalPos, AI_SEEK_PREDICTION );
	seekVel *= fly_seek_scale;
	vel += seekVel;
}

/*
=====================
idAI::AdjustFlySpeed
=====================
*/
void idAI::AdjustFlySpeed( idVec3 &vel ) {
	float speed;

	// apply dampening
	vel -= vel * AI_FLY_DAMPENING * MS2SEC( gameLocal.msec );

	// gradually speed up/slow down to desired speed
	speed = vel.Normalize();
	speed += ( move.speed - speed ) * MS2SEC( gameLocal.msec );
	if ( speed < 0.0f ) {
		speed = 0.0f;
	} else if ( move.speed && ( speed > move.speed ) ) {
		speed = move.speed;
	}

	vel *= speed;
}

/*
=====================
idAI::FlyTurn
=====================
*/
void idAI::FlyTurn( void ) {
	if ( move.moveCommand == MOVE_FACE_ENEMY ) {
		TurnToward( lastVisibleEnemyPos );
	} else if ( ( move.moveCommand == MOVE_FACE_ENTITY ) && move.goalEntity.GetEntity() ) {
		TurnToward( move.goalEntity.GetEntity()->GetPhysics()->GetOrigin() );
	} else if ( move.speed > 0.0f ) {
		const idVec3 &vel = physicsObj.GetLinearVelocity();
		if ( vel.ToVec2().LengthSqr() > 0.1f ) {
			TurnToward( vel.ToYaw() );
		}
	}
	Turn();
}

/*
=====================
idAI::FlyMove
=====================
*/
void idAI::FlyMove( void ) {
	idVec3	goalPos;
	idVec3	oldorigin;
	idVec3	newDest;

	AI_BLOCKED = false;
	if ( ( move.moveCommand != MOVE_NONE ) && ReachedPos( move.moveDest, move.moveCommand ) ) {
		StopMove( MOVE_STATUS_DONE );
	}

	if ( ai_debugMove.GetBool() ) {
		gameLocal.Printf( "%d: %s: %s, vel = %.2f, sp = %.2f, maxsp = %.2f\n", gameLocal.time, name.c_str(), moveCommandString[ move.moveCommand ], physicsObj.GetLinearVelocity().Length(), move.speed, fly_speed );
	}

	if ( move.moveCommand != MOVE_TO_POSITION_DIRECT ) {
		idVec3 vel = physicsObj.GetLinearVelocity();

		if ( GetMovePos( goalPos ) ) {
			CheckObstacleAvoidance( goalPos, newDest );
			goalPos = newDest;
		}

		if ( move.speed	) {
			FlySeekGoal( vel, goalPos );
		}

		// add in bobbing
		AddFlyBob( vel );

		if ( enemy.GetEntity() && ( move.moveCommand != MOVE_TO_POSITION ) ) {
			AdjustFlyHeight( vel, goalPos );
		}

		AdjustFlySpeed( vel );

		physicsObj.SetLinearVelocity( vel );
	}

	// turn
	FlyTurn();

	// run the physics for this frame
	oldorigin = physicsObj.GetOrigin();
	physicsObj.UseFlyMove( true );
	physicsObj.UseVelocityMove( false );
	physicsObj.SetDelta( vec3_zero );
	physicsObj.ForceDeltaMove( disableGravity );
	RunPhysics();

	monsterMoveResult_t	moveResult = physicsObj.GetMoveResult();
	if ( !m_bAFPushMoveables && attack.Length() && TestMelee() ) {
		DirectDamage( attack, enemy.GetEntity() );
	} else {
		idEntity *blockEnt = physicsObj.GetSlideMoveEntity();
		if ( blockEnt && blockEnt->IsType( idMoveable::Type ) && blockEnt->GetPhysics()->IsPushable() ) {
			KickObstacles( viewAxis[ 0 ], kickForce, blockEnt );
		} else if ( moveResult == MM_BLOCKED ) {
			move.blockTime = gameLocal.time + 500;
			AI_BLOCKED = true;
		}
	}

	idVec3 org = physicsObj.GetOrigin();
	if ( oldorigin != org ) {
		TouchTriggers();
	}

	if ( ai_debugMove.GetBool() ) {
		gameRenderWorld->DebugLine( colorCyan, oldorigin, physicsObj.GetOrigin(), 4000 );
		gameRenderWorld->DebugBounds( colorOrange, physicsObj.GetBounds(), org, gameLocal.msec );
		gameRenderWorld->DebugBounds( colorMagenta, physicsObj.GetBounds(), move.moveDest, gameLocal.msec );
		gameRenderWorld->DebugLine( colorRed, org, org + physicsObj.GetLinearVelocity(), gameLocal.msec, true );
		gameRenderWorld->DebugLine( colorBlue, org, goalPos, gameLocal.msec, true );
		gameRenderWorld->DebugLine( colorYellow, org + EyeOffset(), org + EyeOffset() + viewAxis[ 0 ] * physicsObj.GetGravityAxis() * 16.0f, gameLocal.msec, true );
		DrawRoute();
	}
}

/*
=====================
idAI::StaticMove
=====================
*/
void idAI::StaticMove( void ) {
	idActor	*enemyEnt = enemy.GetEntity();

	if ( AI_DEAD || AI_KNOCKEDOUT ) {
		return;
	}

	if ( ( move.moveCommand == MOVE_FACE_ENEMY ) && enemyEnt ) {
		TurnToward( lastVisibleEnemyPos );
	} else if ( ( move.moveCommand == MOVE_FACE_ENTITY ) && move.goalEntity.GetEntity() ) {
		TurnToward( move.goalEntity.GetEntity()->GetPhysics()->GetOrigin() );
	} else if ( move.moveCommand != MOVE_NONE ) {
		TurnToward( move.moveDest );
	}
	Turn();

	physicsObj.ForceDeltaMove( true ); // disable gravity
	RunPhysics();

	AI_ONGROUND = false;

	if ( !m_bAFPushMoveables && attack.Length() && TestMelee() ) {
		DirectDamage( attack, enemyEnt );
	}

	if ( ai_debugMove.GetBool() ) {
		const idVec3 &org = physicsObj.GetOrigin();
		gameRenderWorld->DebugBounds( colorMagenta, physicsObj.GetBounds(), org, gameLocal.msec );
		gameRenderWorld->DebugLine( colorBlue, org, move.moveDest, gameLocal.msec, true );
		gameRenderWorld->DebugLine( colorYellow, org + EyeOffset(), org + EyeOffset() + viewAxis[ 0 ] * physicsObj.GetGravityAxis() * 16.0f, gameLocal.msec, true );
	}
}

/***********************************************************************

	Damage

***********************************************************************/

/*
=====================
idAI::ReactionTo

DarkMod : Added call to AI Relationship Manager
		  We don't have to hardcode this, could be done
		  with scripts, but for now it's hardcoded for
		  testing purposes.
=====================
*/
int idAI::ReactionTo( const idEntity *ent )
{
	if ( ent->fl.hidden )
	{
		// ignore hidden entities
		return ATTACK_IGNORE;
	}

	if ( !ent->IsType( idActor::Type ) ) {
		return ATTACK_IGNORE;
	}

	const idActor *actor = static_cast<const idActor *>( ent );
	if ( actor->IsType( idPlayer::Type ) && static_cast<const idPlayer *>(actor)->noclip ) {
		// ignore players in noclip mode
		return ATTACK_IGNORE;
	}

	// actors will always fight if their teams are enemies
	if ( gameLocal.m_RelationsManager->IsEnemy( team, actor->team ) )
	{
		if ( actor->fl.notarget )
		{
			// don't attack on sight when attacker is notargeted
			return ATTACK_ON_DAMAGE | ATTACK_ON_ACTIVATE;
		}
		return ATTACK_ON_SIGHT | ATTACK_ON_DAMAGE | ATTACK_ON_ACTIVATE;
	}

	// monsters will fight when attacked by lower ranked monsters.  rank 0 never fights back.
	if ( rank && ( actor->rank < rank ) ) {
		return ATTACK_ON_DAMAGE;
	}

	// don't fight back
	return ATTACK_IGNORE;
}


/*
=====================
idAI::Pain
=====================
*/
bool idAI::Pain( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location ) {
	idActor	*actor;

	AI_PAIN = idActor::Pain( inflictor, attacker, damage, dir, location );
	AI_DAMAGE = true;

	// force a blink
	blink_time = 0;

	// ignore damage from self
	if ( attacker != this )
	{
		if ( inflictor )
		{
			AI_SPECIAL_DAMAGE = inflictor->spawnArgs.GetInt( "special_damage" );
		} else
		{
			AI_SPECIAL_DAMAGE = 0;
		}

		if ( enemy.GetEntity() != attacker && attacker->IsType( idActor::Type ) ) {
			actor = ( idActor * )attacker;
			if ( ReactionTo( actor ) & ATTACK_ON_DAMAGE )
			{
				// being attacked always overrides the previous alert
				gameLocal.AlertAI( actor );
				SetEnemy( actor );
			}
		}
	}

	return ( AI_PAIN != 0 );
}


/*
=====================
idAI::SpawnParticles
=====================
*/
void idAI::SpawnParticles( const char *keyName ) {
	const idKeyValue *kv = spawnArgs.MatchPrefix( keyName, NULL );
	while ( kv ) {
		particleEmitter_t pe;

		idStr particleName = kv->GetValue();

		if ( particleName.Length() ) {

			idStr jointName = kv->GetValue();
			int dash = jointName.Find('-');
			if ( dash > 0 ) {
				particleName = particleName.Left( dash );
				jointName = jointName.Right( jointName.Length() - dash - 1 );
			}

			SpawnParticlesOnJoint( pe, particleName, jointName );
			particles.Append( pe );
		}

		kv = spawnArgs.MatchPrefix( keyName, kv );
	}
}

/*
=====================
idAI::SpawnParticlesOnJoint
=====================
*/
const idDeclParticle *idAI::SpawnParticlesOnJoint( particleEmitter_t &pe, const char *particleName, const char *jointName ) {
	idVec3 origin;
	idMat3 axis;

	if ( *particleName == '\0' ) {
		memset( &pe, 0, sizeof( pe ) );
		return pe.particle;
	}

	pe.joint = animator.GetJointHandle( jointName );
	if ( pe.joint == INVALID_JOINT ) {
		gameLocal.Warning( "Unknown particleJoint '%s' on '%s'", jointName, name.c_str() );
		pe.time = 0;
		pe.particle = NULL;
	} else {
		animator.GetJointTransform( pe.joint, gameLocal.time, origin, axis );
		origin = renderEntity.origin + origin * renderEntity.axis;

		BecomeActive( TH_UPDATEPARTICLES );
		if ( !gameLocal.time ) {
			// particles with time of 0 don't show, so set the time differently on the first frame
			pe.time = 1;
		} else {
			pe.time = gameLocal.time;
		}
		pe.particle = static_cast<const idDeclParticle *>( declManager->FindType( DECL_PARTICLE, particleName ) );
		gameLocal.smokeParticles->EmitSmoke( pe.particle, pe.time, gameLocal.random.CRandomFloat(), origin, axis );
	}

	return pe.particle;
}

/*
=====================
idAI::Killed
=====================
*/
void idAI::Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location )
{
	idAngles ang;
	const char *modelDeath;
	bool bPlayerResponsible(false);

	// make sure the monster is activated
	EndAttack();

	if ( g_debugDamage.GetBool() ) {
		gameLocal.Printf( "Damage: joint: '%s', zone '%s'\n", animator.GetJointName( ( jointHandle_t )location ),
			GetDamageGroup( location ) );
	}

	if ( inflictor ) {
		AI_SPECIAL_DAMAGE = inflictor->spawnArgs.GetInt( "special_damage" );
	} else {
		AI_SPECIAL_DAMAGE = 0;
	}

	if ( AI_DEAD ) {
		AI_PAIN = true;
		AI_DAMAGE = true;
		return;
	}

	// stop all voice sounds
	StopSound( SND_CHANNEL_VOICE, false );
	if ( head.GetEntity() ) {
		head.GetEntity()->StopSound( SND_CHANNEL_VOICE, false );
		head.GetEntity()->GetAnimator()->ClearAllAnims( gameLocal.time, 100 );
	}

	disableGravity = false;
	move.moveType = MOVETYPE_DEAD;
	m_bAFPushMoveables = false;

	physicsObj.UseFlyMove( false );
	physicsObj.ForceDeltaMove( false );

	// end our looping ambient sound
	StopSound( SND_CHANNEL_AMBIENT, false );

	if ( attacker && attacker->IsType( idActor::Type ) ) {
		gameLocal.AlertAI( ( idActor * )attacker );
	}

	// activate targets
	ActivateTargets( attacker );

	RemoveAttachments();
	RemoveProjectile();
	StopMove( MOVE_STATUS_DONE );

	ClearEnemy();
	AI_DEAD	= true;

	// make monster nonsolid
	physicsObj.SetContents( 0 );
	physicsObj.GetClipModel()->Unlink();

	Unbind();

	if ( StartRagdoll() )
	{
		if( MouthIsUnderwater() )
			StartSound( "snd_death_liquid", SND_CHANNEL_VOICE, 0, false, NULL );
		else
			StartSound( "snd_death", SND_CHANNEL_VOICE, 0, false, NULL );
	}

	if ( spawnArgs.GetString( "model_death", "", &modelDeath ) ) {
		// lost soul is only case that does not use a ragdoll and has a model_death so get the death sound in here
		StartSound( "snd_death", SND_CHANNEL_VOICE, 0, false, NULL );
		renderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );
		SetModel( modelDeath );
		physicsObj.SetLinearVelocity( vec3_zero );
		physicsObj.PutToRest();
		physicsObj.DisableImpact();
	}

	// AI becomes frobable on death
	Event_SetFrobable( true );

	restartParticles = false;

	if (m_TaskQueue && m_killedTask.Length())
	{
		m_TaskQueue->Push(m_killedTaskPriority, m_killedTask.c_str());
	}
	else
	{
		state = GetScriptFunction( "state_Killed" );
		SetState( state );
		SetWaitState( "" );
	}

	// drop items
	DropOnRagdoll();

	if ( ( attacker && attacker->IsType( idPlayer::Type ) ) && ( inflictor && !inflictor->IsType( idSoulCubeMissile::Type ) ) )
	{
		static_cast< idPlayer* >( attacker )->AddAIKill();
		bPlayerResponsible = ( attacker == gameLocal.GetLocalPlayer() );
	}

	// Update TDM objective system
	gameLocal.m_MissionData->MissionEvent( COMP_KILL, this, bPlayerResponsible );
}

/***********************************************************************

	Targeting/Combat

***********************************************************************/

/*
=====================
idAI::PlayCinematic
=====================
*/
void idAI::PlayCinematic( void ) {
	const char *animname;

	if ( current_cinematic >= num_cinematics ) {
		if ( g_debugCinematic.GetBool() ) {
			gameLocal.Printf( "%d: '%s' stop\n", gameLocal.framenum, GetName() );
		}
		if ( !spawnArgs.GetBool( "cinematic_no_hide" ) ) {
			Hide();
		}
		current_cinematic = 0;
		ActivateTargets( gameLocal.GetLocalPlayer() );
		fl.neverDormant = false;

		return;
	}

	Show();
	current_cinematic++;

	allowJointMod = false;
	allowEyeFocus = false;

	spawnArgs.GetString( va( "anim%d", current_cinematic ), NULL, &animname );
	if ( !animname ) {
		gameLocal.Warning( "missing 'anim%d' key on %s", current_cinematic, name.c_str() );
		return;
	}

	if ( g_debugCinematic.GetBool() ) {
		gameLocal.Printf( "%d: '%s' start '%s'\n", gameLocal.framenum, GetName(), animname );
	}

	headAnim.animBlendFrames = 0;
	headAnim.lastAnimBlendFrames = 0;
	headAnim.BecomeIdle();

	legsAnim.animBlendFrames = 0;
	legsAnim.lastAnimBlendFrames = 0;
	legsAnim.BecomeIdle();

	torsoAnim.animBlendFrames = 0;
	torsoAnim.lastAnimBlendFrames = 0;
	ProcessEvent( &AI_PlayAnim, ANIMCHANNEL_TORSO, animname );

	// make sure our model gets updated
	animator.ForceUpdate();

	// update the anim bounds
	UpdateAnimation();
	UpdateVisuals();
	Present();

	if ( head.GetEntity() ) {
		// since the body anim was updated, we need to run physics to update the position of the head
		RunPhysics();

		// make sure our model gets updated
		head.GetEntity()->GetAnimator()->ForceUpdate();

		// update the anim bounds
		head.GetEntity()->UpdateAnimation();
		head.GetEntity()->UpdateVisuals();
		head.GetEntity()->Present();
	}


	fl.neverDormant = true;

}

/*
=====================
idAI::Activate

Notifies the script that a monster has been activated by a trigger or flashlight

DarkMod: Commented out calls to SetEnemy.  We don't want the AI calling setEnemy,
only the alert state scripts.
=====================
*/
void idAI::Activate( idEntity *activator ) {
	idPlayer *player;

	if ( AI_DEAD || AI_KNOCKEDOUT ) {
		// ignore it when they're dead or KO'd
		return;
	}

	// make sure he's not dormant
	dormantStart = 0;

	if ( num_cinematics ) {
		PlayCinematic();
	} else {
		AI_ACTIVATED = true;
		if ( !activator || !activator->IsType( idPlayer::Type ) ) {
			player = gameLocal.GetLocalPlayer();
		} else {
			player = static_cast<idPlayer *>( activator );
		}

		if ( ReactionTo( player ) & ATTACK_ON_ACTIVATE )
		{
			//SetEnemy( player );
		}

		// update the script in cinematics so that entities don't start anims or show themselves a frame late.
		if ( cinematic ) {
            UpdateAIScript();

			// make sure our model gets updated
			animator.ForceUpdate();

			// update the anim bounds
			UpdateAnimation();
			UpdateVisuals();
			Present();

			if ( head.GetEntity() ) {
				// since the body anim was updated, we need to run physics to update the position of the head
				RunPhysics();

				// make sure our model gets updated
				head.GetEntity()->GetAnimator()->ForceUpdate();

				// update the anim bounds
				head.GetEntity()->UpdateAnimation();
				head.GetEntity()->UpdateVisuals();
				head.GetEntity()->Present();
			}
		}
	}
}

/*
=====================
idAI::EnemyDead
=====================
*/
void idAI::EnemyDead( void ) {
	ClearEnemy();
	AI_ENEMY_DEAD = true;
}

/*
=====================
idAI::TalkTo
=====================
*/
void idAI::TalkTo( idActor *actor ) {
	if ( talk_state != TALK_OK ) {
		return;
	}

	talkTarget = actor;
	if ( actor ) {
		AI_TALK = true;
	} else {
		AI_TALK = false;
	}
}

/*
=====================
idAI::GetEnemy
=====================
*/
idActor	*idAI::GetEnemy( void ) const {
	return enemy.GetEntity();
}

/*
=====================
idAI::GetTalkState
=====================
*/
talkState_t idAI::GetTalkState( void ) const {
	if ( ( talk_state != TALK_NEVER ) && AI_DEAD ) {
		return TALK_DEAD;
	}
	if ( IsHidden() ) {
		return TALK_NEVER;
	}
	return talk_state;
}

/*
=====================
idAI::TouchedByFlashlight
=====================
*/
void idAI::TouchedByFlashlight( idActor *flashlight_owner ) {
	if ( wakeOnFlashlight ) {
		Activate( flashlight_owner );
	}
}

/*
=====================
idAI::ClearEnemy
=====================
*/
void idAI::ClearEnemy( void ) {
	if ( move.moveCommand == MOVE_TO_ENEMY ) {
		StopMove( MOVE_STATUS_DEST_NOT_FOUND );
	}

	enemyNode.Remove();
	enemy				= NULL;
	AI_ENEMY_IN_FOV		= false;
	AI_ENEMY_VISIBLE	= false;
	AI_ENEMY_DEAD		= true;

	SetChatSound();
}

/*
=====================
idAI::EnemyPositionValid
=====================
*/
bool idAI::EnemyPositionValid( void ) const {
	trace_t	tr;
	idVec3	muzzle;
	idMat3	axis;

	if ( !enemy.GetEntity() ) {
		return false;
	}

	if ( AI_ENEMY_VISIBLE ) {
		return true;
	}

	gameLocal.clip.TracePoint( tr, GetEyePosition(), lastVisibleEnemyPos + lastVisibleEnemyEyeOffset, MASK_OPAQUE, this );
	if ( tr.fraction < 1.0f ) {
		// can't see the area yet, so don't know if he's there or not
		return true;
	}

	return false;
}

/*
=====================
idAI::SetEnemyPosition
=====================
*/
void idAI::SetEnemyPosition( void ) {
	idActor		*enemyEnt = enemy.GetEntity();
	int			enemyAreaNum;
	int			areaNum;
	int			lastVisibleReachableEnemyAreaNum = -1;
	aasPath_t	path;
	idVec3		pos;
	bool		onGround;

	if ( !enemyEnt ) {
		return;
	}

	lastVisibleReachableEnemyPos = lastReachableEnemyPos;
	lastVisibleEnemyEyeOffset = enemyEnt->EyeOffset();
	lastVisibleEnemyPos = enemyEnt->GetPhysics()->GetOrigin();
	if ( move.moveType == MOVETYPE_FLY ) {
		pos = lastVisibleEnemyPos;
		onGround = true;
	} else {
		onGround = enemyEnt->GetFloorPos( 64.0f, pos );
		if ( enemyEnt->OnLadder() ) {
			onGround = false;
		}
	}

	if ( !onGround ) {
		if ( move.moveCommand == MOVE_TO_ENEMY ) {
			AI_DEST_UNREACHABLE = true;
		}
		return;
	}

	// when we don't have an AAS, we can't tell if an enemy is reachable or not,
	// so just assume that he is.
	if ( !aas ) {
		lastVisibleReachableEnemyPos = lastVisibleEnemyPos;
		if ( move.moveCommand == MOVE_TO_ENEMY ) {
			AI_DEST_UNREACHABLE = false;
		}
		enemyAreaNum = 0;
		areaNum = 0;
	} else {
		lastVisibleReachableEnemyAreaNum = move.toAreaNum;
		enemyAreaNum = PointReachableAreaNum( lastVisibleEnemyPos, 1.0f );
		if ( !enemyAreaNum ) {
			enemyAreaNum = PointReachableAreaNum( lastReachableEnemyPos, 1.0f );
			pos = lastReachableEnemyPos;
		}
		if ( !enemyAreaNum ) {
			if ( move.moveCommand == MOVE_TO_ENEMY ) {
				AI_DEST_UNREACHABLE = true;
			}
			areaNum = 0;
		} else {
			const idVec3 &org = physicsObj.GetOrigin();
			areaNum = PointReachableAreaNum( org );
			if ( PathToGoal( path, areaNum, org, enemyAreaNum, pos ) ) {
				lastVisibleReachableEnemyPos = pos;
				lastVisibleReachableEnemyAreaNum = enemyAreaNum;
				if ( move.moveCommand == MOVE_TO_ENEMY ) {
					AI_DEST_UNREACHABLE = false;
				}
			} else if ( move.moveCommand == MOVE_TO_ENEMY ) {
				AI_DEST_UNREACHABLE = true;
			}
		}
	}

	if ( move.moveCommand == MOVE_TO_ENEMY ) {
		if ( !aas ) {
			// keep the move destination up to date for wandering
			move.moveDest = lastVisibleReachableEnemyPos;
		} else if ( enemyAreaNum ) {
			move.toAreaNum = lastVisibleReachableEnemyAreaNum;
			move.moveDest = lastVisibleReachableEnemyPos;
		}

		if ( move.moveType == MOVETYPE_FLY ) {
			predictedPath_t path;
			idVec3 end = move.moveDest;
			end.z += enemyEnt->EyeOffset().z + fly_offset;
			idAI::PredictPath( this, aas, move.moveDest, end - move.moveDest, 1000, 1000, SE_BLOCKED, path );
			move.moveDest = path.endPos;
			move.toAreaNum = PointReachableAreaNum( move.moveDest, 1.0f );
		}
	}
}

/*
=====================
idAI::UpdateEnemyPosition
=====================
*/
void idAI::UpdateEnemyPosition( void ) {
	idActor *enemyEnt = enemy.GetEntity();
	int				enemyAreaNum;
	int				areaNum;
	aasPath_t		path;
	predictedPath_t predictedPath;
	idVec3			enemyPos;
	bool			onGround;

	if ( !enemyEnt ) {
		return;
	}

	const idVec3 &org = physicsObj.GetOrigin();

	if ( move.moveType == MOVETYPE_FLY ) {
		enemyPos = enemyEnt->GetPhysics()->GetOrigin();
		onGround = true;
	} else {
		onGround = enemyEnt->GetFloorPos( 64.0f, enemyPos );
		if ( enemyEnt->OnLadder() ) {
			onGround = false;
		}
	}

	if ( onGround ) {
		// when we don't have an AAS, we can't tell if an enemy is reachable or not,
		// so just assume that he is.
		if ( !aas ) {
			enemyAreaNum = 0;
			lastReachableEnemyPos = enemyPos;
		} else {
			enemyAreaNum = PointReachableAreaNum( enemyPos, 1.0f );
			if ( enemyAreaNum ) {
				areaNum = PointReachableAreaNum( org );
				if ( PathToGoal( path, areaNum, org, enemyAreaNum, enemyPos ) ) {
					lastReachableEnemyPos = enemyPos;
				}
			}
		}
	}

	AI_ENEMY_IN_FOV		= false;
	AI_ENEMY_VISIBLE	= false;

	if ( CanSee( enemyEnt, false ) )
	{

		AI_ENEMY_VISIBLE = true;
		if ( CheckFOV( enemyEnt->GetPhysics()->GetOrigin() ) )
		{
			AI_ENEMY_IN_FOV = true;
		}

		SetEnemyPosition();
	}
	else
	{
		// check if we heard any sounds in the last frame
		if ( enemyEnt == gameLocal.GetAlertEntity() ) {
			float dist = ( enemyEnt->GetPhysics()->GetOrigin() - org ).LengthSqr();
			if ( dist < Square( AI_HEARING_RANGE ) ) {
				SetEnemyPosition();
			}
		}
	}

	if ( ai_debugMove.GetBool() ) {
		gameRenderWorld->DebugBounds( colorLtGrey, enemyEnt->GetPhysics()->GetBounds(), lastReachableEnemyPos, gameLocal.msec );
		gameRenderWorld->DebugBounds( colorWhite, enemyEnt->GetPhysics()->GetBounds(), lastVisibleReachableEnemyPos, gameLocal.msec );
	}
}

/*
=====================
idAI::SetEnemy
=====================
*/
void idAI::SetEnemy( idActor *newEnemy ) {
	int enemyAreaNum;

	if ( AI_DEAD || AI_KNOCKEDOUT ) {
		ClearEnemy();
		return;
	}

	AI_ENEMY_DEAD = false;
	if ( !newEnemy ) {
		ClearEnemy();
	} else if ( enemy.GetEntity() != newEnemy ) {
		enemy = newEnemy;
		enemyNode.AddToEnd( newEnemy->enemyList );
		if ( newEnemy->health <= 0 ) {
			EnemyDead();
			return;
		}
		// let the monster know where the enemy is
		newEnemy->GetAASLocation( aas, lastReachableEnemyPos, enemyAreaNum );
		SetEnemyPosition();
		SetChatSound();

		lastReachableEnemyPos = lastVisibleEnemyPos;
		lastVisibleReachableEnemyPos = lastReachableEnemyPos;
		enemyAreaNum = PointReachableAreaNum( lastReachableEnemyPos, 1.0f );
		if ( aas && enemyAreaNum ) {
			aas->PushPointIntoAreaNum( enemyAreaNum, lastReachableEnemyPos );
			lastVisibleReachableEnemyPos = lastReachableEnemyPos;
		}
	}
}

/*
============
idAI::FirstVisiblePointOnPath
============
*/
idVec3 idAI::FirstVisiblePointOnPath( const idVec3 origin, const idVec3 &target, int travelFlags ) const {
	int i, areaNum, targetAreaNum, curAreaNum, travelTime;
	idVec3 curOrigin;
	idReachability *reach;

	if ( !aas ) {
		return origin;
	}

	areaNum = PointReachableAreaNum( origin );
	targetAreaNum = PointReachableAreaNum( target );

	if ( !areaNum || !targetAreaNum ) {
		return origin;
	}

	if ( ( areaNum == targetAreaNum ) || PointVisible( origin ) ) {
		return origin;
	}

	curAreaNum = areaNum;
	curOrigin = origin;

	for( i = 0; i < 10; i++ ) {

		if ( !aas->RouteToGoalArea( curAreaNum, curOrigin, targetAreaNum, travelFlags, travelTime, &reach ) ) {
			break;
		}

		if ( !reach ) {
			return target;
		}

		curAreaNum = reach->toAreaNum;
		curOrigin = reach->end;

		if ( PointVisible( curOrigin ) ) {
			return curOrigin;
		}
	}

	return origin;
}

/*
===================
idAI::CalculateAttackOffsets

calculate joint positions on attack frames so we can do proper "can hit" tests
===================
*/
void idAI::CalculateAttackOffsets( void ) {
	const idDeclModelDef	*modelDef;
	int						num;
	int						i;
	int						frame;
	const frameCommand_t	*command;
	idMat3					axis;
	const idAnim			*anim;
	jointHandle_t			joint;

	modelDef = animator.ModelDef();
	if ( !modelDef ) {
		return;
	}
	num = modelDef->NumAnims();

	// needs to be off while getting the offsets so that we account for the distance the monster moves in the attack anim
	animator.RemoveOriginOffset( false );

	// anim number 0 is reserved for non-existant anims.  to avoid off by one issues, just allocate an extra spot for
	// launch offsets so that anim number can be used without subtracting 1.
	missileLaunchOffset.SetGranularity( 1 );
	missileLaunchOffset.SetNum( num + 1 );
	missileLaunchOffset[ 0 ].Zero();

	for( i = 1; i <= num; i++ ) {
		missileLaunchOffset[ i ].Zero();
		anim = modelDef->GetAnim( i );
		if ( anim ) {
			frame = anim->FindFrameForFrameCommand( FC_LAUNCHMISSILE, &command );
			if ( frame >= 0 ) {
				joint = animator.GetJointHandle( command->string->c_str() );
				if ( joint == INVALID_JOINT ) {
					gameLocal.Error( "Invalid joint '%s' on 'launch_missile' frame command on frame %d of model '%s'", command->string->c_str(), frame, modelDef->GetName() );
				}
				GetJointTransformForAnim( joint, i, FRAME2MS( frame ), missileLaunchOffset[ i ], axis );
			}
		}
	}

	animator.RemoveOriginOffset( true );
}

/*
=====================
idAI::CreateProjectileClipModel
=====================
*/
void idAI::CreateProjectileClipModel( void ) const {
	if ( projectileClipModel == NULL ) {
		idBounds projectileBounds( vec3_origin );
		projectileBounds.ExpandSelf( projectileRadius );
		projectileClipModel	= new idClipModel( idTraceModel( projectileBounds ) );
	}
}

/*
=====================
idAI::GetAimDir
=====================
*/
bool idAI::GetAimDir( const idVec3 &firePos, idEntity *aimAtEnt, const idEntity *ignore, idVec3 &aimDir ) const {
	idVec3	targetPos1;
	idVec3	targetPos2;
	idVec3	delta;
	float	max_height;
	bool	result;

	// if no aimAtEnt or projectile set
	if ( !aimAtEnt || !projectileDef ) {
		aimDir = viewAxis[ 0 ] * physicsObj.GetGravityAxis();
		return false;
	}

	if ( projectileClipModel == NULL ) {
		CreateProjectileClipModel();
	}

	if ( aimAtEnt == enemy.GetEntity() ) {
		static_cast<idActor *>( aimAtEnt )->GetAIAimTargets( lastVisibleEnemyPos, targetPos1, targetPos2 );
	} else if ( aimAtEnt->IsType( idActor::Type ) ) {
		static_cast<idActor *>( aimAtEnt )->GetAIAimTargets( aimAtEnt->GetPhysics()->GetOrigin(), targetPos1, targetPos2 );
	} else {
		targetPos1 = aimAtEnt->GetPhysics()->GetAbsBounds().GetCenter();
		targetPos2 = targetPos1;
	}

	// try aiming for chest
	delta = firePos - targetPos1;
	max_height = delta.LengthFast() * projectile_height_to_distance_ratio;
	result = PredictTrajectory( firePos, targetPos1, projectileSpeed, projectileGravity, projectileClipModel, MASK_SHOT_RENDERMODEL, max_height, ignore, aimAtEnt, ai_debugTrajectory.GetBool() ? 1000 : 0, aimDir );
	if ( result || !aimAtEnt->IsType( idActor::Type ) ) {
		return result;
	}

	// try aiming for head
	delta = firePos - targetPos2;
	max_height = delta.LengthFast() * projectile_height_to_distance_ratio;
	result = PredictTrajectory( firePos, targetPos2, projectileSpeed, projectileGravity, projectileClipModel, MASK_SHOT_RENDERMODEL, max_height, ignore, aimAtEnt, ai_debugTrajectory.GetBool() ? 1000 : 0, aimDir );

	return result;
}

/*
=====================
idAI::BeginAttack
=====================
*/
void idAI::BeginAttack( const char *name ) {
	attack = name;
	lastAttackTime = gameLocal.time;
}

/*
=====================
idAI::EndAttack
=====================
*/
void idAI::EndAttack( void ) {
	attack = "";
}

/*
=====================
idAI::CreateProjectile
=====================
*/
idProjectile *idAI::CreateProjectile( const idVec3 &pos, const idVec3 &dir ) {
	idEntity *ent;
	const char *clsname;

	if ( !projectile.GetEntity() ) {
		gameLocal.SpawnEntityDef( *projectileDef, &ent, false );
		if ( !ent ) {
			clsname = projectileDef->GetString( "classname" );
			gameLocal.Error( "Could not spawn entityDef '%s'", clsname );
		}

		if ( !ent->IsType( idProjectile::Type ) ) {
			clsname = ent->GetClassname();
			gameLocal.Error( "'%s' is not an idProjectile", clsname );
		}
		projectile = ( idProjectile * )ent;
	}

	projectile.GetEntity()->Create( this, pos, dir );

	return projectile.GetEntity();
}

/*
=====================
idAI::RemoveProjectile
=====================
*/
void idAI::RemoveProjectile( void ) {
	if ( projectile.GetEntity() ) {
		projectile.GetEntity()->PostEventMS( &EV_Remove, 0 );
		projectile = NULL;
	}
}

/*
=====================
idAI::LaunchProjectile
=====================
*/
idProjectile *idAI::LaunchProjectile( const char *jointname, idEntity *target, bool clampToAttackCone ) {
	idVec3				muzzle;
	idVec3				dir;
	idVec3				start;
	trace_t				tr;
	idBounds			projBounds;
	float				distance;
	const idClipModel	*projClip;
	float				attack_accuracy;
	float				attack_cone;
	float				projectile_spread;
	float				diff;
	float				angle;
	float				spin;
	idAngles			ang;
	int					num_projectiles;
	int					i;
	idMat3				axis;
	idVec3				tmp;
	idProjectile		*lastProjectile;

	if ( !projectileDef ) {
		gameLocal.Warning( "%s (%s) doesn't have a projectile specified", name.c_str(), GetEntityDefName() );
		return NULL;
	}

	attack_accuracy = spawnArgs.GetFloat( "attack_accuracy", "7" );
	attack_cone = spawnArgs.GetFloat( "attack_cone", "70" );
	projectile_spread = spawnArgs.GetFloat( "projectile_spread", "0" );
	num_projectiles = spawnArgs.GetInt( "num_projectiles", "1" );

	GetMuzzle( jointname, muzzle, axis );

	if ( !projectile.GetEntity() ) {
		CreateProjectile( muzzle, axis[ 0 ] );
	}

	lastProjectile = projectile.GetEntity();

	if ( target != NULL ) {
		tmp = target->GetPhysics()->GetAbsBounds().GetCenter() - muzzle;
		tmp.Normalize();
		axis = tmp.ToMat3();
	} else {
		axis = viewAxis;
	}

	// rotate it because the cone points up by default
	tmp = axis[2];
	axis[2] = axis[0];
	axis[0] = -tmp;

	// make sure the projectile starts inside the monster bounding box
	const idBounds &ownerBounds = physicsObj.GetAbsBounds();
	projClip = lastProjectile->GetPhysics()->GetClipModel();
	projBounds = projClip->GetBounds().Rotate( axis );

	// check if the owner bounds is bigger than the projectile bounds
	if ( ( ( ownerBounds[1][0] - ownerBounds[0][0] ) > ( projBounds[1][0] - projBounds[0][0] ) ) &&
		( ( ownerBounds[1][1] - ownerBounds[0][1] ) > ( projBounds[1][1] - projBounds[0][1] ) ) &&
		( ( ownerBounds[1][2] - ownerBounds[0][2] ) > ( projBounds[1][2] - projBounds[0][2] ) ) ) {
		if ( (ownerBounds - projBounds).RayIntersection( muzzle, viewAxis[ 0 ], distance ) ) {
			start = muzzle + distance * viewAxis[ 0 ];
		} else {
			start = ownerBounds.GetCenter();
		}
	} else {
		// projectile bounds bigger than the owner bounds, so just start it from the center
		start = ownerBounds.GetCenter();
	}

	gameLocal.clip.Translation( tr, start, muzzle, projClip, axis, MASK_SHOT_RENDERMODEL, this );
	muzzle = tr.endpos;

	// set aiming direction
	GetAimDir( muzzle, target, this, dir );
	ang = dir.ToAngles();

	// adjust his aim so it's not perfect.  uses sine based movement so the tracers appear less random in their spread.
	float t = MS2SEC( gameLocal.time + entityNumber * 497 );
	ang.pitch += idMath::Sin16( t * 5.1 ) * attack_accuracy;
	ang.yaw	+= idMath::Sin16( t * 6.7 ) * attack_accuracy;

	if ( clampToAttackCone ) {
		// clamp the attack direction to be within monster's attack cone so he doesn't do
		// things like throw the missile backwards if you're behind him
		diff = idMath::AngleDelta( ang.yaw, current_yaw );
		if ( diff > attack_cone ) {
			ang.yaw = current_yaw + attack_cone;
		} else if ( diff < -attack_cone ) {
			ang.yaw = current_yaw - attack_cone;
		}
	}

	axis = ang.ToMat3();

	float spreadRad = DEG2RAD( projectile_spread );
	for( i = 0; i < num_projectiles; i++ ) {
		// spread the projectiles out
		angle = idMath::Sin( spreadRad * gameLocal.random.RandomFloat() );
		spin = (float)DEG2RAD( 360.0f ) * gameLocal.random.RandomFloat();
		dir = axis[ 0 ] + axis[ 2 ] * ( angle * idMath::Sin( spin ) ) - axis[ 1 ] * ( angle * idMath::Cos( spin ) );
		dir.Normalize();

		// launch the projectile
		if ( !projectile.GetEntity() ) {
			CreateProjectile( muzzle, dir );
		}
		lastProjectile = projectile.GetEntity();
		lastProjectile->Launch( muzzle, dir, vec3_origin );
		projectile = NULL;
	}

	TriggerWeaponEffects( muzzle );

	lastAttackTime = gameLocal.time;

	return lastProjectile;
}

/*
================
idAI::DamageFeedback

callback function for when another entity recieved damage from this entity.  damage can be adjusted and returned to the caller.

FIXME: This gets called when we call idPlayer::CalcDamagePoints from idAI::AttackMelee, which then checks for a saving throw,
possibly forcing a miss.  This is harmless behavior ATM, but is not intuitive.
================
*/
void idAI::DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage ) {
	if ( ( victim == this ) && inflictor->IsType( idProjectile::Type ) ) {
		// monsters only get half damage from their own projectiles
		damage = ( damage + 1 ) / 2;  // round up so we don't do 0 damage

	} else if ( victim == enemy.GetEntity() ) {
		AI_HIT_ENEMY = true;
	}
}

/*
=====================
idAI::DirectDamage

Causes direct damage to an entity

kickDir is specified in the monster's coordinate system, and gives the direction
that the view kick and knockback should go
=====================
*/
void idAI::DirectDamage( const char *meleeDefName, idEntity *ent ) {
	const idDict *meleeDef;
	const char *p;
	const idSoundShader *shader;

	meleeDef = gameLocal.FindEntityDefDict( meleeDefName, false );
	if ( !meleeDef ) {
		gameLocal.Error( "Unknown damage def '%s' on '%s'", meleeDefName, name.c_str() );
	}

	if ( !ent->fl.takedamage ) {
		const idSoundShader *shader = declManager->FindSound(meleeDef->GetString( "snd_miss" ));
		StartSoundShader( shader, SND_CHANNEL_DAMAGE, 0, false, NULL );
		return;
	}

	//
	// do the damage
	//
	p = meleeDef->GetString( "snd_hit" );
	if ( p && *p ) {
		shader = declManager->FindSound( p );
		StartSoundShader( shader, SND_CHANNEL_DAMAGE, 0, false, NULL );
	}

	idVec3	kickDir;
	meleeDef->GetVector( "kickDir", "0 0 0", kickDir );

	idVec3	globalKickDir;
	globalKickDir = ( viewAxis * physicsObj.GetGravityAxis() ) * kickDir;

	ent->Damage( this, this, globalKickDir, meleeDefName, 1.0f, INVALID_JOINT );

	// end the attack if we're a multiframe attack
	EndAttack();
}

/*
=====================
idAI::TestMelee
=====================
*/
bool idAI::TestMelee( void ) const {
	trace_t trace;
	idActor *enemyEnt = enemy.GetEntity();

	if ( !enemyEnt || !melee_range ) {
		return false;
	}

	//FIXME: make work with gravity vector
	idVec3 org = physicsObj.GetOrigin();
	const idBounds &myBounds = physicsObj.GetBounds();
	idBounds bounds;

	// expand the bounds out by our melee range
	bounds[0][0] = -melee_range;
	bounds[0][1] = -melee_range;
	bounds[0][2] = myBounds[0][2] - 4.0f;
	bounds[1][0] = melee_range;
	bounds[1][1] = melee_range;
	bounds[1][2] = myBounds[1][2] + 4.0f;
	bounds.TranslateSelf( org );

	idVec3 enemyOrg = enemyEnt->GetPhysics()->GetOrigin();
	idBounds enemyBounds = enemyEnt->GetPhysics()->GetBounds();
	enemyBounds.TranslateSelf( enemyOrg );

	if ( ai_debugMove.GetBool() ) {
		gameRenderWorld->DebugBounds( colorYellow, bounds, vec3_zero, gameLocal.msec );
	}

	if ( !bounds.IntersectsBounds( enemyBounds ) ) {
		return false;
	}

	idVec3 start = GetEyePosition();
	idVec3 end = enemyEnt->GetEyePosition();

	gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_BOUNDINGBOX, this );
	if ( ( trace.fraction == 1.0f ) || ( gameLocal.GetTraceEntity( trace ) == enemyEnt ) ) {
		return true;
	}

	return false;
}

/*
=====================
idAI::AttackMelee

jointname allows the endpoint to be exactly specified in the model,
as for the commando tentacle.  If not specified, it will be set to
the facing direction + melee_range.

kickDir is specified in the monster's coordinate system, and gives the direction
that the view kick and knockback should go

DarkMod : Took out saving throws.
=====================
*/
bool idAI::AttackMelee( const char *meleeDefName ) {
	const idDict *meleeDef;
	idActor *enemyEnt = enemy.GetEntity();
	const char *p;
	const idSoundShader *shader;

	meleeDef = gameLocal.FindEntityDefDict( meleeDefName, false );
	if ( !meleeDef ) {
		gameLocal.Error( "Unknown melee '%s'", meleeDefName );
	}

	if ( !enemyEnt ) {
		p = meleeDef->GetString( "snd_miss" );
		if ( p && *p ) {
			shader = declManager->FindSound( p );
			StartSoundShader( shader, SND_CHANNEL_DAMAGE, 0, false, NULL );
		}
		return false;
	}

	// check for the "saving throw" automatic melee miss on lethal blow
	// stupid place for this.

/**
* Saving throws removed.  Uncomment the following for saving throws.
**/

/*** BEGIN SAVING THROWS SECTION *****
	bool forceMiss = false;
	if ( enemyEnt->IsType( idPlayer::Type ) && g_skill.GetInteger() < 2 ) {
		int	damage, armor;
		idPlayer *player = static_cast<idPlayer*>( enemyEnt );
		player->CalcDamagePoints( this, this, meleeDef, 1.0f, INVALID_JOINT, &damage, &armor );

		if ( enemyEnt->health <= damage ) {
			int	t = gameLocal.time - player->lastSavingThrowTime;
			if ( t > SAVING_THROW_TIME ) {
				player->lastSavingThrowTime = gameLocal.time;
				t = 0;
			}
			if ( t < 1000 ) {
				gameLocal.Printf( "Saving throw.\n" );
				forceMiss = true;
			}
		}
	}


	// make sure the trace can actually hit the enemy
	if ( forceMiss || !TestMelee() ) {
****** END SAVING THROWS SECTION *******/

	if ( !TestMelee() )
	{
		// missed
		p = meleeDef->GetString( "snd_miss" );
		if ( p && *p ) {
			shader = declManager->FindSound( p );
			StartSoundShader( shader, SND_CHANNEL_DAMAGE, 0, false, NULL );
		}
		return false;
	}

	//
	// do the damage
	//
	p = meleeDef->GetString( "snd_hit" );
	if ( p && *p ) {
		shader = declManager->FindSound( p );
		StartSoundShader( shader, SND_CHANNEL_DAMAGE, 0, false, NULL );
	}

	idVec3	kickDir;
	meleeDef->GetVector( "kickDir", "0 0 0", kickDir );

	idVec3	globalKickDir;
	globalKickDir = ( viewAxis * physicsObj.GetGravityAxis() ) * kickDir;

	enemyEnt->Damage( this, this, globalKickDir, meleeDefName, 1.0f, INVALID_JOINT );

	// cause a LARGE tactile alert in the enemy, if it is an AI
	if( enemyEnt->IsType(idAI::Type) )
	{
		static_cast<idAI *>(enemyEnt)->TactileAlert( this, 100 );
	}

	lastAttackTime = gameLocal.time;

	return true;
}

/*
================
idAI::PushWithAF
================
*/
void idAI::PushWithAF( void ) {
	int i, j;
	afTouch_t touchList[ MAX_GENTITIES ];
	idEntity *pushed_ents[ MAX_GENTITIES ];
	idEntity *ent;
	idVec3 vel( vec3_origin ), vGravNorm( vec3_origin );
	int num_pushed;

	num_pushed = 0;
	af.ChangePose( this, gameLocal.time );
	int num = af.EntitiesTouchingAF( touchList );

	for( i = 0; i < num; i++ ) {
		if ( touchList[ i ].touchedEnt->IsType( idProjectile::Type ) ) {
			// skip projectiles
			continue;
		}

		// make sure we havent pushed this entity already.  this avoids causing double damage
		for( j = 0; j < num_pushed; j++ ) {
			if ( pushed_ents[ j ] == touchList[ i ].touchedEnt ) {
				break;
			}
		}
		if ( j >= num_pushed )
		{
			ent = touchList[ i ].touchedEnt;
			pushed_ents[num_pushed++] = ent;
			vel = ent->GetPhysics()->GetAbsBounds().GetCenter() - touchList[ i ].touchedByBody->GetWorldOrigin();

			if ( ent->IsType(idPlayer::Type) && static_cast<idPlayer *>(ent)->noclip )
			{
				// skip player when noclip is on
				continue;
			}

			if( ent->IsType(idActor::Type) )
			{

				// Id code to stop from pushing the enemy back during melee
				// TODO: This will change with new melee system
				if ( attack.Length() )
				{
					// TODO: Don't need to do this right now, but keep in mind for future melee system
					ent->Damage( this, this, vel, attack, 1.0f, INVALID_JOINT );
				} else
				{
					// Ishtvan: Resolve velocity on to XY plane to stop from pushing AI up
					vGravNorm = physicsObj.GetGravityNormal();
					vel -= (vel * vGravNorm ) * vGravNorm;
					vel.Normalize();
					ent->GetPhysics()->SetLinearVelocity( 80.0f * vel, touchList[ i ].touchedClipModel->GetId() );
				}

				// Tactile Alert:
				if( ent->IsType(idPlayer::Type) )
				{
					// aesthetics: Dont react to dead player?
					if( ent->health > 0 )
						HadTactile( static_cast<idActor *>(ent) );
				}
				else if( ent->IsType(idAI::Type) && (ent->health > 0) && !static_cast<idAI *>(ent)->AI_KNOCKEDOUT )
				{
					HadTactile( static_cast<idActor *>(ent) );
				}
				else
				{
					// TODO: Touched a dead or unconscious body, should issue a body alert
				}
			}
			// Ent was not an actor:
			else
			{
				vel.Normalize();
				ent->ApplyImpulse( this, touchList[i].touchedClipModel->GetId(), ent->GetPhysics()->GetOrigin(), cv_ai_bumpobject_impulse.GetFloat() * vel );
			}
		}
	}
}

/***********************************************************************

	Misc

***********************************************************************/

/*
================
idAI::GetMuzzle
================
*/
void idAI::GetMuzzle( const char *jointname, idVec3 &muzzle, idMat3 &axis ) {
	jointHandle_t joint;

	if ( !jointname || !jointname[ 0 ] ) {
		muzzle = physicsObj.GetOrigin() + viewAxis[ 0 ] * physicsObj.GetGravityAxis() * 14;
		muzzle -= physicsObj.GetGravityNormal() * physicsObj.GetBounds()[ 1 ].z * 0.5f;
	} else {
		joint = animator.GetJointHandle( jointname );
		if ( joint == INVALID_JOINT ) {
			gameLocal.Error( "Unknown joint '%s' on %s", jointname, GetEntityDefName() );
		}
		GetJointWorldTransform( joint, gameLocal.time, muzzle, axis );
	}
}

/*
================
idAI::TriggerWeaponEffects
================
*/
void idAI::TriggerWeaponEffects( const idVec3 &muzzle ) {
	idVec3 org;
	idMat3 axis;

	if ( !g_muzzleFlash.GetBool() ) {
		return;
	}

	// muzzle flash
	// offset the shader parms so muzzle flashes show up
	renderEntity.shaderParms[SHADERPARM_TIMEOFFSET] = -MS2SEC( gameLocal.time );
	renderEntity.shaderParms[ SHADERPARM_DIVERSITY ] = gameLocal.random.CRandomFloat();

	if ( flashJointWorld != INVALID_JOINT ) {
		GetJointWorldTransform( flashJointWorld, gameLocal.time, org, axis );

		if ( worldMuzzleFlash.lightRadius.x > 0.0f ) {
			worldMuzzleFlash.axis = axis;
			worldMuzzleFlash.shaderParms[SHADERPARM_TIMEOFFSET] = -MS2SEC( gameLocal.time );
			if ( worldMuzzleFlashHandle != - 1 ) {
				gameRenderWorld->UpdateLightDef( worldMuzzleFlashHandle, &worldMuzzleFlash );
			} else {
				worldMuzzleFlashHandle = gameRenderWorld->AddLightDef( &worldMuzzleFlash );
			}
			muzzleFlashEnd = gameLocal.time + flashTime;
			UpdateVisuals();
		}
	}
}

/*
================
idAI::UpdateMuzzleFlash
================
*/
void idAI::UpdateMuzzleFlash( void ) {
	if ( worldMuzzleFlashHandle != -1 ) {
		if ( gameLocal.time >= muzzleFlashEnd ) {
			gameRenderWorld->FreeLightDef( worldMuzzleFlashHandle );
			worldMuzzleFlashHandle = -1;
		} else {
			idVec3 muzzle;
			animator.GetJointTransform( flashJointWorld, gameLocal.time, muzzle, worldMuzzleFlash.axis );
			animator.GetJointTransform( flashJointWorld, gameLocal.time, muzzle, worldMuzzleFlash.axis );
			muzzle = physicsObj.GetOrigin() + ( muzzle + modelOffset ) * viewAxis * physicsObj.GetGravityAxis();
			worldMuzzleFlash.origin = muzzle;
			gameRenderWorld->UpdateLightDef( worldMuzzleFlashHandle, &worldMuzzleFlash );
		}
	}
}

/*
================
idAI::Hide
================
*/
void idAI::Hide( void ) {
	idActor::Hide();
	fl.takedamage = false;
	physicsObj.SetContents( 0 );
	physicsObj.GetClipModel()->Unlink();
	StopSound( SND_CHANNEL_AMBIENT, false );
	SetChatSound();

	AI_ENEMY_IN_FOV		= false;
	AI_ENEMY_VISIBLE	= false;
	StopMove( MOVE_STATUS_DONE );
}

/*
================
idAI::Show
================
*/
void idAI::Show( void ) {
	idActor::Show();
	if ( spawnArgs.GetBool( "big_monster" ) ) {
		physicsObj.SetContents( 0 );
	} else if ( use_combat_bbox ) {
		physicsObj.SetContents( CONTENTS_BODY|CONTENTS_SOLID );
	} else {
		physicsObj.SetContents( CONTENTS_BODY );
	}
	// SR CONTENTS_RESONSE FIX
	if( m_StimResponseColl->HasResponse() )
		physicsObj.SetContents( physicsObj.GetContents() | CONTENTS_RESPONSE );


	physicsObj.GetClipModel()->Link( gameLocal.clip );
	fl.takedamage = !spawnArgs.GetBool( "noDamage" );
	SetChatSound();
	StartSound( "snd_ambient", SND_CHANNEL_AMBIENT, 0, false, NULL );
}

/*
=====================
idAI::SetChatSound
=====================
*/
void idAI::SetChatSound( void ) {
	const char *snd;

	if ( IsHidden() ) {
		snd = NULL;
	} else if ( enemy.GetEntity() ) {
		snd = spawnArgs.GetString( "snd_chatter_combat", NULL );
		chat_min = SEC2MS( spawnArgs.GetFloat( "chatter_combat_min", "5" ) );
		chat_max = SEC2MS( spawnArgs.GetFloat( "chatter_combat_max", "10" ) );
	} else if ( !spawnArgs.GetBool( "no_idle_chatter" ) ) {
		snd = spawnArgs.GetString( "snd_chatter", NULL );
		chat_min = SEC2MS( spawnArgs.GetFloat( "chatter_min", "5" ) );
		chat_max = SEC2MS( spawnArgs.GetFloat( "chatter_max", "10" ) );
	} else {
		snd = NULL;
	}

	if ( snd && *snd ) {
		chat_snd = declManager->FindSound( snd );

		// set the next chat time
		chat_time = gameLocal.time + chat_min + gameLocal.random.RandomFloat() * ( chat_max - chat_min );
	} else {
		chat_snd = NULL;
	}
}

/*
================
idAI::CanPlayChatterSounds

Used for playing chatter sounds on monsters.
================
*/
bool idAI::CanPlayChatterSounds( void ) const {
	if ( AI_DEAD || AI_KNOCKEDOUT ) {
		return false;
	}

	if ( IsHidden() ) {
		return false;
	}

	if ( enemy.GetEntity() ) {
		return true;
	}

	if ( spawnArgs.GetBool( "no_idle_chatter" ) ) {
		return false;
	}

	return true;
}

/*
=====================
idAI::PlayChatter
=====================
*/
void idAI::PlayChatter( void ) {
	// check if it's time to play a chat sound
	if ( AI_DEAD || AI_KNOCKEDOUT || !chat_snd || ( chat_time > gameLocal.time ) ) {
		return;
	}

	StartSoundShader( chat_snd, SND_CHANNEL_VOICE, 0, false, NULL );

	// set the next chat time
	chat_time = gameLocal.time + chat_min + gameLocal.random.RandomFloat() * ( chat_max - chat_min );
}

/*
=====================
idAI::UpdateParticles
=====================
*/
void idAI::UpdateParticles( void ) {
	if ( ( thinkFlags & TH_UPDATEPARTICLES) && !IsHidden() ) {
		idVec3 realVector;
		idMat3 realAxis;

		int particlesAlive = 0;
		for ( int i = 0; i < particles.Num(); i++ ) {
			if ( particles[i].particle && particles[i].time ) {
				particlesAlive++;
				if (af.IsActive()) {
					realAxis = mat3_identity;
					realVector = GetPhysics()->GetOrigin();
				} else {
					animator.GetJointTransform( particles[i].joint, gameLocal.time, realVector, realAxis );
					realAxis *= renderEntity.axis;
					realVector = physicsObj.GetOrigin() + ( realVector + modelOffset ) * ( viewAxis * physicsObj.GetGravityAxis() );
				}

				if ( !gameLocal.smokeParticles->EmitSmoke( particles[i].particle, particles[i].time, gameLocal.random.CRandomFloat(), realVector, realAxis )) {
					if ( restartParticles ) {
						particles[i].time = gameLocal.time;
					} else {
						particles[i].time = 0;
						particlesAlive--;
					}
				}
			}
		}
		if ( particlesAlive == 0 ) {
			BecomeInactive( TH_UPDATEPARTICLES );
		}
	}
}

/*
=====================
idAI::TriggerParticles
=====================
*/
void idAI::TriggerParticles( const char *jointName ) {
	jointHandle_t jointNum;

	jointNum = animator.GetJointHandle( jointName );
	for ( int i = 0; i < particles.Num(); i++ ) {
		if ( particles[i].joint == jointNum ) {
			particles[i].time = gameLocal.time;
			BecomeActive( TH_UPDATEPARTICLES );
		}
	}
}


/***********************************************************************

	Head & torso aiming

***********************************************************************/

/*
================
idAI::UpdateAnimationControllers
================
*/
bool idAI::UpdateAnimationControllers( void ) {
	idVec3		local;
	idVec3		focusPos;
	idQuat		jawQuat;
	idVec3		left;
	idVec3 		dir;
	idVec3 		orientationJointPos;
	idVec3 		localDir;
	idAngles 	newLookAng;
	idAngles	diff;
	idMat3		mat;
	idMat3		axis;
	idMat3		orientationJointAxis;
	idAFAttachment	*headEnt = head.GetEntity();
	idVec3		eyepos;
	idVec3		pos;
	int			i;
	idAngles	jointAng;
	float		orientationJointYaw;

	if ( AI_DEAD || AI_KNOCKEDOUT )
	{
		return idActor::UpdateAnimationControllers();
	}

	if ( orientationJoint == INVALID_JOINT ) {
		orientationJointAxis = viewAxis;
		orientationJointPos = physicsObj.GetOrigin();
		orientationJointYaw = current_yaw;
	} else {
		GetJointWorldTransform( orientationJoint, gameLocal.time, orientationJointPos, orientationJointAxis );
		orientationJointYaw = orientationJointAxis[ 2 ].ToYaw();
		orientationJointAxis = idAngles( 0.0f, orientationJointYaw, 0.0f ).ToMat3();
	}

	if ( focusJoint != INVALID_JOINT ) {
		if ( headEnt ) {
			headEnt->GetJointWorldTransform( focusJoint, gameLocal.time, eyepos, axis );
		} else {
			GetJointWorldTransform( focusJoint, gameLocal.time, eyepos, axis );
		}
		eyeOffset.z = eyepos.z - physicsObj.GetOrigin().z;
		if ( ai_debugMove.GetBool() ) {
			gameRenderWorld->DebugLine( colorRed, eyepos, eyepos + orientationJointAxis[ 0 ] * 32.0f, gameLocal.msec );
		}
	} else {
		eyepos = GetEyePosition();
	}

	if ( headEnt ) {
		CopyJointsFromBodyToHead();
	}

	// Update the IK after we've gotten all the joint positions we need, but before we set any joint positions.
	// Getting the joint positions causes the joints to be updated.  The IK gets joint positions itself (which
	// are already up to date because of getting the joints in this function) and then sets their positions, which
	// forces the heirarchy to be updated again next time we get a joint or present the model.  If IK is enabled,
	// or if we have a seperate head, we end up transforming the joints twice per frame.  Characters with no
	// head entity and no ik will only transform their joints once.  Set g_debuganim to the current entity number
	// in order to see how many times an entity transforms the joints per frame.
	idActor::UpdateAnimationControllers();

	idEntity *focusEnt = focusEntity.GetEntity();
	if ( !allowJointMod || !allowEyeFocus || ( gameLocal.time >= focusTime ) ) {
	    focusPos = GetEyePosition() + orientationJointAxis[ 0 ] * 512.0f;
	} else if ( focusEnt == NULL ) {
		// keep looking at last position until focusTime is up
		focusPos = currentFocusPos;
	} else if ( focusEnt == enemy.GetEntity() ) {
		focusPos = lastVisibleEnemyPos + lastVisibleEnemyEyeOffset - eyeVerticalOffset * enemy.GetEntity()->GetPhysics()->GetGravityNormal();
	} else if ( focusEnt->IsType( idActor::Type ) ) {
		focusPos = static_cast<idActor *>( focusEnt )->GetEyePosition() - eyeVerticalOffset * focusEnt->GetPhysics()->GetGravityNormal();
	} else {
		focusPos = focusEnt->GetPhysics()->GetOrigin();
	}

	currentFocusPos = currentFocusPos + ( focusPos - currentFocusPos ) * eyeFocusRate;

	// determine yaw from origin instead of from focus joint since joint may be offset, which can cause us to bounce between two angles
	dir = focusPos - orientationJointPos;
	newLookAng.yaw = idMath::AngleNormalize180( dir.ToYaw() - orientationJointYaw );
	newLookAng.roll = 0.0f;
	newLookAng.pitch = 0.0f;

#if 0
	gameRenderWorld->DebugLine( colorRed, orientationJointPos, focusPos, gameLocal.msec );
	gameRenderWorld->DebugLine( colorYellow, orientationJointPos, orientationJointPos + orientationJointAxis[ 0 ] * 32.0f, gameLocal.msec );
	gameRenderWorld->DebugLine( colorGreen, orientationJointPos, orientationJointPos + newLookAng.ToForward() * 48.0f, gameLocal.msec );
#endif

	// determine pitch from joint position
	dir = focusPos - eyepos;
	dir.NormalizeFast();
	orientationJointAxis.ProjectVector( dir, localDir );
	newLookAng.pitch = -idMath::AngleNormalize180( localDir.ToPitch() );
	newLookAng.roll	= 0.0f;

	diff = newLookAng - lookAng;

	if ( eyeAng != diff ) {
		eyeAng = diff;
		eyeAng.Clamp( eyeMin, eyeMax );
		idAngles angDelta = diff - eyeAng;
		if ( !angDelta.Compare( ang_zero, 0.1f ) ) {
			alignHeadTime = gameLocal.time;
		} else {
			alignHeadTime = gameLocal.time + ( 0.5f + 0.5f * gameLocal.random.RandomFloat() ) * focusAlignTime;
		}
	}

	if ( idMath::Fabs( newLookAng.yaw ) < 0.1f ) {
		alignHeadTime = gameLocal.time;
	}

	if ( ( gameLocal.time >= alignHeadTime ) || ( gameLocal.time < forceAlignHeadTime ) ) {
		alignHeadTime = gameLocal.time + ( 0.5f + 0.5f * gameLocal.random.RandomFloat() ) * focusAlignTime;
		destLookAng = newLookAng;
		destLookAng.Clamp( lookMin, lookMax );
	}

	diff = destLookAng - lookAng;
	if ( ( lookMin.pitch == -180.0f ) && ( lookMax.pitch == 180.0f ) ) {
		if ( ( diff.pitch > 180.0f ) || ( diff.pitch <= -180.0f ) ) {
			diff.pitch = 360.0f - diff.pitch;
		}
	}
	if ( ( lookMin.yaw == -180.0f ) && ( lookMax.yaw == 180.0f ) ) {
		if ( diff.yaw > 180.0f ) {
			diff.yaw -= 360.0f;
		} else if ( diff.yaw <= -180.0f ) {
			diff.yaw += 360.0f;
		}
	}
	lookAng = lookAng + diff * headFocusRate;
	lookAng.Normalize180();

	jointAng.roll = 0.0f;
	for( i = 0; i < lookJoints.Num(); i++ ) {
		jointAng.pitch	= lookAng.pitch * lookJointAngles[ i ].pitch;
		jointAng.yaw	= lookAng.yaw * lookJointAngles[ i ].yaw;
		animator.SetJointAxis( lookJoints[ i ], JOINTMOD_WORLD, jointAng.ToMat3() );
	}

	if ( move.moveType == MOVETYPE_FLY ) {
		// lean into turns
		AdjustFlyingAngles();
	}

	if ( headEnt ) {
		idAnimator *headAnimator = headEnt->GetAnimator();

		if ( allowEyeFocus ) {
			idMat3 eyeAxis = ( lookAng + eyeAng ).ToMat3(); idMat3 headTranspose = headEnt->GetPhysics()->GetAxis().Transpose();
			axis =  eyeAxis * orientationJointAxis;
			left = axis[ 1 ] * eyeHorizontalOffset;
			eyepos -= headEnt->GetPhysics()->GetOrigin();
			headAnimator->SetJointPos( leftEyeJoint, JOINTMOD_WORLD_OVERRIDE, eyepos + ( axis[ 0 ] * 64.0f + left ) * headTranspose );
			headAnimator->SetJointPos( rightEyeJoint, JOINTMOD_WORLD_OVERRIDE, eyepos + ( axis[ 0 ] * 64.0f - left ) * headTranspose );
		} else {
			headAnimator->ClearJoint( leftEyeJoint );
			headAnimator->ClearJoint( rightEyeJoint );
		}
	} else {
		if ( allowEyeFocus ) {
			idMat3 eyeAxis = ( lookAng + eyeAng ).ToMat3();
			axis =  eyeAxis * orientationJointAxis;
			left = axis[ 1 ] * eyeHorizontalOffset;
			eyepos += axis[ 0 ] * 64.0f - physicsObj.GetOrigin();
			animator.SetJointPos( leftEyeJoint, JOINTMOD_WORLD_OVERRIDE, eyepos + left );
			animator.SetJointPos( rightEyeJoint, JOINTMOD_WORLD_OVERRIDE, eyepos - left );
		} else {
			animator.ClearJoint( leftEyeJoint );
			animator.ClearJoint( rightEyeJoint );
		}
	}

	return true;
}

/***********************************************************************

idCombatNode

***********************************************************************/

const idEventDef EV_CombatNode_MarkUsed( "markUsed" );

CLASS_DECLARATION( idEntity, idCombatNode )
	EVENT( EV_CombatNode_MarkUsed,				idCombatNode::Event_MarkUsed )
	EVENT( EV_Activate,							idCombatNode::Event_Activate )
END_CLASS

/*
=====================
idCombatNode::idCombatNode
=====================
*/
idCombatNode::idCombatNode( void ) {
	min_dist = 0.0f;
	max_dist = 0.0f;
	cone_dist = 0.0f;
	min_height = 0.0f;
	max_height = 0.0f;
	cone_left.Zero();
	cone_right.Zero();
	offset.Zero();
	disabled = false;
}

/*
=====================
idCombatNode::Save
=====================
*/
void idCombatNode::Save( idSaveGame *savefile ) const {
	savefile->WriteFloat( min_dist );
	savefile->WriteFloat( max_dist );
	savefile->WriteFloat( cone_dist );
	savefile->WriteFloat( min_height );
	savefile->WriteFloat( max_height );
	savefile->WriteVec3( cone_left );
	savefile->WriteVec3( cone_right );
	savefile->WriteVec3( offset );
	savefile->WriteBool( disabled );
}

/*
=====================
idCombatNode::Restore
=====================
*/
void idCombatNode::Restore( idRestoreGame *savefile ) {
	savefile->ReadFloat( min_dist );
	savefile->ReadFloat( max_dist );
	savefile->ReadFloat( cone_dist );
	savefile->ReadFloat( min_height );
	savefile->ReadFloat( max_height );
	savefile->ReadVec3( cone_left );
	savefile->ReadVec3( cone_right );
	savefile->ReadVec3( offset );
	savefile->ReadBool( disabled );
}

/*
=====================
idCombatNode::Spawn
=====================
*/
void idCombatNode::Spawn( void ) {
	float fov;
	float yaw;
	float height;

	min_dist = spawnArgs.GetFloat( "min" );
	max_dist = spawnArgs.GetFloat( "max" );
	height = spawnArgs.GetFloat( "height" );
	fov = spawnArgs.GetFloat( "fov", "60" );
	offset = spawnArgs.GetVector( "offset" );

	const idVec3 &org = GetPhysics()->GetOrigin() + offset;
	min_height = org.z - height * 0.5f;
	max_height = min_height + height;

	const idMat3 &axis = GetPhysics()->GetAxis();
	yaw = axis[ 0 ].ToYaw();

	idAngles leftang( 0.0f, yaw + fov * 0.5f - 90.0f, 0.0f );
	cone_left = leftang.ToForward();

	idAngles rightang( 0.0f, yaw - fov * 0.5f + 90.0f, 0.0f );
	cone_right = rightang.ToForward();

	disabled = spawnArgs.GetBool( "start_off" );
}

/*
=====================
idCombatNode::IsDisabled
=====================
*/
bool idCombatNode::IsDisabled( void ) const {
	return disabled;
}

/*
=====================
idCombatNode::DrawDebugInfo
=====================
*/
void idCombatNode::DrawDebugInfo( void ) {
	idEntity		*ent;
	idCombatNode	*node;
	idPlayer		*player = gameLocal.GetLocalPlayer();
	idVec4			color;
	idBounds		bounds( idVec3( -16, -16, 0 ), idVec3( 16, 16, 0 ) );

	for( ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( !ent->IsType( idCombatNode::Type ) ) {
			continue;
		}

		node = static_cast<idCombatNode *>( ent );
		if ( node->disabled ) {
			color = colorMdGrey;
		} else if ( player && node->EntityInView( player, player->GetPhysics()->GetOrigin() ) ) {
			color = colorYellow;
		} else {
			color = colorRed;
		}

		idVec3 leftDir( -node->cone_left.y, node->cone_left.x, 0.0f );
		idVec3 rightDir( node->cone_right.y, -node->cone_right.x, 0.0f );
		idVec3 org = node->GetPhysics()->GetOrigin() + node->offset;

		bounds[ 1 ].z = node->max_height;

		leftDir.NormalizeFast();
		rightDir.NormalizeFast();

		const idMat3 &axis = node->GetPhysics()->GetAxis();
		float cone_dot = node->cone_right * axis[ 1 ];
		if ( idMath::Fabs( cone_dot ) > 0.1 ) {
			float cone_dist = node->max_dist / cone_dot;
			idVec3 pos1 = org + leftDir * node->min_dist;
			idVec3 pos2 = org + leftDir * cone_dist;
			idVec3 pos3 = org + rightDir * node->min_dist;
			idVec3 pos4 = org + rightDir * cone_dist;

			gameRenderWorld->DebugLine( color, node->GetPhysics()->GetOrigin(), ( pos1 + pos3 ) * 0.5f, gameLocal.msec );
			gameRenderWorld->DebugLine( color, pos1, pos2, gameLocal.msec );
			gameRenderWorld->DebugLine( color, pos1, pos3, gameLocal.msec );
			gameRenderWorld->DebugLine( color, pos3, pos4, gameLocal.msec );
			gameRenderWorld->DebugLine( color, pos2, pos4, gameLocal.msec );
			gameRenderWorld->DebugBounds( color, bounds, org, gameLocal.msec );
		}
	}
}

/*
=====================
idCombatNode::EntityInView
=====================
*/
bool idCombatNode::EntityInView( idActor *actor, const idVec3 &pos ) {
	if ( !actor || ( actor->health <= 0 ) ) {
		return false;
	}

	const idBounds &bounds = actor->GetPhysics()->GetBounds();
	if ( ( pos.z + bounds[ 1 ].z < min_height ) || ( pos.z + bounds[ 0 ].z >= max_height ) ) {
		return false;
	}

	const idVec3 &org = GetPhysics()->GetOrigin() + offset;
	const idMat3 &axis = GetPhysics()->GetAxis();
	idVec3 dir = pos - org;
	float  dist = dir * axis[ 0 ];

	if ( ( dist < min_dist ) || ( dist > max_dist ) ) {
		return false;
	}

	float left_dot = dir * cone_left;
	if ( left_dot < 0.0f ) {
		return false;
	}

	float right_dot = dir * cone_right;
	if ( right_dot < 0.0f ) {
		return false;
	}

	return true;
}

/*
=====================
idCombatNode::Event_Activate
=====================
*/
void idCombatNode::Event_Activate( idEntity *activator ) {
	disabled = !disabled;
}

/*
=====================
idCombatNode::Event_MarkUsed
=====================
*/
void idCombatNode::Event_MarkUsed( void ) {
	if ( spawnArgs.GetBool( "use_once" ) ) {
		disabled = true;
	}
}

// DarkMod sound propagation functions

void idAI::SPLtoLoudness( SSprParms *propParms )
{
	// put in frequency, duration and bandwidth effects here
	propParms->loudness = propParms->propVol;
}

bool idAI::CheckHearing( SSprParms *propParms )
{
	bool returnval(false);

	if( propParms->loudness > m_AudThreshold)
		returnval = true;

	return returnval;
}

void idAI::HearSound
	( SSprParms *propParms, float noise,
	  idVec3 origin )
{
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("AI Hear Sound called\r");
	// TODO:
	// Modify loudness by propVol/noise ratio,
	// looking up a selectivity spawnarg on the AI to
	// see how well the AI distinguishes signal from noise

	float psychLoud;

	//psychLoud = pow(2, (propParms->loudness - threshold)/10 );
	// this scale didn't make much sense for the alerts ingame
	// because the numbers would have been very close together for
	// very different amounts of alert.
	// It is better to keep it in dB.

	// and so alert units are born!



	/**
	* NOTE: an AlertNum of 1 constitutes just barely seeing something
	* out of the corner of your eye, or just barely hearing a whisper
	* of a sound for a short instant.  An AlertNum of 10 is seeing/hearing twice
	* as much, 20 is four times as much, etc.
	**/

	psychLoud = 1 + (propParms->loudness - m_AudThreshold);

	// don't alert the AI if they're deaf, or this is not a strong enough
	//	alert to overwrite another alert this frame
	if (GetAcuity("aud") > 0 && psychLoud > m_AlertNumThisFrame)
	{
		AI_HEARDSOUND = true;
		m_SoundDir = origin;

		if( propParms->maker->IsType(idActor::Type) )
			m_AlertedByActor = static_cast<idActor *>(propParms->maker);
// TODO: For sounds from other entities, such as moveables, query the responsible actor

		AlertAI( "aud", psychLoud );
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI %s HEARD a sound\r", name.c_str() );
		if( cv_ai_debug.GetBool() )
			gameLocal.Printf("AI %s HEARD a sound\n", name.c_str() );
	}
}


void idAI::AlertAI( const char *type, float amount )
{
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("AlertAI called\r");
	float mod(0), alertInc(0);
	idActor *act(NULL);

	mod = GetAcuity( type );
	alertInc = amount * mod/100.0;

	// Ignore actors in notarget mode
	act = m_AlertedByActor.GetEntity();
	if( act && act->fl.notarget )
		goto Quit;

	if( m_AlertGraceTime )
	{
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Grace period active, testing... \r");
		if( gameLocal.time > (m_AlertGraceStart + m_AlertGraceTime) )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Grace period found to have expired. Resetting. \r");
			m_AlertGraceTime = 0;
			m_AlertGraceActor = NULL;
			m_AlertGraceStart = 0;
			m_AlertGraceThresh = 0;
			m_AlertGraceCount = 0;
			m_AlertGraceCountLimit = 0;
		}
		else if( alertInc < m_AlertGraceThresh
			&& act != NULL
			&& act == m_AlertGraceActor.GetEntity()
			&& m_AlertGraceCount < m_AlertGraceCountLimit )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Grace period allowed, ignoring alert. \r");
			m_AlertGraceCount++;
// Quick hack: Large lightgem values and visual alerts override the grace period count faster
			if( AI_VISALERT )
				m_AlertGraceCount += idMath::Rint( g_Global.m_DarkModPlayer->m_LightgemValue / 8.0f );
			goto Quit;
		}
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Alert %f above threshold %f, or actor is not grace period actor\r", alertInc, m_AlertGraceThresh);
	}
	
	Event_SetAlertLevel(AI_AlertNum + alertInc);

	DM_LOG(LC_AI, LT_DEBUG)LOGSTRING( "AI ALERT: AI %s alerted by alert type \"%s\", base amount %f, modified by acuity %f percent.  Total alert level now: %f\r", name.c_str(), type, amount, mod, (float) AI_AlertNum );

	if( cv_ai_debug.GetBool() )
		gameLocal.Printf("[TDM AI] ALERT: AI %s alerted by alert type \"%s\", base amount %f, modified by acuity %f percent.  Total alert level now: %f\n", name.c_str(), type, amount, mod, (float) AI_AlertNum );

	if( gameLocal.isNewFrame )
		AI_ALERTED = true;

	// set the last alert value so that simultaneous alerts only overwrite if they are greater than the value
	m_AlertNumThisFrame = amount;

	// Objectives callback
	gameLocal.m_MissionData->AlertCallback( this, m_AlertedByActor.GetEntity(), (int) AI_AlertIndex );

Quit:
	return;
}

float idAI::GetAcuity( const char *type ) const
{
	float returnval;
	int ind;

	ind = g_Global.m_AcuityHash.First( g_Global.m_AcuityHash.GenerateKey( type, false ) );
//	DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Retrived Acuity index %d for type %s\r", ind, type);

	if (ind == -1 )
	{
		DM_LOG(LC_AI, LT_ERROR)LOGSTRING("AI %s attempted to query nonexistant acuity type: %s", name.c_str(), type);
		gameLocal.Warning("[AI] AI %s attempted to query nonexistant acuity type: %s", name.c_str(), type);
		returnval = -1;
		goto Quit;
	}
	else if( ind > m_Acuities.Num() )
		DM_LOG(LC_AI, LT_ERROR)LOGSTRING("Acuity index %d exceed acuity array size %d!\r", ind, m_Acuities.Num());

	returnval = m_Acuities[ind];

	// SZ: June 10, 2007
	// Accuities are now modified by alert level
	if (returnval > 0.0)
	{
			float thresh_1 = spawnArgs.GetFloat ("alert_thresh1");
			float thresh_2 = spawnArgs.GetFloat ("alert_thresh2");
			float thresh_3 = spawnArgs.GetFloat ("alert_thresh3");

			if (AI_AlertNum >= thresh_3)
			{
				returnval *= cv_ai_acuity_L3.GetFloat();
			}
			else if (AI_AlertNum >= thresh_2)
			{
				returnval *= cv_ai_acuity_L2.GetFloat();
			}
			else if (AI_AlertNum >= thresh_1)
			{
				returnval *= cv_ai_acuity_L1.GetFloat();
			}
	}

	//DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Acuity %s = %f\r", type, returnval);

Quit:
	return returnval;
}

void idAI::SetAcuity( const char *type, float acuity )
{
	int ind;

	ind = g_Global.m_AcuityHash.First( g_Global.m_AcuityHash.GenerateKey( type, false ) );

	if (ind == -1)
	{
		DM_LOG(LC_AI, LT_ERROR)LOGSTRING("Script on %s attempted to set nonexistant acuity type: %s\r",name.c_str(), type);
		gameLocal.Warning("[AI] Script on %s attempted to set nonexistant acuity type: %s",name.c_str(), type);
		goto Quit;
	}
	else if( ind > m_Acuities.Num() )
	{
		DM_LOG(LC_AI, LT_ERROR)LOGSTRING("Script on %s attempted to set acuity with an index %d greater than acuities array size %d!\r", ind, m_Acuities.Num() );
		goto Quit;
	}

	m_Acuities[ind] = acuity;

Quit:
	return;
}

idVec3 idAI::GetSndDir( void )
{
	return m_SoundDir;
}

idVec3 idAI::GetVisDir( void )
{
	return m_LastSight;
}

idEntity *idAI::GetTactEnt( void )
{
	return m_TactAlertEnt.GetEntity();
}

idActor *idAI::VisualScan( float timecheck )
{
	float visFrac, randFrac, incAlert(0);
	idActor *actor;

	actor = FindEnemy( true );

	if( !actor )
	{
		goto Quit;
	}

	if( !actor->IsType( idPlayer::Type ) )
	{
		actor = NULL;
		goto Quit;
	}

	visFrac = GetVisibility( actor );

	// uncomment for visibility fraction debugging (spams the log)
	//DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Visibility fraction for %s = %f\r", actor->name.c_str(), visFrac );

	// Do the percentage check
	randFrac = gameLocal.random.RandomFloat( );
	if( randFrac > ( (timecheck / s_VisNormtime * cv_ai_sight_prob.GetFloat()) * visFrac ) )
	{
		//DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Random number check failed: random %f > number %f\r", randFrac, (timecheck / s_VisNormtime) * visFrac );
		actor = NULL;
		goto Quit;
	}

	// set AI_VISALERT and the vector for last sighted position

	//quick fix for blind AI:
	if( GetAcuity("vis") > 0 )
	{
		AI_VISALERT = true;
		m_LastSight = actor->GetPhysics()->GetOrigin();

		incAlert = getPlayerVisualStimulusAmount(actor);

		if( incAlert > m_AlertNumThisFrame )
		{
			m_AlertedByActor = actor;
			AlertAI( "vis", incAlert );
		}
	}

	if ( actor )
	{
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI %s SAW actor %s\r", name.c_str(), actor->name.c_str() );
		if( cv_ai_debug.GetBool() )
			gameLocal.Printf( "[DM AI] AI %s SAW actor %s\n", name.c_str(), actor->name.c_str() );
	}

Quit:
	return actor;
}

float idAI::GetVisibility( idEntity *ent ) const
{
/**
* PSUEDOCODE:
* Factors:
* Lightgem / brightness: Current lightgem number / max number
* Distance: Do a linear increase and then cutoff?
*			or more complicated clamped function?
*
* Movement: Do this later, but get the object's current velocity
* from physics object or just read from the lightgem if this
* will be factored in to the lightgem.
**/
	// NOTE: Returns the probability that the entity will be seen in a half second

	float lgem, safedist, clampdist, clampVal;
	float dist, returnval(0);
	idVec3 delta;

	// for now, only players may have their visibility checked
	if(!ent->IsType( idPlayer::Type ))
		goto Quit;

	lgem = (float) g_Global.m_DarkModPlayer->m_LightgemValue;
	// debug for formula checking
	//DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Current lightgem value = %f\r", lgem );

	clampdist = (lgem -1) * ( cv_ai_sightmindist.GetFloat() / 31 );
	safedist = clampdist + (cv_ai_sightmaxdist.GetFloat() - cv_ai_sightmindist.GetFloat())*(1 - idMath::Cos(lgem * 1/31 * idMath::PI /2));

	// clampVal is normalized to 100% at TimeNorm
	// In other words: Visibility percentage scales linearly with the lightgem
	//clampVal = (lgem-1)/31;

	//NOTE: Linear model didn't work so well ingame, not enough response to increasing
	// brightness.. try an exponential model (between 0 and 1) :
	// the formula for now is: 1/e^1.2 * exp(1.2* lgem / lgem_max )
	// TODO: Make the 1.2 a factor that you can tweak.
	clampVal = 1/3.3201f * idMath::Exp16( 1.2 * (float) (lgem/32));

	delta = GetEyePosition() - ent->GetPhysics()->GetOrigin();
	dist = delta.Length()*s_DOOM_TO_METERS;

	//TODO : Add acuity in to this calculation

	if (dist < clampdist)
		returnval = clampVal;
	else if (dist > safedist)
		returnval = 0;
	else
		returnval = clampVal * (1 - (dist-clampdist)/(safedist-clampdist) );

Quit:
	return returnval;
}

void idAI::TactileAlert( idEntity *entest, float amount )
{
	idActor *RespActor = NULL;

	if ( amount == -1 )
		amount = cv_ai_tactalert.GetFloat();

	if( entest == NULL )
	{
		goto Quit;
	}

	
	if( entest->IsType(idActor::Type) )
		RespActor = (idActor *) entest;
	else
		RespActor = entest->m_SetInMotionByActor.GetEntity();

	if( !RespActor || !gameLocal.m_RelationsManager->IsEnemy( team, RespActor->team ) )
		goto Quit;

	// aesthetic touch: Don't alert when the AI touches the dead player
	if ( entest->IsType(idPlayer::Type)
		&& ( static_cast< idPlayer * >( entest )->health < 0 ) )
	{
		goto Quit;
	}

	// If we got this far, we give the alert
	// NOTE: Latest tactile alert always overrides other alerts
	m_TactAlertEnt = entest;
	m_AlertedByActor = RespActor;
	AlertAI( "tact", amount );

	// Set last visual contact location to this location as that is used in case
	// the target gets away
	m_LastSight = entest->GetPhysics()->GetOrigin();
	if (enemy.GetEntity()== NULL)
	{
		lastVisibleEnemyPos = entest->GetPhysics()->GetOrigin();
	}

	AI_TACTALERT = true;

	if( cv_ai_debug.GetBool() )
	{
		// Note: This can spam the log a lot, so only put it in if cv_ai_debug.GetBool() is true
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI %s FELT entity %s\r", name.c_str(), entest->name.c_str() );
		gameLocal.Printf( "[DM AI] AI %s FELT entity %s\n", name.c_str(), entest->name.c_str() );
	}
Quit:
	return;
}

idActor *idAI::FindEnemy( bool useFOV )
{
	int			i;
	idEntity	*ent;
	idActor		*actor;

	if ( gameLocal.InPlayerPVS( this ) )
	{
		for ( i = 0; i < gameLocal.numClients ; i++ ) {
			ent = gameLocal.entities[ i ];

			if ( !ent || !ent->IsType( idActor::Type ) ) {
				continue;
			}

			actor = static_cast<idActor *>( ent );
			if ( ( actor->health <= 0 ) || !( gameLocal.m_RelationsManager->IsEnemy(team, actor->team) ) )
			{
				continue;
			}

			if ( CanSee( actor, useFOV ) )
			{
				goto Quit;
			}
		}
	}

	actor = NULL;

Quit:
	return actor;
}

/*---------------------------------------------------------------------------------*/

float idAI::getMaximumObservationDistance (idVec3 bottomPoint, idVec3 topPoint, idEntity* p_ignoreEntity) const
{
	float lightQuotient = LAS.queryLightingAlongLine (bottomPoint, topPoint, p_ignoreEntity, true);
	float visualAcuityZeroToOne = GetAcuity("vis")/ 100.0;

	float maxDistanceToObserve = lightQuotient
		* cv_ai_sight_scale.GetFloat()
		* visualAcuityZeroToOne;

	return maxDistanceToObserve;
}

/*---------------------------------------------------------------------------------*/

float idAI::getPlayerVisualStimulusAmount(idEntity* p_playerEntity) const
{
	float alertAmount = 0.0;

	//Quick fix for blind AI:
	if( GetAcuity("vis") > 0 )
	{
//		float visFrac = GetVisibility( p_playerEntity );
//		float lgem = (float) g_Global.m_DarkModPlayer->m_LightgemValue;

		// Convert to alert units ( 0.6931472 = ln(2) )

		// Old method, commented out
		// alertAmount = 4*log( visFrac * lgem ) / 0.6931472;

		float CurAlert = (float) AI_AlertNum;
		// convert current alert from log to linear scale, add, then convert back
		// this might not be as good for performance, but it lets us keep all alerts
		// on the same scale.
		if( CurAlert > 0 )
		{
			CurAlert = idMath::Pow16(2,(CurAlert - 1)/10.0f );
			CurAlert += cv_ai_sight_mag.GetFloat() * 1.0;
			// convert back to linear
			CurAlert = 1 + 10.0f * idMath::Log16(CurAlert) / 0.6931472f;
			alertAmount = CurAlert - AI_AlertNum;

		}
		else
			alertAmount = 1;
	}

	return alertAmount;
}


/*---------------------------------------------------------------------------------*/

bool idAI::IsEntityHiddenByDarkness (idEntity* p_entity) const
{
	// Quick test using LAS at entity origin
	idPhysics* p_physics = p_entity->GetPhysics();
	if (p_physics != NULL)
	{
		// Get alert level thresholds
		float thresh_1;
		float thresh_2;
		float thresh_3;
		spawnArgs.GetFloat("alert_thresh1", "1.0", thresh_1);
		spawnArgs.GetFloat("alert_thresh2", "1.0", thresh_2);
		spawnArgs.GetFloat("alert_thresh3", "1.0", thresh_3);

		// Use lightgem if it is the player
		if (p_entity->IsType(idPlayer::Type ))
		{

			float incAlert = getPlayerVisualStimulusAmount(p_entity);

			// Get base sight threshold
			float sightThreshold = cv_ai_sight_thresh.GetFloat();

			// Draw debug graphic
			if (cv_ai_visdist_show.GetFloat() > 1.0)
			{
				idVec4 markerColor (0.0, 0.0, 0.0, 0.0);

				float percToSeen = 1.0;
				if (sightThreshold > 0)
				{
					if (sightThreshold > incAlert)
					{
						percToSeen = (sightThreshold - incAlert) / sightThreshold;
					}
				}

				// Scale red to green from not perceptable to quickly perceptable
				markerColor.x = idMath::Sin (percToSeen * (idMath::PI / 2.0));
				markerColor.y = idMath::Cos (percToSeen * (idMath::PI / 2.0));

				idVec3 observeFrom = GetEyePosition();

				gameRenderWorld->DebugArrow
				(
					markerColor,
					observeFrom,
					p_physics->GetOrigin(),
					2,
					cv_ai_visdist_show.GetInteger()
				);


			}

			// Very low threshold for visibility
			if( incAlert < sightThreshold)
			{
				// Fully seen
				return true;
			}
			else
			{
				// Not fully seen
				return false;
			}
		}
		else // Not the player
		{
			idBounds entityBounds = p_physics->GetAbsBounds();
			entityBounds.ExpandSelf (0.1f); // A single point doesn't work with ellipse intersection

			idVec3 bottomPoint = entityBounds[0];
			idVec3 topPoint = entityBounds[1];
			bottomPoint += p_physics->GetGravityNormal() * 0.1f; // Tweak to stay out of floors
			topPoint += p_physics->GetGravityNormal() * 0.1f; // Tweak to stay out of floors


			float maxDistanceToObserve = getMaximumObservationDistance
			(
				bottomPoint,
				topPoint,
				p_entity
			);

			// Are we close enough to see it in the current light level?
			idVec3 observeFrom = GetEyePosition();
			idVec3 midPoint = bottomPoint + ((topPoint - bottomPoint) /2.0);
			if ( (observeFrom - midPoint).Length() > maxDistanceToObserve)
			{
				// Draw debug graphic?
				if (cv_ai_visdist_show.GetFloat() > 1.0)
				{
					idVec4 markerColor (1.0, 0.0, 0.0, 0.0);
					idVec4 markerColor2 (1.0, 0.0, 1.0, 0.0);
					idVec3 arrowLength = midPoint - observeFrom;
					arrowLength.Normalize();
					arrowLength *= maxDistanceToObserve;

					// Distance we could see
					gameRenderWorld->DebugArrow
					(
						markerColor,
						observeFrom,
						observeFrom + arrowLength,
						2,
						cv_ai_visdist_show.GetInteger()
					);


					// Gap to where we want to see
					gameRenderWorld->DebugArrow
					(
						markerColor2,
						observeFrom + arrowLength,
						midPoint,
						2,
						cv_ai_visdist_show.GetInteger()
					);


				}

				return true;
			}
			else
			{
				// Draw debug graphic?
				if (cv_ai_visdist_show.GetFloat() > 1.0)
				{
					idVec4 markerColor (0.0, 1.0, 0.0, 0.0);

					// We can see to target
					gameRenderWorld->DebugArrow
					(
						markerColor,
						observeFrom,
						midPoint,
						2,
						cv_ai_visdist_show.GetInteger()
					);

				}
				return false;
			}
		}
	}

	// Not in darkness
	return false;


}

/*---------------------------------------------------------------------------------*/

idActor *idAI::FindNearestEnemy( bool useFOV )
{
	idEntity	*ent;
	idActor		*actor, *playerEnemy;
	idActor		*bestEnemy;
	float		bestDist;
	float		dist;
	idVec3		delta;
	pvsHandle_t pvs;

	pvs = gameLocal.pvs.SetupCurrentPVS( GetPVSAreas(), GetNumPVSAreas() );

	bestDist = idMath::INFINITY;
	bestEnemy = NULL;

	for ( ent = gameLocal.activeEntities.Next(); ent != NULL; ent = ent->activeNode.Next() ) {
		if ( ent->fl.hidden || ent->fl.isDormant || !ent->IsType( idActor::Type ) )
		{
			continue;
		}

		actor = static_cast<idActor *>( ent );
		if ( ( actor->health <= 0 ) || !( ReactionTo( actor ) & ATTACK_ON_SIGHT ) )
		{
			continue;
		}

		if ( !gameLocal.pvs.InCurrentPVS( pvs, actor->GetPVSAreas(), actor->GetNumPVSAreas() ) )
		{
			continue;
		}

		delta = physicsObj.GetOrigin() - actor->GetPhysics()->GetOrigin();
		dist = delta.LengthSqr();
		if ( ( dist < bestDist ) && CanSee( actor, useFOV ) ) {
			bestDist = dist;
			bestEnemy = actor;
		}
	}

	playerEnemy = FindEnemy(false);
	if( !playerEnemy )
		goto Quit;

	delta = physicsObj.GetOrigin() - playerEnemy->GetPhysics()->GetOrigin();
	dist = delta.LengthSqr();

	if( dist < bestDist )
		bestEnemy = playerEnemy;
		bestDist = dist;

Quit:
	gameLocal.pvs.FreeCurrentPVS( pvs );
	return bestEnemy;
}

bool idAI::IsEnemy( idEntity *other )
{
	bool returnval = false;
	if ( other->IsType( idActor::Type ) )
	{
		idActor *actor = static_cast<idActor *>( other );
		returnval = gameLocal.m_RelationsManager->IsEnemy( team, actor->team );
	}
	return returnval;
}

void idAI::HadTactile( idActor *actor )
{
	if( !actor )
		goto Quit;

	if( gameLocal.m_RelationsManager->IsEnemy( team, actor->team ) )
		TactileAlert( actor );
	else
	{
		// TODO: FLAG BLOCKED BY FRIENDLY SO THE SCRIPT CAN DO SOMETHING ABOUT IT
	}

	// alert both AI if they bump into eachother
	if( gameLocal.m_RelationsManager->IsEnemy( actor->team, team )
		&& actor->IsType(idAI::Type) )
	{
		static_cast<idAI *>(actor)->TactileAlert( this );
	}

Quit:
	return;
}

/*
=====================
idAI::GetMovementVolMod
=====================
*/

float idAI::GetMovementVolMod( void )
{
	float returnval;
	bool bCrouched(false);

	if( AI_CROUCH )
		bCrouched = true;

	// figure out which of the 6 cases we have:
	if( !AI_RUN && !AI_CREEP )
	{
		if( !bCrouched )
			returnval = m_stepvol_walk;
		else
			returnval = m_stepvol_crouch_walk;
	}

	// NOTE: running always has priority over creeping
	else if( AI_RUN )
	{
		if( !bCrouched )
			returnval = m_stepvol_run;
		else
			returnval = m_stepvol_crouch_run;
	}

	else if( AI_CREEP )
	{
		if( !bCrouched )
			returnval = m_stepvol_creep;
		else
			returnval = m_stepvol_crouch_creep;
	}

	else
	{
		// something unexpected happened
		returnval = 0;
	}

	return returnval;
}

/*
=====================
idAI::CheckTactile

Modified 5/25/06 , removed trace computation, found better way of checking
=====================
*/

// TODO OPTIMIZATION: Do not check for touching if the AI is already engaged in combat!
void idAI::CheckTactile( void )
{
	if( !AI_KNOCKEDOUT && !AI_DEAD )
	{
		idEntity *BlockEnt = physicsObj.GetSlideMoveEntity();
		if ( BlockEnt && BlockEnt->IsType( idActor::Type ) )
		{
			DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("TACT: AI %s is bumping actor %s.\r", name.c_str(), BlockEnt->name.c_str() );
			HadTactile( static_cast<idActor *>(BlockEnt) );
		}
	}
	return;
}

/**
* ========================== BEGIN  TDM KNOCKOUT CODE =============================
**/

/*
=====================
idAI::TestKnockoutBlow
=====================
*/

bool idAI::TestKnockoutBlow( idVec3 dir, trace_t *tr, bool bIsPowerBlow )
{
	bool bReturnVal(false);
	float KOAng(0), MinDot(1);
	idVec3 KOSpot(vec3_zero), delta(vec3_zero);
	idMat3 HeadAxis(mat3_zero);
	idStr LocationName;

	// state name for logging
	const char *statename;
	if( state )
		statename = state->Name();
	else
		statename = "NULL State";

	DM_LOG(LC_AI, LT_DEBUG).LogString("Attempted KO of AI %s in state %s\r", name.c_str(), statename);

	if( AI_KNOCKEDOUT )
	{
		AI_PAIN = true;
		AI_DAMAGE = true;

		goto Quit;
	}

	LocationName = GetDamageGroup( CLIPMODEL_ID_TO_JOINT_HANDLE(tr->c.id) );

	DM_LOG(LC_AI, LT_DEBUG).LogString("AI %s hit with KO object in joint %d corresponding to damage group %s\r", name.c_str(), CLIPMODEL_ID_TO_JOINT_HANDLE(tr->c.id), LocationName.c_str());

	// check if we're hitting the right zone (usually the head)
	if( strcmp(LocationName.c_str(), spawnArgs.GetString("ko_zone")) != 0 )
		goto Quit;

	// Check if the AI is above the alert threshold for KOing
	// Defined the name of the alert threshold in the AI def for generality
	if( AI_AlertNum > spawnArgs.GetFloat( va("alert_thresh%s", spawnArgs.GetString("ko_alert_state")) ) )
	{
		// abort KO if the AI is immune when alerted
		if( spawnArgs.GetBool("ko_alert_immmune") )
			goto Quit;

		// reduce the angle on alert, if needed
		const char *temp = spawnArgs.GetString("ko_angle_alert");
		if( temp[0] != '\0' )
			KOAng = atof( temp );
		else
			KOAng = spawnArgs.GetFloat( "ko_angle" );
	}
	else
		KOAng = spawnArgs.GetFloat( "ko_angle" );

	DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Calculated KO angle = %f\r", KOAng);

	// check if we hit within the cone
	MinDot = (float)cos( DEG2RAD( KOAng * 0.5f ) );

	if( m_HeadJointID == INVALID_JOINT )
	{
		DM_LOG(LC_AI, LT_ERROR)LOGSTRING("Invalid head joint for joint found on AI %s when KO attempted \r", name.c_str());
		goto Quit;
	}

	// store head joint base position to KOSpot, axis to HeadAxis
	GetJointWorldTransform( m_HeadJointID, gameLocal.time, KOSpot, HeadAxis );

	KOSpot += HeadAxis * m_KoOffset;

	delta = KOSpot - tr->c.point;
	delta.NormalizeFast();

	// check if hit was within the cone
	if( (delta * HeadAxis[0]) < MinDot )
		goto Quit;

	// if we made it to this point, this AI just got knocked the taff out!
	Knockout();
	bReturnVal = true;

	DM_LOG(LC_AI, LT_DEBUG).LogString("AI %s was knocked out by a blow to the head\r", name.c_str());

Quit:
	return bReturnVal;
}

void idAI::KnockoutDebugDraw( void )
{
	float KOAng(0), radius(0);
	idVec3 KOSpot(vec3_zero), ConeDir(vec3_zero);
	idMat3 HeadAxis(mat3_zero);

	const char * testZone = spawnArgs.GetString("ko_zone");
	if( AI_KNOCKEDOUT || AI_DEAD || testZone[0] == '\0' )
	{
		goto Quit;
	}

	// Check if the AI is above the alert threshold for KOing
	// Defined the name of the alert threshold in the AI def for generality
	if( AI_AlertNum > spawnArgs.GetFloat( va("alert_thresh%s", spawnArgs.GetString("ko_alert_state")) ) )
	{
		// Do not display if immune
		if( spawnArgs.GetBool("ko_alert_immmune") )
			goto Quit;

		// reduce the angle on alert, if needed
		const char *temp = spawnArgs.GetString("ko_angle_alert");
		if( temp[0] != '\0' )
			KOAng = atof( temp );
		else
			KOAng = spawnArgs.GetFloat( "ko_angle" );
	}
	else
		KOAng = spawnArgs.GetFloat( "ko_angle" );

	if( m_HeadJointID == INVALID_JOINT )
	{
		goto Quit;
	}

	// store head joint base position to KOSpot, axis to HeadAxis
	GetJointWorldTransform( m_HeadJointID, gameLocal.time, KOSpot, HeadAxis );

	KOSpot += HeadAxis * m_KoOffset;

	// Assumes the head joint is facing the same way as the look joint
	ConeDir = -HeadAxis[0];
	radius = DEG2RAD( KOAng * 0.5f ) * 30.0f;

	gameRenderWorld->DebugCone( colorGreen, KOSpot, 30.0f * ConeDir, 0, radius, gameLocal.msec );

Quit:
	return;
}

/*
=====================
idAI::Knockout

Based on idAI::Killed
=====================
*/

void idAI::Knockout( void )
{
	idAngles ang;
	const char *modelKOd;

	if( !m_bCanBeKnockedOut )
#ifdef __linux__
		return; // [OrbWeaver] Jumping past an initialisation is UB, GCC will
			    // not compile
#else
		goto Quit;
#endif

	if( AI_KNOCKEDOUT || AI_DEAD )
	{
		AI_PAIN = true;
		AI_DAMAGE = true;

#ifdef __linux__
		return; // [OrbWeaver] Jumping past an initialisation is UB, GCC will
			    // not compile
#else
		goto Quit;
#endif

	}
	EndAttack();

	// stop all voice sounds
	StopSound( SND_CHANNEL_VOICE, false );
	if ( head.GetEntity() )
	{
		head.GetEntity()->StopSound( SND_CHANNEL_VOICE, false );
		head.GetEntity()->GetAnimator()->ClearAllAnims( gameLocal.time, 100 );
	}

	disableGravity = false;
	move.moveType = MOVETYPE_DEAD;
	m_bAFPushMoveables = false;

	physicsObj.UseFlyMove( false );
	physicsObj.ForceDeltaMove( false );

	// end our looping ambient sound
	StopSound( SND_CHANNEL_AMBIENT, false );

	RemoveAttachments();
	RemoveProjectile();
	StopMove( MOVE_STATUS_DONE );

	ClearEnemy();

	AI_KNOCKEDOUT = true;

	// make original self nonsolid
	physicsObj.SetContents( 0 );
	physicsObj.GetClipModel()->Unlink();

	Unbind();

	if ( StartRagdoll() )
	{
		StartSound( "snd_knockout", SND_CHANNEL_VOICE, 0, false, NULL );
	}

	if ( spawnArgs.GetString( "model_knockedout", "", &modelKOd ) )
	{
		// lost soul is only case that does not use a ragdoll and has a model_death so get the death sound in here
		StartSound( "snd_death", SND_CHANNEL_VOICE, 0, false, NULL );
		renderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );
		SetModel( modelKOd );
		physicsObj.SetLinearVelocity( vec3_zero );
		physicsObj.PutToRest();
		physicsObj.DisableImpact();
	}

	// AI becomes frobable on KO
	Event_SetFrobable( true );

	restartParticles = false;

	if (m_TaskQueue && m_knockedOutTask.Length())
	{
		m_TaskQueue->Push(m_knockedOutTaskPriority, m_knockedOutTask.c_str());
	}
	else
	{
		state = GetScriptFunction( "state_KnockedOut" );
		SetState( state );
		SetWaitState( "" );
	}

	// drop items
	DropOnRagdoll();

	// Update TDM objective system
	// TODO: Need a way to determine if player was responsible for the KO
	gameLocal.m_MissionData->MissionEvent( COMP_KO, this, true );

Quit:
	return;
}

/**
* ========================== END TDM KNOCKOUT CODE =============================
**/

/*
=====================
idAI::FoundBody
=====================
*/
void idAI::FoundBody( idEntity *body )
{
	// TODO: Check if the player is responsible for the body
	gameLocal.m_MissionData->MissionEvent( COMP_AI_FIND_BODY, body, true );
}

/*
=====================
idAI::CheckFOV
=====================
*/
bool idAI::CheckFOV( const idVec3 &pos ) const
{
	//DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("idAI::CheckFOV called \r");

	if ( fovDot == 1.0f )
	{
		return true;
	}

	float	dot;
	idVec3	delta, HeadCenter;
	idMat3	HeadAxis;

	// ugliness
	const_cast<idAI *>(this)->GetJointWorldTransform( m_HeadJointID, gameLocal.time, HeadCenter, HeadAxis );

	//GetJointWorldTransform just gives the head attachment joint coordinate.
	//Offset this by the head center offset (same as KO offset) to get the real head center point
	// TODO: Rename m_KoOffset to m_HeadCenterOffset?
	HeadCenter += HeadAxis * m_KoOffset;

	delta = pos - HeadCenter;
	delta.Normalize();

	dot = HeadAxis[ 0 ] * delta;

	return ( dot >= fovDot );
}

void idAI::FOVDebugDraw( void )
{
	float FOVAng(0), radius(0);
	idVec3 HeadCenter(vec3_zero), ConeDir(vec3_zero);
	idMat3 HeadAxis(mat3_zero);

	if( AI_KNOCKEDOUT || AI_DEAD || m_HeadJointID == INVALID_JOINT )
	{
		goto Quit;
	}

	// probably expensive, but that's okay since this is just for debug mode

	FOVAng = idMath::ACos( fovDot );

	// store head joint base position to HeadCenter, axis to HeadAxis
	GetJointWorldTransform( m_HeadJointID, gameLocal.time, HeadCenter, HeadAxis );

	// offset from head joint position to get the true head center
	HeadCenter += HeadAxis * m_KoOffset;

	ConeDir = HeadAxis[0];

	// Diverge to keep reasonable cone size
	float coneLength;

	if (FOVAng >= (idMath::PI / 4.0f))
	{
		// Fix radius and calculate length
		radius = 60.0f;

		coneLength = radius / idMath::Tan(FOVAng);

	}
	else
	{
		// Fix length and calculate radius
		coneLength = 60.0f;

		// SZ: FOVAng is divergence off to one side (idActor::setFOV uses COS(fov/2.0) to calculate fovDot)
		radius = idMath::Tan(FOVAng) * coneLength;
	}


	gameRenderWorld->DebugCone( colorRed, HeadCenter, coneLength * ConeDir, 0, radius, gameLocal.msec );

Quit:
	return;
}

bool idAI::MouthIsUnderwater( void )
{
	bool bReturnVal( false );
	idVec3 MouthOffset, MouthPosition;

	idEntity *headEnt = head.GetEntity();

	// def file will store the coordinates of the mouth relative to the head origin
	// this will be entered by the modeler

	// check for attached head
	if( headEnt )
	{
		MouthPosition = headEnt->GetPhysics()->GetOrigin();

		// add in the mouth offset oriented by head axis
		MouthPosition += headEnt->GetPhysics()->GetAxis() * m_MouthOffset;
	}
	else if( af.IsLoaded() && AI_KNOCKEDOUT )
	{
		MouthPosition = af.GetPhysics()->GetOrigin( m_HeadBodyID );

		// add in the mouth offset oriented by head axis
		MouthPosition += af.GetPhysics()->GetAxis( m_HeadBodyID ) * m_MouthOffset;
	}
	else
		MouthPosition = GetEyePosition();

	// check if the mouth position is underwater

	int contents = gameLocal.clip.Contents( MouthPosition, NULL, mat3_identity, -1, this );

	bReturnVal = (contents & MASK_WATER) > 0;

	return bReturnVal;
}

void idAI::UpdateAir( void )
{
	if( MouthIsUnderwater() )
	{
		// don't let KO'd AI hold their breath
		if( AI_KNOCKEDOUT )
			m_AirTics = 0;

		m_AirTics--;
	}
	else
	{
		// regain breath twice as fast as losing
		m_AirTics += 2;

		if( m_AirTics > m_AirTicksMax )
			m_AirTics = m_AirTicksMax;
	}


	if( m_AirTics < 0 )
	{
		m_AirTics = 0;

		// do the damage, damage_noair is already defined for the player
		Damage( NULL, NULL, vec3_origin, "damage_noair", 1.0f, 0 );
	}

	// set the timer
	m_AirCheckTimer += m_AirCheckInterval;
}

int	idAI::getAirTicks() const {
	return m_AirTics;
}

void idAI::setAirTicks(int airTicks) {
	m_AirTics = airTicks;
	// Clamp to maximum value
	if( m_AirTics > m_AirTicksMax ) {
		m_AirTics = m_AirTicksMax;
	}
}

/*
===================== Lipsync =====================
*/
void idAI::Event_PlayAndLipSync( const char *soundName, const char *animName )
{
	const idSoundShader *shader;
	const char *sound;

	// Play sound
	int duration;
	StartSound( soundName, SND_CHANNEL_VOICE, 0, false, &duration );

	// Do we want to lipsync this sound?
	StopLipSync(); // Assume not
	if (spawnArgs.GetString( soundName, "", &sound ))
	{
		shader = declManager->FindSound( sound );
		if (shader && !strstr(shader->GetDescription(), "nolipsync"))
		{
			// The sound exists and isn't marked "nolipsync", so start the lipsync
			
			// Get the default animation name if necessary
			if (animName==NULL || animName[0]=='\0')
			{
				// Not specified; get the default from a spawnarg.
				// If even the spawnarg doesn't exist, revert to talk.
				animName = spawnArgs.GetString("lipsync_anim_name","talk");
			}
			
			m_lipSyncActive = true;
			m_lipSyncAnim = GetAnim( ANIMCHANNEL_HEAD, animName );
			m_lipSyncEndTimer = gameLocal.time + duration;
			headAnim.CycleAnim( m_lipSyncAnim );
		}
	}
	idThread::ReturnInt(MS2SEC(duration));
}

void idAI::StopLipSync()
{
	if (m_lipSyncActive)
	{
		// Make sure mouth is closed
		headAnim.SetFrame( m_lipSyncAnim, 0 );
		// Halt animation
		headAnim.StopAnim(1);
	}
	m_lipSyncActive = false;
}

/*
===================== Sheathing/drawing weapons =====================
*/

void idAI::DrawWeapon() 
{
	const function_t* func = scriptObject.GetFunction("DrawWeapon");
	if (func) {
		idThread* thread = new idThread(func);
		thread->CallFunction(this, func, true);
		thread->DelayedStart(0);
	}
}

void idAI::SheathWeapon() 
{
	const function_t* func = scriptObject.GetFunction("SheathWeapon");
	if (func) {
		idThread* thread = new idThread(func);
		thread->CallFunction(this, func, true);
		thread->DelayedStart(0);
	}
}

void idAI::DropOnRagdoll( void )
{
	idEntity *ent = NULL;
	bool bDrop(false), bDropWhenDrawn(false), bSetSolid(false), bSetCorpse(false), bSetFrob(false);
	int mask(0);
	// Id style def_drops
	const idKeyValue *kv = spawnArgs.MatchPrefix( "def_drops", NULL );
	
	while( kv )
	{
		idDict args;

		args.Set( "classname", kv->GetValue() );
		args.Set( "origin", physicsObj.GetOrigin().ToString() );
		gameLocal.SpawnEntityDef( args );
		kv = spawnArgs.MatchPrefix( "def_drops", kv );
	}

	// Drop TDM style attachments
	for( int i=0; i<m_attachments.Num(); i++ )
	{
		ent = m_attachments[i].ent.GetEntity();
		if( !ent || !m_attachments[i].ent.IsValid() )
			continue;

		bDrop = ent->spawnArgs.GetBool( "drop_when_ragdoll" );
		bDropWhenDrawn = ent->spawnArgs.GetBool( "drop_when_drawn" );
		bSetSolid = ent->spawnArgs.GetBool( "drop_add_contents_solid" );
		bSetCorpse = ent->spawnArgs.GetBool( "drop_add_contents_corpse" );
		bSetFrob = ent->spawnArgs.GetBool( "drop_set_frobable" );


		if( !bDrop )
			continue;

		if( bDropWhenDrawn )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Testing drop weapon %s\r", ent->name.c_str() );
			bool bIsMelee(false), bIsRanged(false);

			bIsMelee = ent->spawnArgs.GetBool( "is_weapon_melee" );
			if( bIsMelee && !AI_bMeleeWeapDrawn )
			{
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Melee weapon was not drawn\r" );
				continue;
			}

			bIsRanged = ent->spawnArgs.GetBool( "is_weapon_ranged" );
			if( bIsRanged && !AI_bRangedWeapDrawn )
				continue;
		}

		// Proceed with droppage
		DropAttachment( i );

		if( bSetSolid )
			mask = CONTENTS_SOLID;
		if( bSetCorpse )
			mask = mask | CONTENTS_CORPSE;

		if( mask )
			ent->GetPhysics()->SetContents( ent->GetPhysics()->GetContents() | mask );

		if( bSetFrob )
			ent->m_bFrobable = true;

		ent->GetPhysics()->Activate();
	}
}
