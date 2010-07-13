/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef _MISSION_MANAGER_H_
#define _MISSION_MANAGER_H_

#include "MissionInfo.h"
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

class CMissionDB;
typedef boost::shared_ptr<CMissionDB> CMissionDBPtr;

namespace fs = boost::filesystem;

struct DownloadableMission
{
	idStr modName;
	idStr title;
	idStr author;
	float sizeMB;
	idStr releaseDate;
	idStr language;
	int version;

	idStringList downloadLocations;
};
typedef idList<DownloadableMission> DownloadableMissionList;

class CMissionManager
{
private:
	CMissionDBPtr _missionDB;

	// A plain list of available fs_game names
	idStringList _availableMissions;

	// A list of path => path associations for moving files around
	typedef std::list< std::pair<fs::path, fs::path> > MoveList;

	// The list of new mods
	idStringList _newFoundMissions;

	// The map which should be the starting point
	idStr _curStartingMap;

	DownloadableMissionList _downloadableMissions;

public:
	enum InstallResult
	{
		INSTALLED_OK,
		INDEX_OUT_OF_BOUNDS,
		COPY_FAILURE,
	};

public:
	CMissionManager();

	// This initialises the list of available missions
	void Init();

	// Should be called when the game is shutting down
	void Shutdown();

	// Returns the number of available missions
	int GetNumMissions();

	// Returns the mission info by index (or NULL if out of bounds)
	CMissionInfoPtr GetMissionInfo(int index);

	// returns the mission info by name (always non-NULL)
	CMissionInfoPtr GetMissionInfo(const idStr& name);

	// Returns the info structure for the currently ongoing mod/mission (or NULL if none)
	CMissionInfoPtr GetCurrentMissionInfo();

	// Returns the name of the currently installed mod/mission
	idStr GetCurrentMissionName();

	void EraseModFolder(const idStr& name);

	// Called by MissionData when the player completed a mission
	void OnMissionComplete();

	// Called by gameLocal when the player start/loads a mission
	void OnMissionStart();

	// Clears the mission list and searches for mods to install, then calls GenerateMissionList()
	void ReloadMissionList();

	// The number of newly available missions
	int GetNumNewMissions();

	idStr GetNewFoundMissionsText();

	void ClearNewMissionList();

	// Installs mission (by index)
	InstallResult InstallMission(int index);

	// Installs mission (by fs_game name)
	InstallResult InstallMission(const idStr& name);

	// Uninstalls the currently installed FM, basically clearing our currentfm.txt
	void UninstallMission();

	// Checks online for available missions
	void ReloadDownloadableMissions();

	// Accessor to the downloadble mission list
	const DownloadableMissionList& GetDownloadableMissions() const;

	// Convenience method which copies a file from <source> to <dest>
	// If <overwrite> is set to TRUE, any existing destination file will be removed beforehand
	// Note: CopyFile is already #define'd in a stupid WinBase.h header file, hence DoCopyFile.
	static bool DoCopyFile(const fs::path& source, const fs::path& dest, bool overwrite = false);

	// Removes the given file, returns TRUE if this succeeded or if file wasn't there in the first place, FALSE on error
	static bool DoRemoveFile(const fs::path& fileToRemove);

	// Moves the given file, from <fromPath> to <toPath>
	static bool DoMoveFile(const fs::path& fromPath, const fs::path& toPath);

private:
	// Finds out which map is the starting map (must be called after InitCurrentMod)
	// After this call the CVAR tdm_mapName is initialised and holds the map name.
	void InitStartingMap();

	void SearchForNewMissions();

	// Sub-routine of SearchForNewMissions() investigating the FM folder
	// using the given extension (including dot ".pk4", ".zip")
	MoveList SearchForNewMissions(const idStr& extension);

	// Returns the path to the "darkmod" base
	fs::path GetDarkmodPath();

	// Finds all available missions
	void GenerateMissionList();

	// Sorts all missions by display name
	void SortMissionList();

	// Compare functor to sort mods by display name
	static int MissionSortCompare(const int* a, const int* b);
};
typedef boost::shared_ptr<CMissionManager> CMissionManagerPtr;

#endif /* _MISSION_MANAGER_H_ */
