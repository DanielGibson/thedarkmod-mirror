// vim:ts=4:sw=4:cindent
/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

// Copyright (C) 2010 Tels (Donated to The Dark Mod)

#ifndef __GAME_LODE_H__
#define __GAME_LODE_H__

#include "ModelGenerator.h"

/*
===============================================================================

  Lode - Level Of Detail Entity
  
  Automatically creates/culls entities based on distance from player.

  Can spawn entities based on templates in random places, or manage already
  existing entities. The rules where to spawn what are quite flexible, either
  random, with distribution functions, based on underlying texture etc.

  Inhibitors can also be placed, these inhibit spawning of either all entities
  from one class or all entitites.

  Can also combine spawned entities into "megamodels", where the model consists
  of all the surfaces of the combined models, to reduce entity count and number
  of drawcalls.

===============================================================================
*/

// Defines one material class that modulates how often entities appear on it:
struct lode_material_t {
	idStr					name;			//!< a part, like "grass", or the full name like "sand_dark"
	float					probability;	//!< 0 .. 1.0
};

// Defines one entity class
struct lode_class_t {
	idStr					classname;		//!< Entity class to respawn entities
	idList< int >			skins;			//!< index into skins array
	idRenderModel*			hModel;			//!< Used to share data between many entities with the same model
											//!< (f.i. when you turn a brush inside DR into a idStaticEntity and
											//!< use it as template)
	idStr					modelname;		//!< To load the rendermodel for combining it w/o spawning
											//!< the entity first.
	bool					pseudo;			//!< if true, this class is a pseudo-class, and describes an
											//!< entity with a megamodel (a combined model from many entities),
											//!< the model is still stored in hModel.
											//!< These classes will be skipped when recreating the entities.
	idList<model_combineinfo_t>	info;		//!< For each entity, an entry with the
											//!< where the model starts, so that we
											//!< later can remove it again. Not used yet.
	int						seed;			//!< per-class seed so each class generates the same sequence of
											//!< entities independ from the other classes, helps when the menu
   											//!> setting changes
	int						score;			//!< to find out how many entities (calculated at spawn time from score)
	idVec3					origin;			//!< origin of the original target entity, useful for "flooring"
	float					cullDist;		//!< distance after where we remove the entity from the world
	float					spawnDist;		//!< distance where we respawn the entity
	float					spacing;		//!< min. distance between entities of this class
	float					bunching;		//!< bunching threshold (0 - none, 1.0 - all)
	float					sink_min;		//!< sink into floor at minimum
	float					sink_max;		//!< sink into floor at maximum
	bool					floor;			//!< if true, the entities will be floored (on by default, use
											//!< "lode_floor" "0" to disable, then entities will be positioned
											//!< at "z" where the are in the editor
	bool					stack;			//!< if true, the entities can stack on top of each other
	bool					noinhibit;		//!< if true, the entities of this class will not be inhibited
	float					defaultProb;	//!< Probabiliy with that entity class will appear. Only used if
											//!< materialNames is not empty, and then used as the default when
											//!< no entry in this list matches the texture the entity stands on.
	idList<lode_material_t>	materials;		//!< List of material name parts that we can match against
	int						nocollide;		//!< should this entity collide with:
   											//!< 1 other auto-generated entities from the same class?
											//!< 2 other auto-generated entities (other classes)
											//!< 4 other static entities already present
											//!< 8 world geometry
	idVec3					size;			//!< size of the model for collision tests

	int						falloff;		//!< Entity random distribution method
											//!< 0 - none, 1 - cutoff, 2 - square, 3 - exp(onential), 4 - func
	float					func_x;			//!< only used when falloff == 4
	float					func_y;
	float					func_s;
	float					func_a;
	int						func_Xt;		//!< 1 => X, 2 => X*X
	int						func_Yt;		//!< 1 => X, 2 => X*X
	int						func_f;			//!< 1 => Clamp, 0 => Zeroclamp
	float					func_min;
	float					func_max;
	idStr					map;			//!< name of the image map (greyscale 8-bit TGA)
	CImage*					img;			//!< if map != "": ptr to the distribution image map
};

// Defines one area that inhibits entity spawning
struct lode_inhibitor_t {
	idVec3					origin;			// origin of the area
	idBox					box;			// oriented box of the area
};

// Defines one entity to be spawned/culled
struct lode_entity_t {
	int						skinIdx;		// index into skin list, the final skin for this entity (might be randomly choosen)
	idVec3					origin;			// (semi-random) origin
	idAngles				angles;			// zyx (yaw, pitch, roll) (semi-random) angles
	bool					hidden;			// hidden?
	bool					exists;			// false if culled
	int						entity;			// nr of the entity if exists == true
	int						classIdx;		// index into m_Classes
};

extern const idEventDef EV_Deactivate;
extern const idEventDef EV_CullAll;

class Lode : public idStaticEntity {
public:
	CLASS_PROTOTYPE( Lode );

						Lode( void );
	virtual				~Lode( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );
	virtual void		Think( void );

	/**
	* Clear the m_Classes list and also free any allocated models.
	*/
	void				ClearClasses( void );

	/**
	* Stop thinking and no longer cull/spawn entities.
	*/
	void				Event_Deactivate( idEntity *activator );
	/*
	* Cull all entities. Only useful after Deactivate().
	*/
	void				Event_CullAll( void );

	void				Event_Activate( idEntity *activator );

	/**
	* Given a pointer to a render model, calls AllocModel() on the rendermanager, then
	* copies all surface data from the old model to the new model. Used to construct a
	* copy of an existing model, so it can then be used as blue-print for other models,
	* which will share the same data. If dupData is true, memory for verts and indexes
	* is duplicated, otherwise the new model shares the data of the old model. In this
	* case the memory of the new model needs to be freed differently, of course :)
	*/
	idRenderModel*		DuplicateModel( const idRenderModel *source, const char *snapshotName, const bool dupData = true );

	/**
	* Manipulate memory of a duplicate model so that shared data does not get freed twice.
	*/
	void				FreeSharedModelData ( const idRenderModel *model );

private:

	/**
	* Look at our targets and create the entity classes. Calls PrepareEntities().
	*/
	void				Prepare( void );

	/**
	* Create the entity positions.
	*/
	void				PrepareEntities( void );

	/**
	* Combine entity models into "megamodels". Called automatically by PrepareEntities().
	*/
	void				CombineEntities( void );

	/**
	* Return a random int independedn from RandomFloat/RandomFloatSqr/RandomFloatExp, so we
	* can seed the other random generator per class.
	*/
	int					RandomSeed( void );

	/**
	* In the range 0.. 1.0, using our own m_iSeed value.
	*/
	float				RandomFloat( void );

	/**
	* Squared falloff
	*/
	float				RandomFloatSqr( void );

	/**
	* Exponential falloff
	*/
	float				RandomFloatExp( const float lambda );

	/**
	* Spawn the entity with the given index, return true if it could be spawned.
	* If managed is true, the LODE will take care of this entity for LOD changes.
	*/
	bool				SpawnEntity( const int idx, const bool managed );

	/**
	* Cull the entity with the given index, if it exists, return true if it could
	* be culled.
	*/
	bool				CullEntity( const int idx );

	/**
	* Take the given entity as template and add a class from its values. Returns
	* the floor-space-size of this entity class.
	*/
	float				AddClassFromEntity( idEntity *ent, const int iEntScore );

	/**
	* Add an entry to the skin list unless it is there already. Return the index.
	*/
	int 				AddSkin( const idStr *skin );

	/**
	* Generate a scaling factor depending on the GUI setting.
	*/
	float				LODBIAS ( void );

	/**
	* Set m_iNumEntities from spawnarg, or density, taking GUI setting into account.
	*/
	void				ComputeEntityCount( void );


	/* *********************** Members *********************/

	bool				active;

	/**
	* If true, we need still to prepare our entity list.
	*/
	bool				m_bPrepared;

	/**
	* Set to true if whether this static entity should hide
	* or change models/skins when outside a certain distance from the player.
	* If true, this entity will manage the LOD settings for entities, which
	* means these entities should have a "m_DistCheckInterval" of 0.
	**/
	bool				m_bDistDependent;

	/**
	* Current seed value for the random generator, which generates the sequence used to place
	* entities. Same seed value gives same sequence, thus same placing every time.
	**/
	int					m_iSeed;

	/**
	* Current seed value for the second random generator, which generates the sequence used
	* initialize the first generator.
	**/
	int					m_iSeed_2;

	/**
	* Seed start value for the second random generator, used to restart the sequence.
	**/
	int					m_iOrgSeed;

	/**
	* Sum of all entity scores, used to distribute the entities according to their score.
	**/
	int					m_iScore;

	/**
	* Number of entities to manage overall.
	**/
	int					m_iNumEntities;

	/**
	* Number of entities currently visible.
	**/
	int					m_iNumVisible;

	/**
	* Number of entities currently existing (visible or not).
	**/
	int					m_iNumExisting;

	/**
	* If true, the LOD distance check will only consider distance
	* orthogonal to gravity.  This can be useful for things like
	* turning on rainclouds high above the player.
	**/
	bool				m_bDistCheckXYOnly;

	/**
	* Timestamp and interval between distance checks, in milliseconds
	**/
	int					m_DistCheckTimeStamp;
	int					m_DistCheckInterval;

	/**
	* The classes of entities that we need to construct.
	**/
	idList<lode_class_t>		m_Classes;

	/**
	* The entities that inhibit us from spawning inside their area.
	**/
	idList<lode_inhibitor_t>	m_Inhibitors;

	/**
	* Info about each entitiy that we spawn or cull.
	**/
	idList<lode_entity_t>		m_Entities;

	/**
	* Names of all different skins.
	**/
	idList<idStr>				m_Skins;

	/**
	* A copy of cv_lod_bias, to detect changes during runtime.
	*/
	float						m_fLODBias;

	/**
	* Average floorspace of all classes
	*/
	float						m_fAvgSize;

	/**
	* The PVS this LODE spans - can be more than one when it crosses a visportal.
	* TODO: What if it are more than 64?
	*/
	int							m_iNumPVSAreas;
	int							m_iPVSAreas[64];

	/**
	* If we are outside the PVS area, only think every Nth time. This counter is set
	* to 0 evertime we think and increased if outside the PVS.
	*/
	int							m_iThinkCounter;

	/**
	* Debug level. Default 0. Higher values will print more debug info.
	*/
	int							m_iDebug;

	static const unsigned long	IEEE_ONE  = 0x3f800000;
	static const unsigned long	IEEE_MASK = 0x007fffff;

	static const unsigned long	NOCOLLIDE_SAME   = 0x01;
	static const unsigned long	NOCOLLIDE_OTHER  = 0x02;
	static const unsigned long	NOCOLLIDE_STATIC = 0x04;
	static const unsigned long	NOCOLLIDE_WORLD  = 0x08;
	static const unsigned long	NOCOLLIDE_ATALL  = NOCOLLIDE_WORLD + NOCOLLIDE_STATIC + NOCOLLIDE_OTHER + NOCOLLIDE_SAME;
	static const unsigned long	COLLIDE_WORLD    = NOCOLLIDE_STATIC + NOCOLLIDE_OTHER + NOCOLLIDE_SAME;
	static const unsigned long	COLLIDE_ALL  	 = 0x00;
};

#endif /* !__GAME_LODE_H__ */

