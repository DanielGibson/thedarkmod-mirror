/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/
/*
* Implementation of the CAIComm_Message class.
*
* Copyright 2006: Damon Hill (sophisticatedZombie), USA
*
*/


/*
*--------------------------------------------------------------------------
* Includes
*--------------------------------------------------------------------------
*/
#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id$", init_version);

#include "DarkModGlobals.h"
#include "AIComm_Message.h"

namespace ai {

/*
*--------------------------------------------------------------------------
* Constructor and Destructor
*--------------------------------------------------------------------------
*/

CommMessage::CommMessage
(
	TCommType in_commType,
	float in_maximumRadiusInWorldCoords,
	idEntity* in_p_issuingEntity,
	idEntity* in_p_recipientEntity,
	idEntity* in_p_directObjectEntity,
	const idVec3& in_directObjectLocation
)
{
	// Set member variables
	m_commType = in_commType;
	m_maximumRadiusInWorldCoords = in_maximumRadiusInWorldCoords;
	m_p_issuingEntity = in_p_issuingEntity;
	m_p_recipientEntity = in_p_recipientEntity;
	m_p_directObjectEntity = in_p_directObjectEntity;
	m_directObjectLocation = in_directObjectLocation;

	// Record position of issuance
	m_positionOfIssuance.x = 0.0;
	m_positionOfIssuance.y = 0.0;
	m_positionOfIssuance.z = 0.0;
	if (m_p_issuingEntity.GetEntity() != NULL)
	{
		idPhysics* p_phys  = m_p_issuingEntity.GetEntity()->GetPhysics();
		if (p_phys != NULL)
		{
			m_positionOfIssuance = p_phys->GetOrigin();
		}
		else
		{
			DM_LOG (LC_STIM_RESPONSE, LT_WARNING)LOGSTRING("Issuer has no physics object, so stim considered to be issued from 0,0,0");
		}
	}
	else
	{
		DM_LOG (LC_STIM_RESPONSE, LT_WARNING)LOGSTRING("No issuing entity, so stim considered to be issued from 0,0,0");
	}

	// Done
}

void CommMessage::Save(idSaveGame *savefile) const
{
	savefile->WriteInt(static_cast<int>(m_commType));
	m_p_issuingEntity.Save(savefile);
	m_p_recipientEntity.Save(savefile);
	m_p_directObjectEntity.Save(savefile);
	savefile->WriteVec3(m_directObjectLocation);
	savefile->WriteVec3(m_positionOfIssuance);
	savefile->WriteFloat(m_maximumRadiusInWorldCoords);
}

void CommMessage::Restore(idRestoreGame *savefile)
{
	int tempInt;
	savefile->ReadInt(tempInt);
	m_commType = static_cast<TCommType>(tempInt);
	m_p_issuingEntity.Restore(savefile);
	m_p_recipientEntity.Restore(savefile);
	m_p_directObjectEntity.Restore(savefile);
	savefile->ReadVec3(m_directObjectLocation);
	savefile->ReadVec3(m_positionOfIssuance);
	savefile->ReadFloat(m_maximumRadiusInWorldCoords);
}

} // namespace ai
