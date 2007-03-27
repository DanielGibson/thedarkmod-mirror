/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 866 $
 * $Date: 2007-03-23 22:25:02 +0100 (Fr, 23 M�r 2007) $
 * $Author: greebo $
 *
 ***************************************************************************/
#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id: Response.cpp 870 2007-03-27 14:21:59Z greebo $", init_version);

#include "Response.h"

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
	// Remove all the allocated response effects from the heap
	for (int i = 0; i < m_ResponseEffects.Num(); i++)
		delete m_ResponseEffects[i];
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

	DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Cycling through Response Effects\r");
	DM_LOG(LC_STIM_RESPONSE, LT_ERROR)LOGSTRING("Available Response Effects: %u\r", m_ResponseEffects.Num());
	for (int i = 0; i < m_ResponseEffects.Num(); i++) {
		m_ResponseEffects[i]->runScript(m_Owner, StimEnt);
	}

	// Continue the chain if we have a followup response to be triggered.
	if(m_FollowUp != NULL)
	{
		DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Followup: %08lX\r", m_FollowUp);
		m_FollowUp->TriggerResponse(StimEnt);
	}
}

void CResponse::SetResponseAction(idStr const &action)
{
	m_ScriptFunction = action;
}

CResponseEffect* CResponse::addResponseEffect(const idStr& effectEntityDef, const idStr& effectPostfix)
{
	CResponseEffect* returnValue = NULL;
	
	DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("\nSeeking EffectEntity [%s]\n", effectEntityDef.c_str());
	// Try to locate the specified entity definition
	const idDict* dict = gameLocal.FindEntityDefDict(effectEntityDef.c_str());
	if (dict != NULL) 
	{
		gameLocal.Printf("EffectEntityDef found, looking for script!\r");
		
		idStr scriptStr = dict->GetString("script");
		gameLocal.Printf("Script value is %s.\n", scriptStr.c_str());

		const function_t* scriptFunc = gameLocal.program.FindFunction(scriptStr.c_str());
		if (scriptFunc != NULL)
		{
			DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Script Function found: [%s]\r", scriptStr.c_str());
		}
		// Allocate a new effect object
		CResponseEffect* newEffect = new CResponseEffect(dict, scriptFunc, effectPostfix);
		
		// Add the item to the list
		m_ResponseEffects.Append(newEffect);

		returnValue = newEffect;
	}
	else
	{
		// Entity not found, emit a warning
		gameLocal.Printf("Warning: EffectEntityDef not found: %s.\r", effectEntityDef.c_str());
	}

	DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Items in the list: %u\r", m_ResponseEffects.Num());
	
	return returnValue;
}
