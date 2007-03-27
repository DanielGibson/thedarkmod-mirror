/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 866 $
 * $Date: 2007-03-23 22:25:02 +0100 (Fr, 23 M�r 2007) $
 * $Author: greebo $
 *
 ***************************************************************************/
#ifndef SR_RESPONSEEFFECT__H
#define SR_RESPONSEEFFECT__H

class CResponseEffect
{
	const idDict* _effectEntityDef;

	const function_t* _scriptFunction;

	// The effect postfix, "1_1" for example
	// This is passed to the script along with the "owner" entity,
	// so that the script can lookup any arguments it might need.
	idStr _effectPostfix;

public:
	// Pass the effect entity to this structure
	CResponseEffect(const idDict* effectEntityDef, 
					const function_t* scriptFunction,
					const idStr& effectPostfix) :
		_effectEntityDef(effectEntityDef),
		_scriptFunction(scriptFunction),
		_effectPostfix(effectPostfix)
	{}

	/**
	* Runs the attached response effect script 
	* (does nothing if the scriptfunc pointer is NULL)
	*
	* @owner: The entity this script is affecting
	* @stimEntity: The entity that triggered this response
	*/
	void runScript(idEntity* owner, idEntity* stimEntity);
};

#endif /* SR_RESPONSEEFFECT__H */
