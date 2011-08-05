/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

// Copyright (C) 2004 Id Software, Inc.
//

#ifndef __LANGDICT_H__
#define __LANGDICT_H__

/*
===============================================================================

	Simple dictionary specifically for the localized string tables.

===============================================================================
*/

class idLangKeyValue {
public:
	idStr					key;
	idStr					value;
};

class idLangDict {
public:
							idLangDict( void );
							~idLangDict( void );

	void					Clear( void );
	bool					Load( const char *fileName, bool clear = true );
	void					Save( const char *fileName );

	const char *			AddString( const char *str );
	const char *			GetString( const char *str, bool dowarn = true ) const;

	/**
	* Tels: Print some statistics about memory usage.
	*/
	void				Print( void ) const;
							// adds the value and key as passed (doesn't generate a "#str_xxxxx" key or ensure the key/value pair is unique)
	void					AddKeyVal( const char *key, const char *val );

	int						GetNumKeyVals( void ) const;
	const idLangKeyValue *	GetKeyVal( int i ) const;

	void					SetBaseID(int id) { baseID = id; };

private:
	idList<idLangKeyValue>	args;
	idHashIndex				hash;

	bool					ExcludeString( const char *str ) const;
	int						GetNextId( void ) const;
	int						GetHashKey( const char *str ) const;

	int						baseID;
};

#endif /* !__LANGDICT_H__ */
