/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#pragma hdrstop

#include "../game/game_local.h"

static bool init_version = FileVersionList("$Id$", init_version);

#pragma warning(disable : 4996)

#include "MissionData.h"
#include "../game/player.h"
#include "StimResponse/StimResponseCollection.h"

/**
* Add new component type names here.  Must be in exact same order as EComponentType
*	enum, defined in MissionData.h
**/
static const char *gCompTypeName[COMP_COUNT] =
{
	"kill",
	"ko",
	"ai_find_item",
	"ai_find_body",
	"alert",
	"item",
	"location",
	"custom",
	"custom_clocked",
	"info_location",
	"distance"
};

/**
* Add in new specification types here.  Must be in exact same order as
*	ESpecificationMethod enum, defined in MissionData.h
**/
static const char *gSpecTypeName[SPEC_COUNT] =
{
	"none",
	"name",
	"overall",
	"group",
	"classname",
	"spawnclass",
	"ai_type",
	"ai_team",
	"ai_innocence"
};

// TODO: Move to config file or player spawnargs
const int s_FAILURE_FADE_TIME = 3000;

CObjectiveComponent::CObjectiveComponent( void )
{
	m_bNotted = false;
	m_bState = false;
	m_bReversible = true;
	m_bLatched = false;
	m_Type = COMP_ITEM;
	m_SpecMethod[0] = SPEC_NONE;
	m_SpecMethod[1] = SPEC_NONE;
	m_SpecIntVal[0] = 0;
	m_SpecIntVal[1] = 0;
	m_IntArgs.Clear();
	m_StrArgs.Clear();

	m_ClockInterval = 1000;
	m_TimeStamp = 0;

	m_Index[0] = 0;
	m_Index[1] = 0;
}

CObjectiveComponent::~CObjectiveComponent( void )
{
	m_SpecStrVal[0].Clear();
	m_SpecStrVal[1].Clear();

	m_IntArgs.Clear();
	m_StrArgs.Clear();
}

bool CObjectiveComponent::SetState( bool bState )
{
	bool bReturnVal(false);

	if( m_bNotted )
		bState = !bState;

	if(bState != m_bState)
	{
		// state has changed, check for latching to see if it can change
		if( !m_bReversible )
		{
			if( !m_bLatched )
			{
				m_bLatched = true;
				m_bState = bState;
				bReturnVal = true;
			}
		}
		else
		{
			m_bState = bState;
			bReturnVal = true;
		}
	}

	return bReturnVal;
}

void CObjectiveComponent::Save( idSaveGame *savefile ) const
{
	savefile->WriteInt( m_Index[0] );
	savefile->WriteInt( m_Index[1] );
	savefile->WriteBool( m_bNotted );
	savefile->WriteInt( m_Type );
	savefile->WriteInt( m_SpecMethod[0] );
	savefile->WriteInt( m_SpecMethod[1] );
	savefile->WriteString( m_SpecStrVal[0] );
	savefile->WriteString( m_SpecStrVal[1] );
	savefile->WriteInt( m_SpecIntVal[0] );
	savefile->WriteInt( m_SpecIntVal[1] );
	savefile->WriteBool( m_bState );
	savefile->WriteBool( m_bLatched );

	savefile->WriteInt( m_IntArgs.Num() );
	for( int i=0; i<m_IntArgs.Num(); i++ )
		savefile->WriteInt( m_IntArgs[i] );

	savefile->WriteInt( m_StrArgs.Num() );
	for( int j=0; j<m_StrArgs.Num(); j++ )
		savefile->WriteString( m_StrArgs[j] );

	savefile->WriteInt( m_ClockInterval );
	savefile->WriteInt( m_TimeStamp );
	savefile->WriteBool( m_bReversible );
}

void CObjectiveComponent::Restore( idRestoreGame *savefile )
{
	int num(0), tempInt(0);

	savefile->ReadInt( m_Index[0] );
	savefile->ReadInt( m_Index[1] );
	savefile->ReadBool( m_bNotted );
	savefile->ReadInt( tempInt );
	m_Type = (EComponentType) tempInt;
	savefile->ReadInt( tempInt );
	m_SpecMethod[0] = (ESpecificationMethod) tempInt;
	savefile->ReadInt( tempInt );
	m_SpecMethod[1] = (ESpecificationMethod) tempInt;
	savefile->ReadString( m_SpecStrVal[0] );
	savefile->ReadString( m_SpecStrVal[1] );
	savefile->ReadInt( m_SpecIntVal[0] );
	savefile->ReadInt( m_SpecIntVal[1] );
	savefile->ReadBool( m_bState );
	savefile->ReadBool( m_bLatched );

	savefile->ReadInt( num );
	m_IntArgs.SetNum( num );
	for( int i=0; i<num; i++ )
		savefile->ReadInt( m_IntArgs[i] );

	savefile->ReadInt( num );
	m_StrArgs.SetNum( num );
	for( int j=0; j<num; j++ )
		savefile->ReadString( m_StrArgs[j] );

	savefile->ReadInt( m_ClockInterval );
	savefile->ReadInt( m_TimeStamp );
	savefile->ReadBool( m_bReversible );
}

CMissionData::CMissionData( void )
{
	int i;

	Clear();

// Initialize Hash indexes used for parsing string names to enum index
	idStrList CompTypeNames, SpecTypeNames;

	for(i = 0; i < COMP_COUNT; i++)
		CompTypeNames.Append(gCompTypeName[i]);

	for(i = 0; i < COMP_COUNT; i++)
		SpecTypeNames.Append(gSpecTypeName[i]);

	CompTypeNames.Condense();
	SpecTypeNames.Condense();

	for( int i=0; i < CompTypeNames.Num(); i++ )
	{
		m_CompTypeHash.Add( m_CompTypeHash.GenerateKey( CompTypeNames[i].c_str(), false ), i );
	}
	for( int i=0; i < SpecTypeNames.Num(); i++ )
	{
		m_SpecTypeHash.Add( m_SpecTypeHash.GenerateKey( SpecTypeNames[i].c_str(), false ), i );
	}
}

CMissionData::~CMissionData( void )
{
	Clear();
}

void CMissionData::Clear( void )
{
	m_bObjsNeedUpdate = false;
	m_Objectives.Clear();
	m_ClockedComponents.Clear();

	// Clear all the stats (this is kind've ugly)
	// create a cleared stat to copy to all the SStat fields
	SStat ClearedSt;
	ClearedSt.ByInnocence[0] = 0;
	ClearedSt.ByInnocence[1] = 0;
	ClearedSt.WhileAirborne = 0;
	for( int n1=0; n1<MAX_TEAMS; n1++ )
	{
		ClearedSt.ByTeam[n1] = 0;
	}
	for( int n2=0; n2<MAX_TYPES; n2++ )
	{
		ClearedSt.ByType[n2] = 0;
	}
	ClearedSt.Overall = 0;

	for( int n2=0; n2<MAX_AICOMP; n2++ )
	{
		m_Stats.AIStats[n2] = ClearedSt;
	}
	for( int n3=0; n3<MAX_ALERTNUMS; n3++ )
	{
		m_Stats.AIAlerts[n3] = ClearedSt;
	}

	m_Stats.DamageDealt = 0;
	m_Stats.DamageReceived = 0;
	m_Stats.LootOverall = 0;

	m_SuccessLogicStr.Clear();
	m_FailureLogicStr.Clear();

	m_SuccessLogic.Clear();
	m_FailureLogic.Clear();
}

void CMissionData::Save( idSaveGame *savefile ) const
{
	savefile->WriteBool( m_bObjsNeedUpdate );
	
	savefile->WriteInt( m_Objectives.Num() );
	for( int i=0; i < m_Objectives.Num(); i++ )
		m_Objectives[i].Save( savefile );

	// Save mission stats
	for( int j=0; j < MAX_AICOMP; j++ )
	{
		savefile->WriteInt( m_Stats.AIStats[j].Overall );
		savefile->WriteInt( m_Stats.AIStats[j].WhileAirborne );
		for( int k1=0; k1 < MAX_TEAMS; k1++ )
			savefile->WriteInt( m_Stats.AIStats[j].ByTeam[k1] );
		for( int k2=0; k2 < MAX_TEAMS; k2++ )
			savefile->WriteInt( m_Stats.AIStats[j].ByType[k2] );
		savefile->WriteInt( m_Stats.AIStats[j].ByInnocence[0] );
		savefile->WriteInt( m_Stats.AIStats[j].ByInnocence[1] );
	}

	for( int l=0; l < MAX_ALERTNUMS; l++ )
	{
		savefile->WriteInt( m_Stats.AIAlerts[l].Overall );
		savefile->WriteInt( m_Stats.AIAlerts[l].WhileAirborne );
		for( int m1=0; m1 < MAX_TEAMS; m1++ )
			savefile->WriteInt( m_Stats.AIAlerts[l].ByTeam[m1] );
		for( int m2=0; m2 < MAX_TEAMS; m2++ )
			savefile->WriteInt( m_Stats.AIAlerts[l].ByType[m2] );
		savefile->WriteInt( m_Stats.AIAlerts[l].ByInnocence[0] );
		savefile->WriteInt( m_Stats.AIAlerts[l].ByInnocence[1] );
	}

	savefile->WriteInt( m_Stats.DamageDealt );
	savefile->WriteInt( m_Stats.DamageReceived );
	savefile->WriteInt( m_Stats.LootOverall );
}

void CMissionData::Restore( idRestoreGame *savefile )
{
	int num(0);

	savefile->ReadBool( m_bObjsNeedUpdate );
	
	savefile->ReadInt( num );
	m_Objectives.SetNum( num );
	for( int i=0; i < num; i++ )
		m_Objectives[i].Restore( savefile );

	// Rebuild list of clocked components now that we've loaded objectives
	for( int ind = 0; ind < m_Objectives.Num(); ind++ )
	{
		for( int ind2 = 0; ind2 < m_Objectives[ind].m_Components.Num(); ind2++ )
		{
			CObjectiveComponent *pComp = &m_Objectives[ind].m_Components[ind2];
			if( (pComp->m_Type == COMP_CUSTOM_CLOCKED) || (pComp->m_Type == COMP_DISTANCE) || (pComp->m_Type == COMP_INFO_LOCATION) )
				m_ClockedComponents.Append( pComp );
		}
	}

	// Restore mission stats
	for( int j=0; j < MAX_AICOMP; j++ )
	{
		savefile->ReadInt( m_Stats.AIStats[j].Overall );
		savefile->ReadInt( m_Stats.AIStats[j].WhileAirborne );
		for( int k1=0; k1 < MAX_TEAMS; k1++ )
			savefile->ReadInt( m_Stats.AIStats[j].ByTeam[k1] );
		for( int k2=0; k2 < MAX_TEAMS; k2++ )
			savefile->ReadInt( m_Stats.AIStats[j].ByType[k2] );
		savefile->ReadInt( m_Stats.AIStats[j].ByInnocence[0] );
		savefile->ReadInt( m_Stats.AIStats[j].ByInnocence[1] );
	}

	for( int l=0; l < MAX_ALERTNUMS; l++ )
	{
		savefile->ReadInt( m_Stats.AIAlerts[l].Overall );
		savefile->ReadInt( m_Stats.AIAlerts[l].WhileAirborne );
		for( int m1=0; m1 < MAX_TEAMS; m1++ )
			savefile->ReadInt( m_Stats.AIAlerts[l].ByTeam[m1] );
		for( int m2=0; m2 < MAX_TEAMS; m2++ )
			savefile->ReadInt( m_Stats.AIAlerts[l].ByType[m2] );
		savefile->ReadInt( m_Stats.AIAlerts[l].ByInnocence[0] );
		savefile->ReadInt( m_Stats.AIAlerts[l].ByInnocence[1] );
	}	

	savefile->ReadInt( m_Stats.DamageDealt );
	savefile->ReadInt( m_Stats.DamageReceived );
	savefile->ReadInt( m_Stats.LootOverall );
}

void CMissionData::MissionEvent
	(
		EComponentType CompType,
		SObjEntParms *EntDat1,
		SObjEntParms *EntDat2,
		bool bBoolArg
	)
{
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Mission event called \r");
	SStat *pStat(NULL);
	bool bCompState;

	if( !EntDat1 )
	{
		// log error
		goto Quit;
	}

	// Update AI stats, don't add to stats if playerresponsible is false
	// Stas for KOs, kills, body found, item found
	if( ( ( CompType == COMP_KILL && EntDat1->bIsAI ) || CompType == COMP_KO
		|| CompType == COMP_AI_FIND_BODY || CompType == COMP_AI_FIND_ITEM
		|| CompType == COMP_ALERT ) && bBoolArg )
	{
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Determined AI event \r");
		if( CompType == COMP_ALERT )
		{
			if( EntDat1->value > MAX_ALERTNUMS )
			{
				// log error
				goto Quit;
			}
			// index in this array is determined by alert value
			pStat = &m_Stats.AIAlerts[ EntDat1->value ];
		}
		else
			pStat = &m_Stats.AIStats[ CompType ];

		if( CompType > MAX_AICOMP || !pStat)
		{
			DM_LOG(LC_AI,LT_ERROR)LOGSTRING("Objectives: No AI stat found for comptype %d\r", CompType );
			goto Quit;
		}

		// Add to all appropriate stats
		pStat->Overall++;
		pStat->ByTeam[ EntDat1->team ]++;
		pStat->ByType[ EntDat1->type ]++;
		pStat->ByInnocence[ EntDat1->innocence ]++;

		if( EntDat1->bWhileAirborne )
			pStat->WhileAirborne++;

		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Done adding to stats, checking for objectives...\r" );
	}

	for( int i=0; i<m_Objectives.Num(); i++ )
	{
		CObjective *pObj = &m_Objectives[i];

		for( int j=0; j < pObj->m_Components.Num(); j++ )
		{
			CObjectiveComponent *pComp;
			pComp = &pObj->m_Components[j];

			// match component type
			if( pComp->m_Type != CompType )
				continue;
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Matching Component found: %d, %d\r", i+1, j+1 );

			// check if the specifiers match, for first spec and second if it exists
			if( !MatchSpec(pComp, EntDat1, 0) )
				continue;
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: First specification check matched: %d, %d\r", i+1, j+1 );

			if( pComp->m_SpecMethod[1] != SPEC_NONE )
			{
				if( !MatchSpec(pComp, EntDat2, 1) )
					continue;
			}
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Second specification check matched or absent: %d, %d\r", i+1, j+1 );

			bCompState = EvaluateObjective( pComp, EntDat1, EntDat2, bBoolArg );
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objective component evaluation result: %d \r", (int) bCompState );

			// notify the component of the current state. If the state changed,
			// this will return true and we must mark this objective for update.
			if( pComp->SetState( bCompState ) )
			{
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objective %d, Component %d state changed, needs updating", i+1, j+1 );
				pObj->m_bNeedsUpdate = true;
				m_bObjsNeedUpdate = true;
			}
		}
	}
Quit:
	return;
}

void CMissionData::MissionEvent
	(
	EComponentType CompType,
	idEntity *Ent1, idEntity *Ent2,
	bool bBoolArg, bool bWhileAirborne
	)
{
	SObjEntParms data1, data2;

	// at least the first ent must exist
	if(!Ent1)
	{
		// log error
		goto Quit;
	}
	FillParmsData( Ent1, &data1 );
	data1.bWhileAirborne = bWhileAirborne;

	if( !Ent2 )
		MissionEvent( CompType, &data1, NULL, bBoolArg );
	else
	{
		FillParmsData( Ent2, &data2 );
		MissionEvent( CompType, &data1, &data2, bBoolArg );
	}

Quit:
	return;
}

bool	CMissionData::MatchSpec
			(
			CObjectiveComponent *pComp,
			SObjEntParms *EntDat,
			int ind
			)
{
	bool bReturnVal(false);
	ESpecificationMethod SpecMethod;

	// objectives only have two specified ents at max
	if( !pComp || !EntDat || ind > 1 )
		goto Quit;
	SpecMethod = pComp->m_SpecMethod[ ind ];

	switch( SpecMethod )
	{
		case SPEC_NONE:
			bReturnVal = true;
			break;
		case SPEC_NAME:
			bReturnVal = ( pComp->m_SpecStrVal[ind] == EntDat->name );
			break;
		case SPEC_OVERALL:
			bReturnVal = true;
			break;
		case SPEC_GROUP:
			bReturnVal = ( pComp->m_SpecStrVal[ind] == EntDat->group );
			break;
		case SPEC_CLASSNAME:
			bReturnVal = ( pComp->m_SpecStrVal[ind] == EntDat->classname );
			break;
		case SPEC_SPAWNCLASS:
			bReturnVal = ( pComp->m_SpecStrVal[ind] == EntDat->spawnclass );
			break;
		case SPEC_AI_TYPE:
			bReturnVal = ( pComp->m_SpecIntVal[ind] == EntDat->type );
			break;
		case SPEC_AI_TEAM:
			bReturnVal = ( pComp->m_SpecIntVal[ind] == EntDat->team );
			break;
		case SPEC_AI_INNOCENCE:
			bReturnVal = ( pComp->m_SpecIntVal[ind] == EntDat->innocence );
			break;
		default:
			break;
	}

Quit:
	return bReturnVal;
}

bool	CMissionData::EvaluateObjective
			(
			CObjectiveComponent *pComp,
			SObjEntParms *EntDat1,
			SObjEntParms *EntDat2,
			bool bBoolArg
			)
{
	bool bReturnVal(false);
	int value(0), index(0);

	EComponentType CompType = pComp->m_Type;
	ESpecificationMethod SpecMeth = pComp->m_SpecMethod[0];

	// LOCATION : If we get this far with location, the specifiers
	// already match, that means it's already true and no further evaluation is needed
	if( CompType == COMP_LOCATION )
	{
		// Return value is set to whether the item entered or left the location
		bReturnVal = bBoolArg;
		goto Quit;
	}

	// AI COMPONENTS:
	if( ( CompType == COMP_KILL || CompType == COMP_KO
		|| CompType == COMP_AI_FIND_BODY || CompType == COMP_AI_FIND_ITEM
		|| CompType == COMP_ALERT ) )
	{
		int AlertNum = 0;
		if( CompType == COMP_ALERT )
			AlertNum = pComp->m_IntArgs[1];

		if( AlertNum < 0 || AlertNum > MAX_ALERTNUMS )
			goto Quit;

		// name, classname and spawnclass are all one-shot objectives and not counted up (for now)
		if( SpecMeth == SPEC_NONE || SpecMeth == SPEC_NAME || SpecMeth == SPEC_CLASSNAME || SpecMeth == SPEC_SPAWNCLASS )
		{
			bReturnVal = true;
			goto Quit;
		}

		switch(SpecMeth)
		{
			case SPEC_OVERALL:
				value = GetStatOverall( CompType, AlertNum );
				break;
			case SPEC_AI_TYPE:
				index = EntDat1->type;
				value = GetStatByType( CompType, index, AlertNum );
				break;
			case SPEC_AI_TEAM:
				index = EntDat1->team;
				value = GetStatByTeam( CompType, index, AlertNum );
				break;
			case SPEC_AI_INNOCENCE:
				index = EntDat1->innocence;
				value = GetStatByInnocence( CompType, index, AlertNum );
				break;
		}

		bReturnVal = value >= pComp->m_IntArgs[0];
	}

	// ITEMS:
	else if( CompType == COMP_ITEM )
	{
		// name, classname and spawnclass are all one-shot objectives and not counted up (for now)
		if( SpecMeth == SPEC_NONE || SpecMeth == SPEC_NAME || SpecMeth == SPEC_CLASSNAME || SpecMeth == SPEC_SPAWNCLASS )
		{
			// Returnval is set based on whether item is entering or leaving inventory
			bReturnVal = bBoolArg;
			goto Quit;
		}

		switch( SpecMeth )
		{
			// overall loot
			case SPEC_OVERALL:
				value = EntDat1->valueSuperGroup;
				break;
			case SPEC_GROUP:
				value = EntDat1->value;
				break;
			default:
				break;
		}
		bReturnVal = value >= pComp->m_IntArgs[0];
	}

Quit:
	return bReturnVal;
}

void CMissionData::UpdateObjectives( void )
{
	bool bObjEnabled(true);

// =============== Begin Handling of Clocked Objective Components ===============

	for( int k=0; k < m_ClockedComponents.Num(); k++ )
	{
		CObjectiveComponent *pComp = m_ClockedComponents[k];

		// check if timer is due to fire
		if( !pComp  )
			continue;

		// if parent objective is invalid or the timer hasn't fired or it's latched, don't do anything
		if( m_Objectives[ pComp->m_Index[0] ].m_state == STATE_INVALID
			|| (gameLocal.time - pComp->m_TimeStamp < pComp->m_ClockInterval)
			|| pComp->m_bLatched )
		{
			continue;
		}

// COMP_DISTANCE - Do a distance check
		else if( pComp->m_Type == COMP_DISTANCE )
		{
			pComp->m_TimeStamp = gameLocal.time;

			idEntity *ent1, *ent2;
			idVec3 delta;
			int dist(0);

			ent1 = gameLocal.FindEntity( pComp->m_StrArgs[0].c_str() );
			ent2 = gameLocal.FindEntity( pComp->m_StrArgs[1].c_str() );

			if( !ent1 || !ent2 )
			{
				DM_LOG(LC_AI, LT_WARNING)LOGSTRING("Objective %d, component %d: Distance objective component given bad entity names %s , %s \r", pComp->m_Index[0], pComp->m_Index[1], pComp->m_StrArgs[0].c_str(), pComp->m_StrArgs[1].c_str() );
				continue;
			}

			delta = ent1->GetPhysics()->GetOrigin();
			delta = delta - ent2->GetPhysics()->GetOrigin();

			dist = pComp->m_IntArgs[0];
			dist *= dist;

			SetComponentState( pComp, ( delta.LengthSqr() < dist ) );
		}

// COMP_INFO_LOCATION - Check if an ent by name is in an info_location or info_location group
		else if( pComp->m_Type == COMP_INFO_LOCATION )
		{
			bool bEval(false);
			idEntity *checkEnt = NULL;

			// Spec method 0 is always by name, so no need to check it.
			// we should indicate this in the objective setup GUI
			checkEnt = gameLocal.FindEntity( pComp->m_SpecStrVal[0].c_str() );
			if( !checkEnt )
			{
				DM_LOG(LC_AI, LT_WARNING)LOGSTRING("Objective %d, component %d: Info_location objective could not find entity: %s \r", pComp->m_Index[0], pComp->m_Index[1], pComp->m_SpecStrVal[0].c_str() );
				continue;
			}

			idLocationEntity *loc = gameLocal.LocationForPoint( checkEnt->GetPhysics()->GetOrigin() );
			if( loc )
			{
				if( pComp->m_SpecMethod[1] == SPEC_GROUP )
					bEval = (pComp->m_SpecIntVal[1] == loc->m_ObjectiveGroup );
				else
					bEval = ( pComp->m_SpecStrVal[1] == loc->name );
			}

			SetComponentState( pComp, bEval );
		}

// COMP_CUSTOM_CLOCKED - Run a clocked script
		else if( pComp->m_Type == COMP_CUSTOM_CLOCKED )
		{
			pComp->m_TimeStamp = gameLocal.time;

			function_t *pScriptFun = gameLocal.program.FindFunction( pComp->m_StrArgs[0].c_str() );

			if(pScriptFun)
			{
				idThread *pThread = new idThread( pScriptFun );
				pThread->CallFunction( pScriptFun, true );
				pThread->DelayedStart( 0 );
			}
			else
			{
				DM_LOG(LC_AI, LT_WARNING)LOGSTRING("Objective %d, component %d: Custom clocked objective called bad script: %s \r", pComp->m_Index[0], pComp->m_Index[1], pComp->m_StrArgs[0].c_str() );
				gameLocal.Printf("WARNING: Objective %d, component %d: Custom clocked objective called bad script: %s \n", pComp->m_Index[0], pComp->m_Index[1], pComp->m_StrArgs[0].c_str() );
			}
		}
	}

// ============== End Handling of Clocked Objective Components =============

	// Check if any objective states have changed:
	if( !m_bObjsNeedUpdate )
		goto Quit;
	m_bObjsNeedUpdate = false;

	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Objectives in need of updating \r");

	for( int i=0; i<m_Objectives.Num(); i++ )
	{
		CObjective *pObj = &m_Objectives[i];

		// skip objectives that don't need updating
		if( !pObj->m_bNeedsUpdate || pObj->m_state == STATE_INVALID )
			continue;
		pObj->m_bNeedsUpdate = false;
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Found objective in need of update: %d \r", i+1);

		// If objective was just completed
		if( pObj->CheckSuccess() )
		{
			// Check for enabling objectives
			for( int k=0; k < pObj->m_EnablingObjs.Num(); k++ )
			{
				int ObjNum = pObj->m_EnablingObjs[k] - 1;
				if( ObjNum >= m_Objectives.Num() || ObjNum < 0 )
					continue;

				EObjCompletionState CompState = m_Objectives[ObjNum].m_state;

				bObjEnabled = bObjEnabled && (CompState == STATE_COMPLETE || CompState == STATE_INVALID);
			}
			if( !bObjEnabled )
				goto Quit;

			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Objective %d COMPLETED\r", i+1);
			SetCompletionState( i, STATE_COMPLETE );
		}
		else if( pObj->CheckFailure() )
		{
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: Objective %d FAILED\r", i+1);
				SetCompletionState(i, STATE_FAILED );
		}
		else
		{
			pObj->m_state = STATE_INCOMPLETE;
		}
	}

Quit:
	return;
}

void CMissionData::Event_ObjectiveComplete( int ind )
{
	bool bTest(true), bTemp(false);

	// Ongoing objectives don't play the sound or mark off in the GUI as complete during mission
	if( !m_Objectives[ind].m_bOngoing )
	{
		idPlayer *   player;
		player = gameLocal.localClientNum >= 0 ? static_cast<idPlayer *>( gameLocal.entities[ gameLocal.localClientNum ] ) : NULL;

		player->StartSound("snd_objective_complete", SND_CHANNEL_ANY, 0, false, NULL);

		// call completion script
		function_t *pScriptFun = gameLocal.program.FindFunction( m_Objectives[ind].m_CompletionScript.c_str() );
		if(pScriptFun)
		{
			idThread *pThread = new idThread( pScriptFun );
			pThread->CallFunction( pScriptFun, true );
			pThread->DelayedStart( 0 );
		}

// TODO: Update the GUI to mark the objective as complete

	}

	// check if all mandatory, valid and active objectives have been completed
	// If so, the mission is complete
	for( int i=0; i<m_Objectives.Num(); i++ )
	{
		CObjective *pObj = &m_Objectives[i];
		bTemp = ( pObj->m_state == STATE_COMPLETE || pObj->m_state == STATE_INVALID
					 || !pObj->m_bMandatory );
		bTest = bTest && bTemp;
	}

	if( bTest )
		Event_MissionComplete();
}

void CMissionData::Event_ObjectiveFailed( int ind )
{
	// if the objective was mandatory, fail the mission
	if( m_Objectives[ind].m_bMandatory )
		Event_MissionFailed();
	else
	{
		// play an objective failed sound for optional objectives?

		// call failure script
		function_t *pScriptFun = gameLocal.program.FindFunction( m_Objectives[ind].m_FailureScript.c_str() );
		if(pScriptFun)
		{
			idThread *pThread = new idThread( pScriptFun );
			pThread->CallFunction( pScriptFun, true );
			pThread->DelayedStart( 0 );
		}
	}
}

void CMissionData::Event_MissionComplete( void )
{
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: MISSION COMPLETE. \r");
	gameLocal.Printf("MISSION COMPLETED\n");

	// TODO: Go to mission successful GUI
	// TODO: Read off which map to go to next, basically call endLevel

	// for now, just play the sound (later it will be played in the GUI)
	idPlayer *player = gameLocal.GetLocalPlayer();
	if(player)
	{
		player->StartSoundShader( declManager->FindSound( "mission_complete" ), SND_CHANNEL_ANY, 0, false, NULL );
	}
}

void CMissionData::Event_MissionFailed( void )
{
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objectives: MISSION FAILED. \r");
	gameLocal.Printf("MISSION FAILED\n");

	idPlayer *player = gameLocal.GetLocalPlayer();
	if(player)
	{
		player->playerView.Fade( colorBlack, s_FAILURE_FADE_TIME );
		player->PostEventMS( &EV_Player_DeathMenu, s_FAILURE_FADE_TIME + 1 );
	}
}


// ============================== Stats =================================

int CMissionData::GetStatOverall( EComponentType CompType, int AlertNum )
{
	int returnVal(0);

	if( AlertNum < 0 || AlertNum > MAX_ALERTNUMS )
		goto Quit;

	if( CompType == COMP_ALERT )
		returnVal = m_Stats.AIAlerts[ AlertNum ].Overall;
	else
		returnVal = m_Stats.AIStats[ CompType ].Overall;

Quit:
	return returnVal;
}

int CMissionData::GetStatByTeam( EComponentType CompType, int index, int AlertNum )
{
	int returnVal(0);

	if( AlertNum < 0 || AlertNum > MAX_ALERTNUMS )
		goto Quit;
	if( CompType == COMP_ALERT )
		returnVal = m_Stats.AIAlerts[ AlertNum ].ByTeam[index];
	else
		returnVal = m_Stats.AIStats[ CompType ].ByTeam[index];

Quit:
	return returnVal;
}

int CMissionData::GetStatByType( EComponentType CompType, int index, int AlertNum )
{
	int returnVal(0);

	if( AlertNum < 0 || AlertNum > MAX_ALERTNUMS )
		goto Quit;
	if( CompType == COMP_ALERT )
		returnVal = m_Stats.AIAlerts[ AlertNum ].ByType[index];
	else
		returnVal = m_Stats.AIStats[ CompType ].ByType[index];

Quit:
	return returnVal;
}

int CMissionData::GetStatByInnocence( EComponentType CompType, int index, int AlertNum )
{
	int returnVal(0);

	if( AlertNum < 0 || AlertNum > MAX_ALERTNUMS )
		goto Quit;
	if( CompType == COMP_ALERT )
		returnVal = m_Stats.AIAlerts[ AlertNum ].ByInnocence[index];
	else
		returnVal = m_Stats.AIStats[ CompType ].ByInnocence[index];

Quit:
	return returnVal;
}

int CMissionData::GetStatAirborne( EComponentType CompType, int AlertNum )
{
	int returnVal(0);

	if( AlertNum < 0 || AlertNum > MAX_ALERTNUMS )
		goto Quit;
	if( CompType == COMP_ALERT )
		returnVal = m_Stats.AIAlerts[ AlertNum ].WhileAirborne;
	else
		returnVal = m_Stats.AIStats[ CompType ].WhileAirborne;

Quit:
	return returnVal;
}

void CMissionData::AIDamagedByPlayer( int DamageAmount )
{
	m_Stats.DamageDealt += DamageAmount;
}

void CMissionData::PlayerDamaged( int DamageAmount )
{
	m_Stats.DamageReceived += DamageAmount;
}

int CMissionData::GetDamageDealt( void )
{
	return m_Stats.DamageDealt;
}

int CMissionData::GetDamageReceived( void )
{
	return m_Stats.DamageReceived;
}

// ============================== Misc.  ==============================

void CMissionData::FillParmsData( idEntity *ent, SObjEntParms *parms )
{
	if(!ent || !parms)
		goto Quit;

	parms->name = ent->name;
	parms->group = ent->spawnArgs.GetString("inv_name");
	parms->classname = ent->spawnArgs.GetString("classname");
	parms->spawnclass = ent->spawnArgs.GetString("spawnclass");

	if( ent->IsType(idActor::Type) )
	{
		idActor *actor = static_cast<idActor *>(ent);

		parms->team = actor->team;
		parms->type = actor->m_AItype;
		parms->innocence = (int) actor->m_Innocent;
		parms->bIsAI = true;
	}

Quit:
	return;
}

void CMissionData::SetComponentState_Ext( int ObjIndex, int CompIndex, bool bState )
{
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("SetComponentState: Called for obj %d, comp %d, state %d. \r", ObjIndex, CompIndex, (int) bState );

	// Offset the indices into "internal" values (start at 0)
	ObjIndex--;
	CompIndex--;

	if( ObjIndex >= m_Objectives.Num() || ObjIndex < 0  )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("SetComponentState: Objective num %d out of bounds. \r", (ObjIndex+1) );
		goto Quit;
	}
	if( CompIndex >= m_Objectives[ObjIndex].m_Components.Num() || CompIndex < 0 )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("SetComponentState: Component num %d out of bounds for objective %d. \r", (CompIndex+1), (ObjIndex+1) );
		goto Quit;
	}

	// call internal SetComponentState
	SetComponentState( ObjIndex, CompIndex, bState );

Quit:
	return;
}

void CMissionData::SetComponentState(int ObjIndex, int CompIndex, bool bState)
{
	CObjectiveComponent *pComp(NULL);

	pComp = &m_Objectives[ObjIndex].m_Components[CompIndex];

	if( !pComp )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("SetComponentState: NULL component found \r" );
		goto Quit;
	}

	if( pComp->SetState( bState ) )
	{
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("SetComponentState: Objective %d, Component %d state changed, needs updating", (ObjIndex+1), (CompIndex+1) );
		m_Objectives[ObjIndex].m_bNeedsUpdate = true;
		m_bObjsNeedUpdate = true;
	}

Quit:
	return;
}

void CMissionData::SetComponentState( CObjectiveComponent *pComp, bool bState )
{
	if( !pComp )
		goto Quit;

	SetComponentState( pComp->m_Index[0]-1, pComp->m_Index[1]-1, bState );

Quit:
	return;
}

void CMissionData::SetCompletionState( int ObjIndex, int State )
{
	CObjective *pObj = NULL;

	if( ObjIndex >= m_Objectives.Num() || ObjIndex < 0 )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("Attempt was made to set completion state of invalid objective index: %d \r", ObjIndex );
		gameLocal.Printf("WARNING: Objective system: Attempt was made to set completion state of invalid objective index: %d \n", ObjIndex);
		goto Quit;
	}

	// check if the state int is valid by comparing to highest number in enum
	if( State < 0 || State > STATE_FAILED )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("Attempt was made to set objective index: %d to invalid completion state: %d \r", ObjIndex, State);
		gameLocal.Printf("WARNING: Objective system: Attempt was made to set objective index: %d to invalid completion state: %d \n", ObjIndex, State);
		goto Quit;
	}

	pObj = &m_Objectives[ObjIndex];
	if( !pObj )
	{
		DM_LOG(LC_AI,LT_ERROR)LOGSTRING("SetCompletionState: NULL Objective found for obj %d \r", ObjIndex );
		goto Quit;
	}

	// Don't do anything if we are already in that state
	if( pObj->m_state == State )
		goto Quit;

	// Check for latching:
	if( !pObj->m_bReversible )
	{
		// do not do anything if latched
		if( pObj->m_bLatched )
			goto Quit;

		// Irreversible objectives latch to either complete or failed
		if( State == STATE_COMPLETE || State == STATE_FAILED )
			pObj->m_bLatched = true;
	}


	m_Objectives[ObjIndex].m_state = (EObjCompletionState) State;

	if( State == STATE_COMPLETE )
		Event_ObjectiveComplete( ObjIndex );
	else if( State == STATE_FAILED )
		Event_ObjectiveFailed( ObjIndex );
Quit:
	return;
}

// for scripters:

int CMissionData::GetCompletionState( int ObjIndex )
{
	int returnInt = -1;

	if( ObjIndex >= m_Objectives.Num() || ObjIndex < 0 )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("GetCompletionState: Bad objective index: %d \r", ObjIndex );
		gameLocal.Printf("WARNING: Objective system: Attempt was made to get completion state of invalid objective index: %d \n", ObjIndex);
		goto Quit;
	}

	returnInt = m_Objectives[ObjIndex].m_state;

Quit:
	return returnInt;
}

bool CMissionData::GetComponentState( int ObjIndex, int CompIndex )
{
	bool bReturnVal(false);

	if( ObjIndex >= m_Objectives.Num() || ObjIndex < 0  )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("GetComponentState: Objective num %d out of bounds. \r", (ObjIndex+1) );
		gameLocal.Printf("WARNING: Objective System: GetComponentState: Objective num %d out of bounds. \n", (ObjIndex+1) );
		goto Quit;
	}
	if( CompIndex >= m_Objectives[ObjIndex].m_Components.Num() || CompIndex < 0 )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("GetComponentState: Component num %d out of bounds for objective %d. \r", (CompIndex+1), (ObjIndex+1) );
		gameLocal.Printf("WARNING: Objective System: GetComponentState: Component num %d out of bounds for objective %d. \n", (CompIndex+1), (ObjIndex+1) );
		goto Quit;
	}

	bReturnVal = m_Objectives[ObjIndex].m_Components[CompIndex].m_bState;

Quit:
	return bReturnVal;
}

void CMissionData::UnlatchObjective( int ObjIndex )
{
	if( ObjIndex >= m_Objectives.Num() || ObjIndex < 0 )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("UnlatchObjective: Bad objective index: %d \r", ObjIndex );
		gameLocal.Printf("WARNING: Objective system: Attempt was made to unlatch an invalid objective index: %d \n", ObjIndex);
		goto Quit;
	}

	m_Objectives[ObjIndex].m_bLatched = false;

Quit:
	return;
}

void CMissionData::UnlatchObjectiveComp(int ObjIndex, int CompIndex )
{
	if( ObjIndex >= m_Objectives.Num() || ObjIndex < 0 )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("UnlatchObjective: Bad objective index: %d \r", ObjIndex );
		gameLocal.Printf("WARNING: Objective system: Attempt was made to unlatch a component of invalid objective index: %d \n", ObjIndex);
		goto Quit;
	}

	if( CompIndex >= m_Objectives[ObjIndex].m_Components.Num() || CompIndex < 0 )
	{
		DM_LOG(LC_AI,LT_WARNING)LOGSTRING("UnlatchObjective: Component num %d out of bounds for objective %d. \r", (CompIndex+1), (ObjIndex+1) );
		gameLocal.Printf("WARNING: Objective system: Attempt was made to unlatch invalid component: %d of objective: %d \n", (CompIndex+1), (ObjIndex+1) );
		goto Quit;
	}

	m_Objectives[ObjIndex].m_Components[CompIndex].m_bLatched = false;

Quit:
	return;
}

void CMissionData::Event_SetObjVisible( int ObjIndex, bool bVal )
{
	if( ObjIndex > m_Objectives.Num() || ObjIndex < 0 )
	{
		// log error
		goto Quit;
	}
	m_Objectives[ObjIndex].m_bVisible = bVal;

Quit:
	return;
}

void CMissionData::Event_SetObjMandatory( int ObjIndex, bool bVal )
{
	if( ObjIndex > m_Objectives.Num() || ObjIndex < 0 )
	{
		// log error
		goto Quit;
	}
	m_Objectives[ObjIndex].m_bMandatory = bVal;

Quit:
	return;
}

void CMissionData::Event_SetObjOngoing( int ObjIndex, bool bVal )
{
	if( ObjIndex > m_Objectives.Num() || ObjIndex < 0 )
	{
		// log error
		goto Quit;
	}
	m_Objectives[ObjIndex].m_bOngoing = bVal;

Quit:
	return;
}

void CMissionData::Event_SetObjEnabling(int ObjIndex, idStr StrIn)
{
	idLexer				src;
	idToken				token;
	idList<int>			ObjList;

	if( ObjIndex > m_Objectives.Num() || ObjIndex < 0 )
	{
		// log error
		goto Quit;
	}

	// parse in the int list of "enabling objectives"
	src.LoadMemory( StrIn.c_str(), StrIn.Length(), "" );
	while( src.ReadToken( &token ) )
	{
		if( token.IsNumeric() )
			ObjList.Append( token.GetIntValue() );
	}
	src.FreeSource();

	m_Objectives[ObjIndex].m_EnablingObjs = ObjList;

Quit:
	return;
}

// Objective parsing:
// returns the index of the first objective added, for scripting purposes
int CMissionData::AddObjsFromEnt( idEntity *ent )
{
	CObjective			ObjTemp;
	idLexer				src;
	idToken				token;
	idDict				*args;
	idStr				StrTemp, StrTemp2, TempStr2;
	int					Counter(1), Counter2(1); // objective indices start at 1 and must be offset for the inner code
	int					ReturnVal(-1);

	if( !ent )
		goto Quit;

	args = &ent->spawnArgs;
	if( !args )
		goto Quit;

	// store the first index of first added objective
	ReturnVal = m_Objectives.Num();

	// go thru all the objective-related spawnargs
	while( args->MatchPrefix( va("obj%d_", Counter) ) != NULL )
	{
		ObjTemp.m_Components.Clear();
		ObjTemp.m_ObjNum = Counter - 1;

		StrTemp = va("obj%d_", Counter);
		ObjTemp.m_state = (EObjCompletionState) args->GetInt( StrTemp + "state", "0");
		ObjTemp.m_text = args->GetString( StrTemp + "desc", "" );
		ObjTemp.m_bMandatory = args->GetBool( StrTemp + "mandatory", "1");
		ObjTemp.m_bReversible = !args->GetBool (StrTemp + "irreversible", "0" );
		ObjTemp.m_bVisible = args->GetBool( StrTemp + "visible", "1");
		ObjTemp.m_bOngoing = args->GetBool( StrTemp + "ongoing", "0");
		ObjTemp.m_CompletionScript = args->GetString( StrTemp + "script_complete" );
		ObjTemp.m_FailureScript = args->GetString( StrTemp + "script_failed" );
		ObjTemp.m_SuccessLogicStr = args->GetString( StrTemp + "logic_success", "" );
		ObjTemp.m_FailureLogicStr = args->GetString( StrTemp + "logic_failure", "" );

		// parse in the int list of "enabling objectives"
		TempStr2 = args->GetString( StrTemp + "enabling_objs", "" );
		src.LoadMemory( TempStr2.c_str(), TempStr2.Length(), "" );
		while( src.ReadToken( &token ) )
		{
			if( token.IsNumeric() )
				ObjTemp.m_EnablingObjs.Append( token.GetIntValue() );
		}
		src.FreeSource();

		// Parse difficulty level. If difficulty not specified, then
		// this objective applies to all levels.
		TempStr2 = args->GetString( StrTemp + "difficulty", "" );
		if (TempStr2.Length() > 0) {
			ObjTemp.m_bApplies = false;
			src.LoadMemory( TempStr2.c_str(), TempStr2.Length(), "" );
			while( src.ReadToken( &token ) )
			{
				if( token.IsNumeric() )
					if (g_skill.GetInteger() == token.GetIntValue()) {
						ObjTemp.m_bApplies = true;
						break;
					}
			}
			if (!ObjTemp.m_bApplies) {
				// Objectives that don't apply to this difficulty level are considered invalid.
				// They don't need to be completed.
				ObjTemp.m_state = STATE_INVALID;
			}
			src.FreeSource();
		}

		// parse objective components
		Counter2 = 1;
		while( args->MatchPrefix( va("obj%d_%d_", Counter, Counter2) ) != NULL )
		{
			StrTemp2 = StrTemp + va("%d_", Counter2);
			CObjectiveComponent CompTemp;

			CompTemp.m_bState = args->GetBool( StrTemp2 + "state", "0" );
			CompTemp.m_bNotted = args->GetBool( StrTemp2 + "not", "0" );
			CompTemp.m_bReversible = !args->GetBool( StrTemp2 + "irreversible", "0" );

			// use comp. type hash to convert text type to EComponentType
			idStr TypeString = args->GetString( StrTemp2 + "type", "");
			int TypeNum = m_CompTypeHash.First(m_CompTypeHash.GenerateKey( TypeString, false ));
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Parsing objective component type '%s', typenum %d \r", TypeString.c_str(), TypeNum );

			if( TypeNum == -1 )
			{
				DM_LOG(LC_AI,LT_ERROR)LOGSTRING("Unknown objective component type '%s' when adding objective %d, component %d \r", TypeString, Counter, Counter2 );
				gameLocal.Printf("Objective System Error: Unknown objective component type '%s' when adding objective %d, component %d.  Objective component ignored. \n", TypeString, Counter, Counter2 );
				continue;
			}
			CompTemp.m_Type = (EComponentType) TypeNum;

			for( int ind=0; ind<2; ind++ )
			{
				// Use spec. type hash to convert text specifier to ESpecificationMethod enum
				idStr SpecString = args->GetString(va(StrTemp2 + "spec%d", ind + 1), "none");
				int SpecNum = m_SpecTypeHash.First(m_SpecTypeHash.GenerateKey( SpecString, false ));

				if( SpecNum == -1 )
				{
					DM_LOG(LC_AI,LT_ERROR)LOGSTRING("Unknown objective component specification type '%s' when adding objective %d, component %d \r", TypeString, Counter, Counter2 );
					gameLocal.Printf("Objective System Error: Unknown objective component specification type '%s' when adding objective %d, component %d.  Setting default specifier type 'none' \n", TypeString, Counter, Counter2 );
					SpecNum = 0;
				}
				CompTemp.m_SpecMethod[ind] = (ESpecificationMethod) SpecNum;
			}

			for( int ind=0; ind < 2; ind++ )
			{
				CompTemp.m_SpecStrVal[ind] = args->GetString( va(StrTemp2 + "spec_strval%d", ind + 1), "" );
				CompTemp.m_SpecIntVal[ind] = args->GetInt( va(StrTemp2 + "spec_intval%d", ind + 1), "0" );
			}

			// Use idLexer to read in string args and int args, space delimited lists
			TempStr2 = args->GetString( StrTemp2 + "args_str", "" );
			src.LoadMemory( TempStr2.c_str(), TempStr2.Length(), "" );
			src.SetFlags( LEXFL_NOSTRINGCONCAT | LEXFL_NOFATALERRORS | LEXFL_ALLOWPATHNAMES );

			while( src.ReadToken( &token ) )
				CompTemp.m_StrArgs.Append( token.c_str() );
			src.FreeSource();
			// same for int args:
			TempStr2 = args->GetString( StrTemp2 + "args_int", "" );
			src.LoadMemory( TempStr2.c_str(), TempStr2.Length(), "" );
			while( src.ReadToken( &token ) )
			{
				if( token.IsNumeric() )
					CompTemp.m_IntArgs.Append( token.GetIntValue() );
			}
			src.FreeSource();

			// Pad args with dummies to prevent a hard crash when they are read, if otherwise empty
			CompTemp.m_StrArgs.Append("");
			CompTemp.m_StrArgs.Append("");
			CompTemp.m_IntArgs.Append(0);
			CompTemp.m_IntArgs.Append(0);

			CompTemp.m_ClockInterval = 1000 * int(args->GetFloat( StrTemp2 + "clock_interval", "1.0" ));

			CompTemp.m_Index[0] = Counter;
			CompTemp.m_Index[1] = Counter2;

			ObjTemp.m_Components.Append( CompTemp );
			Counter2++;
		}

		if( ObjTemp.m_Components.Num() > 0 )
		{
			m_Objectives.Append( ObjTemp );
			
			// Parse success/failure logic
			gameLocal.Printf("Objective %d: Parsing success and failure logic\n", Counter);
			m_Objectives[ m_Objectives.Num() - 1 ].ParseLogicStrs();

			ObjTemp.Clear();
		}
		Counter++;
	}

	// Process the objectives and add clocked components to clocked components list
	for( int ind = 0; ind < m_Objectives.Num(); ind++ )
	{
		for( int ind2 = 0; ind2 < m_Objectives[ind].m_Components.Num(); ind2++ )
		{
			CObjectiveComponent *pComp = &m_Objectives[ind].m_Components[ind2];
			if( (pComp->m_Type == COMP_CUSTOM_CLOCKED) || (pComp->m_Type == COMP_DISTANCE) || (pComp->m_Type == COMP_INFO_LOCATION) )
				m_ClockedComponents.Append( pComp );
		}
	}

	// check if any objectives were actually added, if not return -1
	if( m_Objectives.Num() == ReturnVal )
		ReturnVal = -1;
Quit:
	return ReturnVal;
}

/**==========================================================================
* CObjective
*==========================================================================**/

CObjective::CObjective( void )
{
	Clear();
}

CObjective::~CObjective( void )
{
	Clear();
}

void CObjective::Clear( void )
{
	m_ObjNum = -1;
	m_state = STATE_INCOMPLETE;
	m_text = "";
	m_bNeedsUpdate = false;
	m_bMandatory = false;
	m_bReversible = true;
	m_bLatched = false;
	m_bVisible = true;
	m_bOngoing = false;
	m_bApplies = true;
	m_handle = 0;
	m_Components.Clear();
	m_EnablingObjs.Clear();
	m_CompletionScript.Clear();
	m_FailureScript.Clear();
	m_SuccessLogicStr.Clear();
	m_FailureLogicStr.Clear();
	m_SuccessLogic.Clear();
	m_FailureLogic.Clear();
}

void CMissionData::InventoryCallback(idEntity *ent, idStr ItemName, int value, int OverallVal, bool bPickedUp)
{
	SObjEntParms Parms;

	if( ent )
		FillParmsData( ent, &Parms );

	Parms.group = ItemName;
	Parms.value = value;
	Parms.valueSuperGroup = OverallVal;

	MissionEvent( COMP_ITEM, &Parms, bPickedUp );

	return;
}

int CMissionData::GetTotalLoot( void )
{
	return m_Stats.LootOverall;
}

void CMissionData::ChangeTotalLoot(int amount)
{
	m_Stats.LootOverall += amount;
}


// =============== Boolean Logic Parsing for Objective Failure/Success ==============

bool CObjective::CheckFailure( void )
{
	bool bTest(false);

	if( !m_FailureLogic.IsEmpty() )
		bTest = gameLocal.m_MissionData->EvalBoolLogic( &m_FailureLogic, false, m_ObjNum );
	else
	{
		// Default logic: If ANY components of an ongoing objective are false, the objective is failed
		if( m_bOngoing && !(m_state == STATE_INVALID) )
		{
			bTest = true;
			for( int j=0; j < m_Components.Num(); j++ )
			{
				bTest = bTest && m_Components[j].m_bState;
			}

			bTest = !bTest;
		}
	}
	return bTest;
}

bool CObjective::CheckSuccess( void )
{
	bool bTest(true);
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Check Success Called \r");

	if( !m_SuccessLogic.IsEmpty() )
	{
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Evaluating custom success logic \r");
		bTest = gameLocal.m_MissionData->EvalBoolLogic( &m_SuccessLogic, false, m_ObjNum );
	}
	else
	{
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Evaluating default success logic \r");
		// Default logic: All components must be true to succeed
		for( int j=0; j < m_Components.Num(); j++ )
		{
			bTest = bTest && m_Components[j].m_bState;
		}
	}
	return bTest;
}


/**
* Parse the boolean logic strings into matrices.
* Returns false if there was an error in the parsing.
**/
bool CObjective::ParseLogicStrs( void )
{
	bool bReturnVal(true), bTemp(false);

	if( m_SuccessLogicStr != "" )
	{
		bReturnVal = gameLocal.m_MissionData->ParseLogicStr( &m_SuccessLogicStr, &m_SuccessLogic );
		
		if( !bReturnVal )
			gameLocal.Error("Objective success logic failed to parse \n");
	}
	if( m_FailureLogicStr != "" )
	{
		bTemp = gameLocal.m_MissionData->ParseLogicStr( &m_FailureLogicStr, &m_FailureLogic );
		
		if( !bTemp )
			gameLocal.Error("Objective failure logic failed to parse \n");

		bReturnVal = bReturnVal && bTemp;
	}

	return bReturnVal;
}

/**
* Parse a string into a logic matrix.
* Returns false if there was an error in the parsing
**/
bool CMissionData::ParseLogicStr( idStr *input, SBoolParseNode *output )
{
	idLexer		src;
	idToken		token;
	int			col(0), row(0), level(0);

	bool		bReturnVal( false );
	bool		bFollowingOperator( false ); // whether we expect an identifier or open parenthesis
	bool		bOperatorOK( false ); // can the next token be an operator
	bool		bNOTed( false ); // next parse node will be NOTted
	// initialize as advancing to 0,0 at start of parsing
	bool		bRowAdvanced( true );
	bool		bColAdvanced( true );

	SBoolParseNode *CurrentNode( NULL );


	// set up outer node:
	output->Clear();
	output->PrevNode = NULL;
	CurrentNode = output;

	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Parsing string: %s \r", input->c_str() );
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Outer parse node is %08lx \r", CurrentNode);

	src.LoadMemory( input->c_str(), input->Length(), "" );
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Loaded memory to lexer \r" );

	while( src.ReadToken( &token ))
	{
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Parsing token: %s At level: %d \r", token.c_str(), level );
		if( level < 0 )
		{
			gameLocal.Printf("[Objective Logic] ERROR: Unbalanced parenthesis, found unexpected \")\" \n"); 
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Unbalanced parenthesis, found unexpected \")\" \r");
			goto Quit;
		}

		// New parse node (identifier or parenthesis)
		if( token.IsNumeric() || (token.Cmp( "(" ) == 0) )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] New parse node ( identifier or \"(\" ) \r" );
			bFollowingOperator = false;

			SBoolParseNode NewNode;
			NewNode.bNotted = bNOTed;
			NewNode.PrevRow = row;
			NewNode.PrevCol = col;
			NewNode.PrevNode = CurrentNode;
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Parse: New node at %d, %d, points at previous node: %08lx \r", row, col, CurrentNode);

			if( token.IsNumeric() )
			{
				// Node is a leaf: set Ident to the identifier
				NewNode.Ident = token.GetIntValue() - 1;
			}

			// Add node to the appropriate point in the matrix-tree - same for leaves and branches
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Adding new node to matrix-tree \r" );
			if( bColAdvanced )
			{
				idList< SBoolParseNode > NewCol;
				NewCol.Append( NewNode );
				CurrentNode->Cols.Append( NewCol );
			}
			else if( bRowAdvanced )
			{
				CurrentNode->Cols[ col ].Append( NewNode );
			}

			// If neither row nor column advanced, we have a problem, such as two leaves in a row
			else
			{
				gameLocal.Printf("[Objective Logic] ERROR: Unexpected identifier found \n");
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Unexpected identifier found \r");
				goto Quit;
			}

			// Node is a branch, step in
			if( token.Cmp( "(" ) == 0 )
			{
				level++;
				bOperatorOK = false;

				CurrentNode = &CurrentNode->Cols[ col ].operator[]( row );
				row = 0;
				col = 0;
				// new level expects these to be true
				bRowAdvanced = true;
				bColAdvanced = true;
			}
			// node is a leaf, keep going on same level
			else
			{
				bOperatorOK = true;
				bRowAdvanced = false;
				bColAdvanced = false;
			}

			bNOTed = false;
		} // New Parse Node

		else if( token.Icmp( "AND" ) == 0 )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Parsing AND operator \r" );

			if( bFollowingOperator || !bOperatorOK )
			{
				gameLocal.Printf("[Objective Logic] ERROR: Found unexpected operator AND \n");
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Found unexpected operator AND \r");
				goto Quit;
			}
			bFollowingOperator = true;
			bOperatorOK = false;

			col++;
			bColAdvanced = true;
		} // AND

		else if( token.Icmp( "OR" ) == 0 )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Parsing OR operator \r" );

			if( bFollowingOperator || !bOperatorOK )
			{
				gameLocal.Printf("[Objective Logic] ERROR: Found unexpected operator OR \n");
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Found unexpected operator OR \r");
				goto Quit;
			}

			bFollowingOperator = true;
			bOperatorOK = false;

			row++;
			bRowAdvanced = true;
		} // OR

		else if( token.Icmp( "NOT" ) == 0 )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Parsing NOT operator \r" );
			bNOTed = true;
			bOperatorOK = false;
		} // NOT

		else if( token.Cmp( ")" ) == 0 )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Parsing \")\" \r" );
			if( bFollowingOperator )
			{
				gameLocal.Printf("[Objective Logic] ERROR: Identifier expected, found \")\" \n");
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Identifier expected, found \")\" \r");
				goto Quit;
			}

			// If level is empty, report error
			if( CurrentNode->Cols.Num() == 0 )
			{
				gameLocal.Printf("[Objective Logic] ERROR: Identifier expected, found \")\" \n");
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Identifier expected, found \")\" \r");
				goto Quit;
			}
		
			// step out
			level--;

			row = CurrentNode->PrevRow;
			col = CurrentNode->PrevRow;
			CurrentNode = CurrentNode->PrevNode;
		} // Step Out of Node

		else
		{
			gameLocal.Printf("[Objective Logic] ERROR: Unrecognized token: %s \n", token.c_str() ); 
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Unrecognized token: %s \r", token.c_str() ); 
			goto Quit;
		}
	}


// Finished parsing
DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Reached EOF \r");

if( level != 0 )
	{
		gameLocal.Printf("[Objective Logic] ERROR: Unbalanced parenthesis, expected \")\" not found \n");
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Unbalanced parenthesis, expected \")\" not found \r");
		goto Quit;
	}

if( bFollowingOperator )
	{
		gameLocal.Printf("[Objective Logic] ERROR: Expected identifier, found EOF \n");
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] ERROR: Expected identifier, found EOF \n");
		goto Quit;
	}

	// Successfully parsed
	DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Successfully parsed \r");
	bReturnVal = true;

Quit:

	src.FreeSource();
	return bReturnVal;
}

bool CMissionData::EvalBoolLogic( SBoolParseNode *StartNode, bool bObjComp, int ObjNum )
{

	int level(0); // current level of branching
	bool bReturnVal(false);
	bool bResolvedLevel(false); // have we completely evaluated the lower level in the previous pass of the loop?
	bool bLowerLevResult(false); // the result of evaluating the lower level in the prev. pass of loop
	SBoolParseNode *CurrentNode = NULL;

	int CurrentCol(0), CurrentRow(0); // matrix coordinates in the current level


	if(!StartNode)
	{
		// Log error
		goto Quit;
	}

	CurrentNode = StartNode;


/* PSUEDOCODE:
Will always do 1 of 3 things:
1. Back up a level because we hit a leaf
2. Advance the matrix and back up a level because the matrix is "done"
3. Advance the matrix and go down a level because the matrix is not yet done

Going down one level can only happen because we finished an evaluation in
	the previous step, and we now want to advance to the next matrix spot and
	go down a level at the node at that next matrix spot

When we advance to the next matrix spot, it can happen in two ways:
1. We have evaluated TRUE in the previous step

	If there is a next column, go to the first row of that next column and go down a level

	If there is no next column, we are done with this level, eval it as TRUE and go up

2. We have evaluated FALSE in the previous step
	If there is another row, go to that next column and go down a level

	If there is no next row, we are done with this level, eval it as FALSE and go up
*/

	while( level >= 0 && (CurrentNode != NULL) )
	{
		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Evaluating: level %d, row %d, column %d\r", level, CurrentRow, CurrentCol);
		
		// check if the node on this level contains a matrix (branch)
		// If it does not, it must be directly addressing a component (leaf)
		if( CurrentNode->Cols.Num() <= 0 )
		{
			// Leaf found, evaluate and go up a level
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Evaluating leaf with identifier %d\r", CurrentNode->Ident);
			
			if( bObjComp )
			{
				int state = GetCompletionState( CurrentNode->Ident );
				bLowerLevResult = (state == STATE_COMPLETE);
			}
			else
				bLowerLevResult = GetComponentState( ObjNum, CurrentNode->Ident );

			if( CurrentNode->bNotted )
				bLowerLevResult = !bLowerLevResult;

			bResolvedLevel = true;

			CurrentCol = CurrentNode->PrevCol;
			CurrentRow = CurrentNode->PrevRow;
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("PrevNode in leaf at %d, %d, points at previous node: %08lx \r", CurrentRow, CurrentCol, CurrentNode->PrevNode);
			CurrentNode = CurrentNode->PrevNode;

			level--;
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Leaf evaluated, stepping out \r");
			continue;
		}

		// if we have just backed up a level, advance the matrix appropriately

		// If we evaluate TRUE in the lower level:
		if( bResolvedLevel && bLowerLevResult )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Resolved previous level as TRUE\r");
			// if there is no next column, this level evals to TRUE due to AND logic success
			if( CurrentCol + 1 >= CurrentNode->Cols.Num() )
			{
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] AND logic success at current level, stepping out \r");
				bLowerLevResult = true;

				CurrentCol = CurrentNode->PrevCol;
				CurrentRow = CurrentNode->PrevRow;
				CurrentNode = CurrentNode->PrevNode;

				level--;
				continue;
			}
			// else, advance to next column and go down a level
			else
			{
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Continuing on in current level, to next column \r");
				CurrentCol++;
				CurrentRow = 0;
			}
		}
		// If we came back up after we evaluated to FALSE in the lower level
		if( bResolvedLevel && !bLowerLevResult )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Resolved previous level as FALSE\r");

			// If there are no more rows in this column, evaluate this level to FALSE (Due to AND logic failure)
			if( CurrentRow + 1 >= CurrentNode->Cols.operator[](CurrentCol).Num() )
			{
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Current level failed (no more rows to OR), stepping out\r");
				bLowerLevResult = false;

				CurrentCol = CurrentNode->PrevCol;
				CurrentRow = CurrentNode->PrevRow;
				CurrentNode = CurrentNode->PrevNode;

				level--;
				continue;
			}
			// else, advance to the next row
			{
				DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Continuing on in current level, to next row\r");
				CurrentRow++;
			}
		}

		DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("[Objective Logic] Going down a level\r");
		// If we get to this point in the loop, we must be going down a level
		bResolvedLevel = false;
		CurrentNode = &CurrentNode->Cols[CurrentCol].operator[](CurrentRow);

		level++;
	}

	bReturnVal = bLowerLevResult;

Quit:
	return bReturnVal;
}

void CObjective::Save( idSaveGame *savefile ) const
{
	savefile->WriteString( m_text );
	savefile->WriteBool( m_bMandatory );
	savefile->WriteBool( m_bVisible );
	savefile->WriteBool( m_bOngoing );
	savefile->WriteBool( m_bApplies );
	savefile->WriteInt( m_ObjNum );
	savefile->WriteInt( m_handle );
	savefile->WriteInt( m_state );
	savefile->WriteBool( m_bNeedsUpdate );
	savefile->WriteBool( m_bReversible );
	savefile->WriteBool( m_bLatched );

	savefile->WriteInt( m_Components.Num() );
	for( int i=0; i < m_Components.Num(); i++ )
		m_Components[i].Save( savefile );

	savefile->WriteInt( m_EnablingObjs.Num() );
	for( int j=0; j < m_EnablingObjs.Num(); j++ )
		savefile->WriteInt( m_EnablingObjs[j] );

	savefile->WriteString( m_CompletionScript );
	savefile->WriteString( m_FailureScript );
	savefile->WriteString( m_SuccessLogicStr );
	savefile->WriteString( m_FailureLogicStr );
}

void CObjective::Restore( idRestoreGame *savefile )
{
	int num(0), tempInt(0);
	savefile->ReadString( m_text );
	savefile->ReadBool( m_bMandatory );
	savefile->ReadBool( m_bVisible );
	savefile->ReadBool( m_bOngoing );
	savefile->ReadBool( m_bApplies );
	savefile->ReadInt( m_ObjNum );
	savefile->ReadInt( m_handle );
	savefile->ReadInt( tempInt );
	m_state = (EObjCompletionState) tempInt;
	savefile->ReadBool( m_bNeedsUpdate );
	savefile->ReadBool( m_bReversible );
	savefile->ReadBool( m_bLatched );

	savefile->ReadInt( num );
	m_Components.SetNum( num );
	for( int i=0; i < num; i++ )
		m_Components[i].Restore( savefile );

	savefile->ReadInt( num );
	m_EnablingObjs.SetNum( num );
	for( int j=0; j < num; j++ )
		savefile->ReadInt( m_EnablingObjs[j] );

	savefile->ReadString( m_CompletionScript );
	savefile->ReadString( m_FailureScript );
	savefile->ReadString( m_SuccessLogicStr );
	savefile->ReadString( m_FailureLogicStr );

	// We have to re-parse the logic since the parse nodes involve raw pointer linkages
	ParseLogicStrs();
}


/*===========================================================================
*
*CObjectiveLocation
*
*============================================================================*/
CLASS_DECLARATION( idEntity, CObjectiveLocation )
END_CLASS

CObjectiveLocation::CObjectiveLocation( void )
{
	m_Interval = 1000;
	m_TimeStamp = 0;

	m_EntsInBounds.Clear();
}

void CObjectiveLocation::Spawn()
{
	m_Interval = (int) 1000.0f * spawnArgs.GetFloat( "interval", "1.0" );
	m_TimeStamp = gameLocal.time;

// Set the contents to a useless trigger so that the collision model will be loaded
// FLASHLIGHT_TRIGGER seems to be the only one that doesn't do anything else we don't want
	GetPhysics()->SetContents( CONTENTS_FLASHLIGHT_TRIGGER );

	// SR CONTENTS_RESONSE FIX
	if( m_StimResponseColl->HasResponse() )
		GetPhysics()->SetContents( GetPhysics()->GetContents() | CONTENTS_RESPONSE );

	GetPhysics()->EnableClip();

	BecomeActive( TH_THINK );
}

void CObjectiveLocation::Think()
{
	int NumEnts(0);
	idEntity *Ents[MAX_GENTITIES];
	idStrList current, added, missing;
	bool bFound(false);

	// only check on clock ticks
	if( (gameLocal.time - m_TimeStamp) < m_Interval )
		goto Quit;

	m_TimeStamp = gameLocal.time;

	// bounding box test
	NumEnts = gameLocal.clip.EntitiesTouchingBounds(GetPhysics()->GetAbsBounds(), -1, Ents, MAX_GENTITIES);
	for( int i=0; i<NumEnts; i++ )
	{
		if( Ents[i] && Ents[i]->m_bIsObjective )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objective location %s found entity %s during clock tick. \r", name.c_str(), Ents[i]->name.c_str() );
			current.Append( Ents[i]->name );
		}
	}

	// compare current list to previous clock tick list to generate added list
	for( int i = 0; i < current.Num(); i++ )
	{
		bFound = false;
		for( int j = 0; j < m_EntsInBounds.Num(); j++ )
		{
			if( current[i] == m_EntsInBounds[j] )
			{
                  bFound = true;
                  break;
            }
		}

		if( !bFound )
		{
			added.Append( current[i] );
		}
	}

	// compare again the other way to generate missing list
	for( int i = 0; i < m_EntsInBounds.Num(); i++ )
	{
		bFound = false;
		for( int j = 0; j < current.Num(); j++ )
		{
			if( m_EntsInBounds[i] == current[j] )
			{
                  bFound = true;
                  break;
            }
		}

		if( !bFound )
		{
			missing.Append( m_EntsInBounds[i] );
		}
	}

	// call objectives system for all missing or added ents
	for( int i=0; i<added.Num(); i++ )
	{
		idEntity *Ent = gameLocal.FindEntity( added[i].c_str() );
		if( Ent )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objective entity %s entered objective location %s \r", Ent->name.c_str(), name.c_str() );
			gameLocal.m_MissionData->MissionEvent( COMP_LOCATION, Ent, this, true );
		}
	}

	for( int j=0; j<missing.Num(); j++ )
	{
		idEntity *Ent2 = gameLocal.FindEntity( missing[j].c_str() );
		if( Ent2 )
		{
			DM_LOG(LC_AI,LT_DEBUG)LOGSTRING("Objective entity %s left objective location %s \r", Ent2->name.c_str(), name.c_str() );
			gameLocal.m_MissionData->MissionEvent( COMP_LOCATION, Ent2, this, false );
		}
	}

	// copy over the list
	m_EntsInBounds.Clear();
	m_EntsInBounds = current;

	current.Clear();
	missing.Clear();
	added.Clear();

Quit:
	idEntity::Think();
	return;
}
