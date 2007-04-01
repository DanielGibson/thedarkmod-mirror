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

static bool init_version = FileVersionList("$Id: ResponseEffect.cpp 870 2007-03-27 14:21:59Z greebo $", init_version);

#include "ResponseEffect.h"

/********************************************************************/
/*                 CResponseEffect                                  */
/********************************************************************/

void CResponseEffect::runScript(idEntity* owner, idEntity* stimEntity, float magnitude) {
	if (_scriptFunction == NULL) return;

	DM_LOG(LC_STIM_RESPONSE, LT_DEBUG)LOGSTRING("Running ResponseEffect Script...\r");
	idThread *pThread = new idThread(_scriptFunction);
	int n = pThread->GetThreadNum();
	pThread->CallFunctionArgs(_scriptFunction, true, "eesff", owner, stimEntity, _effectPostfix.c_str(), magnitude, n);
	pThread->DelayedStart(0);
}
