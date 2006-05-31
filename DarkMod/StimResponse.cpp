/******************************************************************************/
/*                                                                            */
/*         StimResponse (C) by Gerhard W. Gruber in Germany 2005              */
/*                          All rights reserved                               */
/*                                                                            */
/******************************************************************************/

/******************************************************************************
 *
 * PROJECT: DarkMod
 * $Source$
 * $Revision$
 * $Date$
 * $Author$
 * $Name$
 *
 * $Log$
 * Revision 1.12  2006/05/31 20:24:55  sparhawk
 * Added timerstim skeleton
 *
 * Revision 1.11  2006/05/17 05:40:43  sophisticatedzombie
 * Made TriggerResponse virtual.
 * Added virtual PostFired event to CStim to handle state cleanup in descended classes after stim fired.
 *
 * Revision 1.10  2006/05/03 21:32:02  sparhawk
 * Scripthread interface changed. Now it takes
 * values instead of pointer to values.
 *
 * Revision 1.9  2006/04/26 21:29:46  sparhawk
 * Timed stim/response core added.
 *
 * Revision 1.8  2006/03/13 21:05:20  sparhawk
 * SIT stimtype replaced with INVITE to name it more generic
 *
 * Revision 1.7  2006/02/07 18:55:01  sparhawk
 * 1. State is now moved to CStimResponse so responses can now also be disabled.
 * 2. Removed state SS_ACTIVE (what was that again for???)
 *
 * Revision 1.6  2006/02/06 22:13:51  sparhawk
 * Added ignore list for responses.
 *
 * Revision 1.5  2006/02/05 22:14:20  sparhawk
 * Response now looks on the local object for the script function, before it looks for a global function.
 *
 * Revision 1.4  2006/02/04 23:51:39  sparhawk
 * Finished the Stim/Response for radius types.
 *
 * Revision 1.3  2006/01/31 22:34:44  sparhawk
 * StimReponse first working version
 *
 * Revision 1.2  2006/01/24 22:03:24  sparhawk
 * Stim/Response implementation preliminary
 *
 * Revision 1.1  2005/12/08 21:34:28  sparhawk
 * Intitial release
 *
 *
 * DESCRIPTION: Implementation for the Stim/Response system.
 *
 *****************************************************************************/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "DarkModGlobals.h"
#include "StimResponse.h"
#include "AIComm_StimResponse.h"

// If default stims are to be added, this array also must be
// updated. USER and UNDEFINED are not to be added though, as
// they have special meanings. This array allows us to reuse
// the name in the key in the entity definition for the 
// predefined stim/responses instead of their numerical values.
char *cStimType[] = {
	"STIM_FROB",
	"STIM_FIRE",
	"STIM_WATER",
	"STIM_DAMAGE",
	"STIM_SHIELD",
	"STIM_HEALING",
	"STIM_HOLY",
	"STIM_MAGIC",
	"STIM_TOUCH",
	"STIM_KNOCKOUT",
	"STIM_KILL",
	"STIM_RESTORE",
	"STIM_LIGHT",
	"STIM_SOUND",
	"STIM_VISUAL",
	"STIM_INVITE",
	"STIM_READ",
	"STIM_RANDOM",
	"STIM_TIMER",
	"STIM_COMMUNICATION",
	NULL
};

CStimResponseCollection::CStimResponseCollection(void)
{
}

CStimResponseCollection::~CStimResponseCollection(void)
{
}

CStim* CStimResponseCollection::createStim(idEntity* p_owner, StimType type)
{
		CStim* pRet;

		if (type == ST_COMMUNICATION)
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG).LogString ("Creating CAIComm_Stim");
			pRet = new CAIComm_Stim (p_owner, type);
		}
		else
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG).LogString ("Creating CStim");
			pRet = new CStim(p_owner, type);
		}

		return pRet;
}

CResponse* CStimResponseCollection::createResponse(idEntity* p_owner, StimType type)
{
		CResponse* pRet;

		if (type == ST_COMMUNICATION)
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG).LogString ("Creating CAIComm_Response");
			pRet = new CAIComm_Response (p_owner, type);
		}
		else
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG).LogString ("Creating CResponse");
			pRet = new CResponse(p_owner, type);
		}

		return pRet;
}

CStim *CStimResponseCollection::AddStim(idEntity *Owner, int Type, float fRadius, bool bRemovable, bool bDefault)
{
	CStim *pRet = NULL;
	int i, n;

	n = m_Stim.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Stim[i]->m_StimTypeId == Type)
		{

			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG).LogString ("Stim of that type is already in collection, returning it");
			pRet = m_Stim[i];
			break;
		}
	}

	if(pRet == NULL)
	{
		// Create either type specific descended class, or the default base class
		pRet = createStim(Owner, (StimType) Type);
		m_Stim.Append(pRet);
	}

	if(pRet != NULL)
	{
		pRet->m_Default = bDefault;
		pRet->m_Removable = bRemovable;
		pRet->m_Radius = fRadius;
		pRet->m_State = SS_DISABLED;

		AddEntityToList((idList<void *>	&)gameLocal.m_StimEntity, Owner); 
	}

	return pRet;
}

CResponse *CStimResponseCollection::AddResponse(idEntity *Owner, int Type, bool bRemovable, bool bDefault)
{
	CResponse *pRet = NULL;
	int i, n;

	n = m_Response.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Response[i]->m_StimTypeId == Type)
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG).LogString ("Response of that type is already in collection, returning it");
			pRet = m_Response[i];
			break;
		}
	}

	if(pRet == NULL)
	{
		pRet = createResponse (Owner, (StimType) Type);
		m_Response.Append(pRet);
	}

	if(pRet != NULL)
	{
		pRet->m_Default = bDefault;
		pRet->m_Removable = bRemovable;

		AddEntityToList((idList<void *>	&)gameLocal.m_RespEntity, Owner); 
	}

	return pRet;
}

CStim *CStimResponseCollection::AddStim(CStim *s)
{
	CStim *pRet = NULL;
	int i, n;

	if(s == NULL)
		goto Quit;

	n = m_Stim.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Stim[i]->m_StimTypeId == s->m_StimTypeId)
		{
			pRet = m_Stim[i];
			break;
		}
	}

	if(pRet == NULL)
	{
		pRet = s;
		m_Stim.Append(pRet);

		AddEntityToList((idList<void *>	&)gameLocal.m_StimEntity, s->m_Owner); 
	}

Quit:
	return pRet;
}

CResponse *CStimResponseCollection::AddResponse(CResponse *r)
{
	CResponse *pRet = NULL;
	int i, n;

	if(r == NULL)
		goto Quit;

	n = m_Response.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Response[i]->m_StimTypeId == r->m_StimTypeId)
		{
			pRet = m_Response[i];
			break;
		}
	}

	if(pRet == NULL)
	{
		pRet = r;
		m_Response.Append(pRet);

		AddEntityToList((idList<void *>	&)gameLocal.m_RespEntity, r->m_Owner);
	}

Quit:
	return pRet;
}


int CStimResponseCollection::RemoveStim(int Type)
{
	CStim *pRet = NULL;
	int i, n;

	n = m_Stim.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Stim[i]->m_StimTypeId == Type)
		{
			pRet = m_Stim[i];
			if(pRet->m_Removable == true)
			{
				m_Stim.RemoveIndex(i);
				delete pRet;
			}
			break;
		}
	}

	return m_Stim.Num();
}

int CStimResponseCollection::RemoveResponse(int Type)
{
	CResponse *pRet = NULL;
	int i, n;

	n = m_Response.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Response[i]->m_StimTypeId == Type)
		{
			pRet = m_Response[i];
			if(pRet->m_Removable == true)
			{
				m_Response.RemoveIndex(i);
				delete pRet;
			}
			break;
		}
	}

	return m_Response.Num();
}

int CStimResponseCollection::RemoveStim(CStim *s)
{
	CStim *pRet = NULL;
	int i, n;

	n = m_Stim.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Stim[i] == s)
		{
			pRet = m_Stim[i];
			if(pRet->m_Removable == true)
			{
				m_Stim.RemoveIndex(i);
				delete pRet;
			}
			break;
		}
	}

	return m_Stim.Num();
}

int CStimResponseCollection::RemoveResponse(CResponse *r)
{
	CResponse *pRet = NULL;
	int i, n;

	n = m_Response.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Response[i] == r)
		{
			pRet = m_Response[i];
			if(pRet->m_Removable == true)
			{
				m_Response.RemoveIndex(i);
				delete pRet;
			}
			break;
		}
	}

	return m_Response.Num();
}


void CStimResponseCollection::AddEntityToList(idList<void *> &oList, void *e)
{
	bool add = true;
	int i, n;

	n = oList.Num();
	for(i = 0; i < n; i++)
	{
		if(oList[i] == e)
		{
			add = false;
			break;
		}
	}

	if(add == true)
		oList.Append(e);
}

CStimResponse *CStimResponseCollection::GetStimResponse(int StimType, bool Stim)
{
	CStimResponse *rc = NULL, *sr;
	int i, n;
	idList<CStimResponse *> *oList = (Stim == true) ? (idList<CStimResponse *> *)&m_Stim : (idList<CStimResponse *> *)&m_Response;

	n = oList->Num();
	for(i = 0; i < n; i++)
	{
		sr = oList->operator[](i);
		if(sr->m_StimTypeId == StimType)
		{
			rc = sr;
			break;
		}
	}

	return rc;
}

CStim *CStimResponseCollection::GetStim(int StimType)
{
	return (CStim *)GetStimResponse(StimType, true);
}

CResponse *CStimResponseCollection::GetResponse(int StimType)
{
	return (CResponse *)GetStimResponse(StimType, false);
}

bool CStimResponseCollection::ParseSpawnArg(const idDict *args, idEntity *Owner, const char sr_class, int Counter)
{
	bool rc = false;
	idStr str;
	idStr name;
	CStim *stim = NULL;
	CResponse *resp = NULL;
	CStimResponse *sr = NULL;
	float Radius = 0.0f;
	StimState state;

	// Check if the entity contains either a stim or a response.
	if(sr_class != 'S' && sr_class != 'R')
	{
		DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Invalid sr_class value [%s]\r", str.c_str());
		goto Quit;
	}

	// Get the id of the stim/response type so we know what sub-class to create
	sprintf(name, "sr_type_%u", Counter);
	args->GetString(name, "-1", str);

	// This is invalid as an entity definition
	if(str == "-1")
	{
		sr = NULL;
		goto Quit;
	}

	// If the first character is alphanumeric, we check if it 
	// is a known id and convert it.
	StimType typeOfStim = ST_DEFAULT;

	if((str[0] >= 'a' && str[0] <= 'z')
		|| (str[0] >= 'A' && str[0] <= 'Z'))
	{
		int i = 0;
		bool found = false;

		while(cStimType[i] != NULL)
		{
			if(str == cStimType[i])
			{
				typeOfStim = (StimType) i;
				found = true;
				break;
			}
			i++;
		}

		// If the string hasn't been found, we have an invalid id.
		if(found == false)
		{
			DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Invalid sr_type id [%s]\r", str.c_str());
			sr = NULL;
			goto Quit;
		}
	}
	else if(str[0] >= '0' && str[0] <= '9') // Is it numeric?
	{	
		typeOfStim = (StimType) atol(str.c_str());
	}
	else		// neither a character nor a number, thus it is invalid.
	{
		DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Invalid sr_type id [%s]\r", str.c_str());
		sr = NULL;
		goto Quit;
	}


	if(sr_class == 'S')
	{
		stim = createStim (Owner, typeOfStim);
		sr = stim;
	}
	else if (sr_class == 'R')
	{
		resp = createResponse (Owner, typeOfStim);
		sr = resp;
	}

	// Set stim response type
	sr->m_StimTypeId = typeOfStim;

	// Set stim response name string
	sr->m_StimTypeName = str;

	// Read stim response state from the def file
	sprintf(name, "sr_state_%u", Counter);
	args->GetInt(name, "1", (int &)state);
	if(state != SS_DISABLED
		&& state != SS_ENABLED)
	{
		DM_LOG(LC_STIM_RESPONSE, LT_WARNING)LOGSTRING("Invalid state %u for %s defaulting to enabled (%u)\r", state, sr->m_StimTypeName.c_str(), SS_ENABLED);
		state = SS_ENABLED;
	}
	sr->m_State = state;

	// A stim also may have a radius
	if(sr_class == 'S')
	{
		sprintf(name, "sr_radius_%u", Counter);
		args->GetFloat(name, "0.0", Radius);
		stim->m_Radius = Radius;

		// Check if we have a timer on this stim.
		CreateTimer(args, stim); 
	}
	else	// this is only for responses
	{
		// Get the name of the script function for processing the response
		name = "sr_script_" + str;
		args->GetString(name, "", str);
		if(str == "")
		{
			DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("No scriptfunction specified for stim/response id [%s]\r", sr->m_StimTypeName.c_str());
			delete sr;
			sr = NULL;
			goto Quit;
		}

		resp->m_ScriptFunction = str;
	}

	rc = true;

Quit:
	if(sr != NULL)
	{
		if(stim != NULL)
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Stim %08lX added to collection for %08lX\r", stim, Owner);
			AddStim(stim);
			stim->m_State = state;
		}

		if(resp != NULL)
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Response %08lX added to collection for %08lX\r", resp, Owner);
			AddResponse(resp);
		}
	}

	return rc;
}

void CStimResponseCollection::ParseSpawnArgsToStimResponse(const idDict *args, idEntity *Owner)
{
	idStr str;
	idStr name;
	int i;
	char sr_class;

	if(Owner == NULL)
	{
		DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Owner set to NULL is not allowed!\r");
		goto Quit;
	}

	i = 1;
	while(i != 0)
	{
		sprintf(name, "sr_class_%u", i);
		DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Looking for %s\r", name.c_str());
		if(!args->GetString(name, "X", str))
			goto Quit;

		sr_class = str[0];
		if(ParseSpawnArg(args, Owner, sr_class, i) == false)
			goto Quit;

		i++;
	}

Quit:
	return;
}


void CStimResponseCollection::CreateTimer(const idDict *args, CStim *stim)
{
	idStr str;
	int n;
	CStimResponseTimer *t = NULL;

	t = new CStimResponseTimer;

	args->GetString("sr_timer_duration", "", str);
	if((t->m_Duration = t->ParseTimeString(str)) == TIMER_UNDEFINED)
		goto Quit;

	args->GetInt("sr_timer_reload", "-1", n);
	t->m_Reload = n;

	args->GetString("sr_timer_type", "RELOAD", str);
	if(str.Cmp("RELOAD") == 0)
		t->m_Type = CStimResponseTimer::SRTT_RELOAD;
	else
		t->m_Type = CStimResponseTimer::SRTT_SINGLESHOT;

	AddEntityToList((idList<void *>	&)gameLocal.m_StimTimer, stim);

	args->GetString("sr_timer_reload_duration", "", str);
	t->m_ReloadTimer = t->ParseTimeString(str);

	args->GetString("sr_timer_apply_duration", "", str);
	t->m_Apply = t->ParseTimeString(str);

	stim->m_Timer = t;
	t = NULL;
	AddEntityToList((idList<void *>	&)gameLocal.m_StimTimer, stim); 

Quit:
	if(t)
		delete t;

	return;
}

/********************************************************************/
/*                    CStimResponse                                 */
/********************************************************************/
CStimResponse::CStimResponse(idEntity *Owner, int Type)
{
	m_StimTypeId = Type;
	m_Owner = Owner;
	m_State = SS_DISABLED;
	m_Removable = true;
	m_Default = false;
}

CStimResponse::~CStimResponse(void)
{
}

void CStimResponse::EnableSR(bool bEnable)
{
	if(bEnable == true)
		m_State = SS_ENABLED;
	else
		m_State = SS_DISABLED;
}

/********************************************************************/
/*                     CStim                                        */
/********************************************************************/
CStim::CStim(idEntity *e, int Type)
: CStimResponse(e, Type)
{
	m_Timer = NULL;
	m_Radius = 0.0;
	m_TriggerDamage = 0.0;
	m_DurationDamage = 0.0;
	m_Chance = 1.0;
	m_ChanceTimer = false;
	m_MaxResponses = 0;
	m_CurResponses = 0;
	m_ApplyTimer = 0;
	m_ApplyTimerVal = 0;
}

CStim::~CStim(void)
{
	if(m_Timer != NULL)
		delete m_Timer;
}

void CStim::AddResponseIgnore(idEntity *e)
{
	if(CheckResponseIgnore(e) != true)
		m_ResponseIgnore.Append(e);
}

void CStim::RemoveResponseIgnore(idEntity *e)
{
	m_ResponseIgnore.Remove(e);
}

bool CStim::CheckResponseIgnore(idEntity *e)
{
	bool rc = false;
	int i, n;

	n = m_ResponseIgnore.Num();
	for(i = 0; i < n; i++)
	{
		if(m_ResponseIgnore[i] == e)
		{
			rc = true;
			break;
		}
	}

	return rc;
}

void CStim::PostFired (int numResponses)
{
	// Default implementation does nothing with this event
	int blah;
	blah = 0;

}



/********************************************************************/
/*                   CResponse                                      */
/********************************************************************/
CResponse::CResponse(idEntity *e, int Type)
: CStimResponse(e, Type)
{
	m_FollowUp = NULL;
	m_ScriptFunction = NULL;
	m_MinDamage = 0.0f;
	m_MaxDamage = 0;
	m_Chance = 1.0f;
}

CResponse::~CResponse(void)
{
}

void CResponse::TriggerResponse(idEntity *StimEnt)
{
	DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("CResponse::TriggerResponse \r");

	DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Response for Id %s triggered (Action: %s)\r", m_StimTypeName.c_str(), m_ScriptFunction.c_str());

	const function_t *pScriptFkt = m_Owner->scriptObject.GetFunction(m_ScriptFunction.c_str());
	if(pScriptFkt == NULL)
	{
		DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Action: %s not found in local space, checking for global.\r", m_ScriptFunction.c_str());
		pScriptFkt = gameLocal.program.FindFunction(m_ScriptFunction.c_str());
	}

	if(pScriptFkt)
	{
		DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Running ResponseScript\r");
		idThread *pThread = new idThread(pScriptFkt);
		int n = pThread->GetThreadNum();
		pThread->CallFunctionArgs(pScriptFkt, true, "eef", m_Owner, StimEnt, n);
		pThread->DelayedStart(0);
	}
	else
	{
		DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("ResponseActionScript not found! [%s]\r", m_ScriptFunction.c_str());
	}

	// Continue the chain if we have a followup response to be triggered.
	if(m_FollowUp != NULL)
	{
		DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Followup: %08lX\r", m_FollowUp);
		m_FollowUp->TriggerResponse(StimEnt);
	}
}


/********************************************************************/
/*                 CStimResponseTimer                               */
/********************************************************************/
CStimResponseTimer::CStimResponseTimer(void)
{
	m_Type = SRTT_SINGLESHOT;
	m_State = SRTS_DISABLED;
	m_Reload = 0;
	m_ReloadVal = 0;
	m_ReloadTimer = 0;
	m_ReloadTimerVal = 0;
	m_Apply = 0;
	m_ApplyVal = 0;
	m_Duration = 0;
	m_DurationVal = 0;
}

CStimResponseTimer::~CStimResponseTimer(void)
{
}

TimerValue CStimResponseTimer::ParseTimeString(idStr &str)
{
	TimerValue v = TIMER_UNDEFINED;
	int i, h, m, s;

	if(str.Length() == 0)
		goto Quit;

	if(str[2] != ':' && str[5]  != ':')
	{
		DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Invalid timer string [%s]\r", str.c_str());
		goto Quit;
	}

	h = m = s = 0;
	for(i = 0; i < 3; i++)
	{
		switch(i)
		{
			case 0:
			{
				h = atoi(str.c_str());
				if(!(h >= 0 && h <= 23))
				{
					DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Invalid hour string [%s]\r", str.c_str());
					goto Quit;
				}
			}
			break;

			case 1:
			{
				m = atoi(str.c_str());
				if(!(m >= 0 && m <= 59))
				{
					DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Invalid minute string [%s]\r", str.c_str());
					goto Quit;
				}
			}
			break;

			case 2:
			{
				s = atoi(str.c_str());
				if(!(s >= 0 && s <= 59))
				{
					DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Invalid minute string [%s]\r", str.c_str());
					goto Quit;
				}
			}
			break;
		}
	}

	v = SetHours(h) + SetMinutes(m) + SetSeconds(s);

Quit:
	return v;
}

