/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#pragma once

#include "StimResponse/Stim.h"
#include "StimResponse/Response.h"
#include "AIComm_Message.h"

/**
* This class manifests an AI's response link to 
* a response to a AI Communications Stim
*/
class CAIComm_Response:
	public CResponse
{
public:
	CAIComm_Response(idEntity* Owner, int Type, int uniqueId);

	/**
	* This override of the base class version
	* calls a script function of a different format
	* than the default stim response, and passes
	* in the communication stim parameters.
	*/
	virtual void TriggerResponse(idEntity *Stim, CStim* stim);
};

/*------------------------------------------------------------------*/

typedef struct tagTAICommMessageNode
{
	// The message
	ai::CommMessage* p_message;

	// Node links
	tagTAICommMessageNode* p_prev;
	tagTAICommMessageNode* p_next;

} TAICommMessageNode;

/*------------------------------------------------------------------*/

/**
* This class manifests an AI's outbound communications
* stim and contains their outbound communication messages.
*/
class CAIComm_Stim: 
	public CStim
{
protected:

	// The number of comm messages in the list
	unsigned long messageCount;

	// List header and trailer pointers
	TAICommMessageNode* p_firstMessage;
	TAICommMessageNode* p_lastMessage;

public:
	/*!
	* Constructor
	*/
	CAIComm_Stim (idEntity* Owner, int Type, int uniqueId);

	/*!
	* Destructor
	*/
	virtual ~CAIComm_Stim(void);

	virtual void Save(idSaveGame *savefile) const;
	virtual void Restore(idRestoreGame *savefile);

	/*!
	* This method clears all the messages from the list and destroys
	* them.
	*/
	void clearMessages();

	/*!
	* This method adds a communication message to this stim
	*
	* @param in_commType The type of the communication message
	*
	* @param in_maximumRadiusOfResponse The maximum radius away that
	*	the message should be responded to (due to audability)
	* 
	* @param in_p_issuingEntity The entity speaking the communication
	*
	* @param in_p_recipientEntity The entity intended to hear the communication
	*
	* @param in_p_directObjectEntity The entity that the communication is about
	*
	* @param in_directObjectLocation The location that the communication is about
	*
	*/
	bool addMessage
	(
		ai::CommMessage::TCommType in_commType,
		float in_maximumRadiusOfResponse,
		idEntity* in_p_issuingEntity,
		idEntity* in_p_recipientEntity,
		idEntity* in_p_directObjectEntity,
		const idVec3& in_directObjectLocation
	);

	/**
	* Get the number of messages in the stim
	*/
	unsigned long getNumMessages();

	/**
	* Gets the first message in the stim
	*
	* @return pointer to the message
	* @return NULL if no messages
	*/
	ai::CommMessage* getFirstMessage (unsigned long& out_iterationHandle);

	/**
	* Gets the next message in the stim.
	*
	* @return pointer to the message
	* @return NULL if no more messages
	*/
	ai::CommMessage* getNextMessage (unsigned long& inout_iterationHandle);

	/**
	* BASE CLASS VIRTUAL OVERRIDE
	*
	* This version removes any messages in the list of messages as they
	* have been distributed.
	*/
	virtual void PostFired (int numResponses);


};

