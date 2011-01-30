/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#include "../idlib/precompiled.h"
#pragma hdrstop


#include "UserManager.h"

int UserManager::GetNumUsers()
{
	return m_users.Num();
}

void UserManager::AddUser(idActor* actor)
{
	idEntityPtr<idActor> actorPtr;
	actorPtr = actor;

	int index(m_users.FindIndex(actorPtr));
	if ( index < 0 ) {
		// angua: actor is not in users list yet
		if (actor->IsType(idAI::Type))
		{
			idAI* ai = static_cast<idAI*>(actor);
			int num = m_users.Num();
			idActor* currentActor;
			// go through users list
			for (int i = 0; i < num; i++)
			{
				currentActor = m_users[i].GetEntity();
				if (currentActor->IsType(idAI::Type))
				{
					idAI* currentAI = static_cast<idAI*>(currentActor);
					if (ai->AI_AlertLevel > currentAI->AI_AlertLevel)
					{
						// the ai's alert level is higher than the ai at this position in the list, add it in front of this
						m_users.Insert(actorPtr, i);
						return;
					}
				}
			}
			// otherwise, append at the end of the list
			m_users.Append(actorPtr);
		}
	}
}

void UserManager::RemoveUser(idActor* actor)
{
	idEntityPtr<idActor> actorPtr;
	actorPtr = actor;

	int index = m_users.FindIndex(actorPtr);
	if (index >= 0)
	{
		m_users.RemoveIndex(index);
	}
}

idActor* UserManager::GetMasterUser()
{
	if (GetNumUsers() > 0)
	{
		return m_users[0].GetEntity();
	}
	return NULL;
}


idActor* UserManager::GetUserAtIndex(const int index) // grayman #2345
{
	if (GetNumUsers() > 0)
	{
		return m_users[index].GetEntity();
	}
	return NULL;
}

void UserManager::InsertUserAtIndex(idActor* actor,const int index) // grayman #2345
{
	if ((actor != NULL) && (index >= 0))
	{
		idEntityPtr<idActor> actorPtr;
		actorPtr = actor;
		m_users.Insert(actorPtr,index);
	}
}

int UserManager::GetIndex(idActor* actor) // grayman #2345
{
	idEntityPtr<idActor> actorPtr;
	actorPtr = actor;
	return (m_users.FindIndex(actorPtr));
}

void UserManager::Save(idSaveGame* savefile) const
{
	int num = m_users.Num();
	savefile->WriteInt(num);
	for (int i = 0; i < num; i++)
	{
		m_users[i].Save(savefile);
	}
}

void UserManager::Restore(idRestoreGame* savefile)
{
	int num;
	savefile->ReadInt(num);
	for (int i = 0; i < num; i++)
	{
		idEntityPtr<idActor> actor;
		actor.Restore(savefile);
		m_users.AddUnique(actor);
	}
}

