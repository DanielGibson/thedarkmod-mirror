/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 2167 $
 * $Date: 2008-04-06 20:41:22 +0200 (So, 06 Apr 2008) $
 * $Author: greebo $
 *
 ***************************************************************************/

#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id: MultiStateMoverPosition.cpp 2167 2008-04-06 18:41:22Z greebo $", init_version);

#include "MultiStateMoverPosition.h"
#include "MultiStateMover.h"

CLASS_DECLARATION( idEntity, CMultiStateMoverPosition )
	EVENT( EV_PostSpawn,	CMultiStateMoverPosition::Event_PostSpawn )
END_CLASS

void CMultiStateMoverPosition::Spawn() 
{
	PostEventMS(&EV_PostSpawn, 4);
}

void CMultiStateMoverPosition::Event_PostSpawn()
{
	// Find all AAS obstacle entities among the targets
	for (int i = 0; i < targets.Num(); i++)
	{
		idEntity* target = targets[i].GetEntity();

		if (target != NULL && target->IsType(idFuncAASObstacle::Type))
		{
			// Allocate a new list element and call the operator=
			aasObstacleEntities.Alloc() = static_cast<idFuncAASObstacle*>(target);
		}
	}

	// Remove all AAS obstacle entities from our targets, they should not be blindly triggered
	for (int i = 0; i < aasObstacleEntities.Num(); i++)
	{
		RemoveTarget(aasObstacleEntities[i].GetEntity());
	}
}

void CMultiStateMoverPosition::Save(idSaveGame *savefile) const
{
	savefile->WriteInt(aasObstacleEntities.Num());
	for (int i = 0; i < aasObstacleEntities.Num(); i++)
	{
		aasObstacleEntities[i].Save(savefile);
	}
}

void CMultiStateMoverPosition::Restore(idRestoreGame *savefile)
{
	int num;
	savefile->ReadInt(num);
	aasObstacleEntities.SetNum(num);
	for (int i = 0; i < num; i++)
	{
		aasObstacleEntities[i].Restore(savefile);
	}
}

void CMultiStateMoverPosition::OnMultistateMoverArrive(CMultiStateMover* mover)
{
	if (mover == NULL) return;

	// First, check if we should trigger our targets
	if (spawnArgs.GetBool("always_trigger_targets", "1"))
	{
		ActivateTargets(mover);

		// Tell the idFuncAASObstacles to (re-)activate the AAS areas
		for (int i = 0; i < aasObstacleEntities.Num(); i++)
		{
			aasObstacleEntities[i].GetEntity()->SetAASState(true);
		}
	}

	// Run the mover event script
	RunMoverEventScript("call_on_arrive", mover);
}

void CMultiStateMoverPosition::OnMultistateMoverLeave(CMultiStateMover* mover)
{
	if (mover == NULL) return;

	// First, check if we should trigger our targets
	if (spawnArgs.GetBool("always_trigger_targets", "1"))
	{
		ActivateTargets(mover);

		// Handle the idFuncAASObstacles separately, tell them to deactivate the AAS areas
		for (int i = 0; i < aasObstacleEntities.Num(); i++)
		{
			aasObstacleEntities[i].GetEntity()->SetAASState(false);
		}
	}

	// Run the mover event script
	RunMoverEventScript("call_on_leave", mover);
}

void CMultiStateMoverPosition::RunMoverEventScript(const idStr& spawnArg, CMultiStateMover* mover)
{
	idStr scriptFuncName;
	if (!spawnArgs.GetString(spawnArg, "", scriptFuncName))
	{
		return; // no scriptfunction
	}

	// Script function signature is like this: void scriptobj::onMultiStateMover(entity mover)
	idThread* thread = CallScriptFunctionArgs(scriptFuncName, true, 0, "ee", this, mover);
	if (thread != NULL)
	{
		// greebo: Run the thread at once, the script result might be needed below.
		thread->Execute();
	}
}
