// vim:ts=4:sw=4:cindent
/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 4618 $
 * $Date: 2011-02-20 19:10:42 +0100 (Sun, 20 Feb 2011) $
 * $Author: tels $
 *
 ***************************************************************************/

// Copyright (C) 2004 Id Software, Inc.
// Copyright (C) 2011 The Dark Mod

#ifndef __GAME_TDM_EMITTER_H__
#define __GAME_TDM_EMITTER_H__

#include "misc.h"

/*
===============================================================================

idFuncEmitter

===============================================================================
*/

/** Defines additional data for emitters with more than one particle */
typedef struct {
	int						defHandle;
	idVec3					offset;
	idRenderModel *			handle;
	idStr					name;		// empty when this model equals the first model
	int						flags;		// 0 => visible, 1 => hidden
} emitter_models_t;

class idFuncEmitter : public idStaticEntity {
public:
	CLASS_PROTOTYPE( idFuncEmitter );

						idFuncEmitter( void );
						~idFuncEmitter( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );
	void				Event_Activate( idEntity *activator );

	virtual void		WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void		ReadFromSnapshot( const idBitMsgDelta &msg );

	virtual void		Think( void );
	virtual void		Present( void );

	// switch to a new model
	virtual void		SetModel( const char *modelname );

protected:

	// add an extra model
	void				SetModel( int id, const idStr &modelName, const idVec3 &offset );

private:
	bool						hidden;

	idStr						m_modelName;		//! name of the original particle model
	idList<emitter_models_t> 	m_models;			//! struct with data for additional models
};

/*
===============================================================================

idFuncSplat

===============================================================================
*/

class idFuncSplat : public idFuncEmitter {
public:
	CLASS_PROTOTYPE( idFuncSplat );

	idFuncSplat( void );

	void				Spawn( void );

private:
	void				Event_Activate( idEntity *activator );
	void				Event_Splat();
};


#endif /* !__GAME_TDM_EMITTER_H__ */

