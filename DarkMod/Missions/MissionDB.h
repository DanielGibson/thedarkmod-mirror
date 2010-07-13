/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef _MISSION_DB_H_
#define _MISSION_DB_H_

#include <map>
#include <boost/shared_ptr.hpp>
#include "MissionInfo.h"

/**
 * greebo: The mission database class holds the list of available TDM missions
 * and the corresponding data like play statistics, size info, etc.
 */
class CMissionDB
{
private:
	// Named mission info structures (fs_game => info)
	typedef std::map<std::string, CMissionInfoPtr> MissionInfoMap;
	MissionInfoMap _missionInfo;

public:
	CMissionDB();

	void Init();

	// Saves changed data to disk
	void Save();

	// Returns the mission info structure for this fs_game
	// Always returns non-NULL, if the name is not existing, 
	// a new structure will be created
	const CMissionInfoPtr& GetMissionInfo(const idStr& name);

	// Checks whether there is a record for the given mission name
	bool MissionInfoExists(const idStr& name);

private:
	void ReloadMissionInfoFile();
};
typedef boost::shared_ptr<CMissionDB> CMissionDBPtr;

#endif /* _MISSION_DB_H_ */
