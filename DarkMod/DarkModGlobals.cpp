/******************************************************************************/
/*                                                                            */
/*         DarkModGlobals (C) by Gerhard W. Gruber in Germany 2004            */
/*                          All rights reserved                               */
/*                                                                            */
/******************************************************************************/

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

#pragma warning(disable : 4996 4800)

static bool init_version = FileVersionList("$Id$", init_version);

#ifdef _WINDOWS_
#include "c:\compiled.h"
#endif

#include "DarkModGlobals.h"
#include "PlayerData.h"
#include "MissionData.h"
#include "Misc.h"
#include "Profile.h"
//#include "direct.h" [OrbWeaver] does not exist?
#include "il/il.h"
#include "sndPropLoader.h"
#include "sndProp.h"
#include "Relations.h"
#include "shop.h"
#include "../game/ai/ai.h"
#include "sourcehook/sourcehook.h"
#include "sourcehook/sourcehook_impl.h"

// Default length of time for holding down jump key to start
// mantling.
#define DARKMOD_JUMP_HOLD_MANTLE_TRIGGER_MILLISECONDS 100.0f

// Default time values for phases of mantling
#define DARKMOD_MANTLE_MILLISECONDS_HANG		750.0f
#define DARKMOD_MANTLE_MILLISECONDS_PULL		750.0f
#define DARKMOD_MANTLE_MILLISECONDS_SHIFTHANDS	500.0f
#define DARKMOD_MANTLE_MILLISECONDS_PUSH		800.0f

// The default max light quotient for hiding spots
#define MAX_HIDING_SPOT_MAX_LIGHT_QUOTIENT 2.0

// Default damage scale for mantling at high velocities
// The 15.0 m/s minimum limit is based on OCEA guidance (United States labor laws)
#define DARKMOD_MINIMUM_METERS_PER_SECOND_FOR_MANTLING_DAMAGE 15.0f

// TODO: The 0.5 damager/m/s scale is completely made up based on a scarce
// few tests and should be tweaked for gameplay
#define DARKMOD_POINTS_DAMAGE_PER_METERS_PER_SECOND_OVER_MINIMUM_VELOCITY 0.5f

// Default time value for phases of leaning
#define DARKMOD_NUM_MILLISECONDS_FOR_LEAN_MOVE 600.0f

// Default lean angle
#define DARKMOD_MAX_LEAN_TILT_DEGREES 12.0f

// Default readius of the AI COMMUNICATIONS STIM
#define DEFAULT_DARKMOD_AI_COMMUNICATIONS_STIM_RADIUS 5000.0

// Darkmod 
#define DEFAULT_MAX_NUM_HIDING_SPOT_TESTS_PER_AI_FRAME 10.0

class idAI;

// Name of the logfile to use for Dark Mod logging
#ifdef __linux__
const char* DARKMOD_LOGFILE = "/tmp/DarkMod.log";
#else
const char* DARKMOD_LOGFILE = "c:\\d3modlogger.log";
#endif

static char *LTString[LT_COUNT+1] = {
	"INI",
	"FRC",
	"ERR",
	"BEG",
	"END",
	"WAR",
	"INF",
	"DEB",
	"---"
};

static char *LCString[LC_COUNT+1] = {
	"INIT",
	"FORCE",
	"MISC",
	"SYSTEM",
	"FROBBING",
	"AI",
	"SOUND",
	"FUNCTION",
	"ENTITY",
	"INVENTORY",
	"LIGHT",
	"WEAPON",
	"MATH",
	"MOVEMENT",
	"LOCKPICK",
	"FRAME",
	"STIMRESP",
	"(empty)"
};

SourceHook::CSourceHookImpl g_SourceHook;
SourceHook::ISourceHook *g_SHPtr = NULL;
int g_PLID = 0;
const char *DM_OSPathToRelativePath(const char *OSPath);
const char *DM_RelativePathToOSPath(const char *relativePath, const char *basePath = "fs_devpath");
const char *DM_BuildOSPath(const char *base, const char *game, const char *relativePath);

// Intercept declarations
//SH_DECL_HOOK1(idFileSystem, OSPathToRelativePath, SH_NOATTRIB, 0, const char *, const char *);
//SH_DECL_HOOK2(idFileSystem, RelativePathToOSPath, SH_NOATTRIB, 0, const char *, const char *, const char *);
SH_DECL_HOOK3(idFileSystem, BuildOSPath, SH_NOATTRIB, 0, const char *, const char *, const char *, const char *);

// declare various global objects
CsndPropLoader	g_SoundPropLoader;
CsndProp		g_SoundProp;
CRelations		g_globalRelations;
CMissionData	g_MissionData;
CShop			g_Shop;

static idList<const char *> *s_FileVersion = NULL;

bool FileVersionList(const char *str, bool state)
{
	if(s_FileVersion == NULL)
		s_FileVersion = new idList<const char *>;

	if(state == false)
		s_FileVersion->AddUnique(str);

	return true;
}

void FileVersionDump(void)
{
	int i, n;

	n = s_FileVersion->Num();
	for(i = 0; i < n; i++)
		DM_LOG(LC_INIT, LT_INIT)LOGSTRING("%s\r", (*s_FileVersion)[i]);
}


CGlobal::CGlobal(void)
{
	m_DarkModPlayer = new CDarkModPlayer;

	memset(m_LogArray, 0, sizeof(m_LogArray));
	memset(m_ClassArray, 0, sizeof(m_ClassArray));

	memset(m_ModPath, 0, sizeof(m_ModPath));
	memset(m_ModName, 0, sizeof(m_ModName));

	m_LogArray[LT_INIT] = true;			// This is always on
	m_LogArray[LT_FORCE] = true;			// This is always on
	m_LogArray[LT_ERROR] = false;
	m_LogArray[LT_BEGIN] = false;
	m_LogArray[LT_END] = false;
	m_LogArray[LT_DEBUG] = false;

	m_ClassArray[LC_INIT] = true;
	m_ClassArray[LC_FORCE] = true;			// This is always on
	m_ClassArray[LC_SYSTEM] = false;
	m_ClassArray[LC_FROBBING] = false;
	m_ClassArray[LC_AI] = false;
	m_ClassArray[LC_SOUND] = false;
	m_ClassArray[LC_FUNCTION] = false;
	m_ClassArray[LC_MOVEMENT] = false;

	m_Frame = 0;
	m_DefaultFrobDistance = 100.0f;
	m_MaxFrobDistance = 0;
	m_LogClass = LC_SYSTEM;
	m_LogType = LT_DEBUG;
	m_Filename = "undefined";
	m_Linenumber = 0;
	m_WeakLightgem = false;
	m_AICommStimRadius = DEFAULT_DARKMOD_AI_COMMUNICATIONS_STIM_RADIUS;
	m_maxNumHidingSpotPointTestsPerAIFrame = int(DEFAULT_MAX_NUM_HIDING_SPOT_TESTS_PER_AI_FRAME);
	m_hidingSpotMaxLightQuotient = MAX_HIDING_SPOT_MAX_LIGHT_QUOTIENT;
	
	m_LogFile = NULL;

	if((m_LogFile = fopen(DARKMOD_LOGFILE, "w+b")) != NULL)
		DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Initialzing mod logging\r");

	// initialize the AI Acuities hash

/**
* Define AI Acuities Here:
* NOTE: If you add an acuity, your total number of acuities
* must be below s_MAXACUITIES defined in AI.h, unless you
* want to chagne that and recompile everything.
**/
	m_AcuityNames.Append("vis");
	m_AcuityNames.Append("aud");
	m_AcuityNames.Append("tact");
	m_AcuityNames.Append("env");
	m_AcuityNames.Append("other");

	m_AcuityNames.Condense();

	for( int i=0; i < m_AcuityNames.Num(); i++ )
	{
		m_AcuityHash.Add( m_AcuityHash.GenerateKey( m_AcuityNames[i].c_str(), false ), i );
	}

	//*******
	// Initialize the Mantling and Leaning variables
	//*******
	m_jumpHoldMantleTrigger_Milliseconds = DARKMOD_JUMP_HOLD_MANTLE_TRIGGER_MILLISECONDS;

	// Default time values for phases of mantling
	m_mantleHang_Milliseconds = DARKMOD_MANTLE_MILLISECONDS_HANG;
	m_mantlePull_Milliseconds = DARKMOD_MANTLE_MILLISECONDS_PULL;
	m_mantleShiftHands_Milliseconds = DARKMOD_MANTLE_MILLISECONDS_SHIFTHANDS;
	m_mantlePush_Milliseconds = DARKMOD_MANTLE_MILLISECONDS_PUSH;

	// Default time value for leaning
	m_leanMove_Milliseconds = DARKMOD_NUM_MILLISECONDS_FOR_LEAN_MOVE;

	// Default angle for leaning
	m_leanMove_DegreesTilt = DARKMOD_MAX_LEAN_TILT_DEGREES;

	// Default minimum velocity for mantling damage and damage scale
	m_minimumVelocityForMantleDamage = DARKMOD_MINIMUM_METERS_PER_SECOND_FOR_MANTLING_DAMAGE;
	m_damagePointsPerMetersPerSecondOverMinimum = DARKMOD_POINTS_DAMAGE_PER_METERS_PER_SECOND_OVER_MINIMUM_VELOCITY;

	/* initialize Sourcehook required global */
	g_SHPtr = static_cast<SourceHook::ISourceHook*>(&g_SourceHook); 
}

CGlobal::~CGlobal(void)
{
	if(m_LogFile != NULL)
		fclose(m_LogFile);
}

void CGlobal::GetModName()
{
	int i, n;
	char PathSep, *p;

	// Get the mod path from the filesystem
	assert(fileSystem != NULL);
	const char *modpath = fileSystem->RelativePathToOSPath(".");
	char name[256];

	DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Modpath: %08lx - [%s]\r", modpath, modpath);

#ifdef _WINDOWS_
		PathSep = '\\';
#else
		PathSep = '/';
#endif

	strcpy(m_ModPath, modpath);
	n = strlen(m_ModPath)-1;
	if(n <= 0)
		goto Quit;

	// First we cut of the path
	for(i = n; i >= 0; i--)
	{
		if(m_ModPath[i] == '.')
		{
			m_ModPath[i] = 0;
			continue;
		}

		if(m_ModPath[i] == PathSep)
		{
			m_ModPath[i] = 0;
			break;
		}
	}

	n = strlen(m_ModPath)-1;
	if(n <= 0)
		goto Quit;

	memset(name, 0, sizeof(name));
	p = name;
	for(i = n; i >= 0; i--)
	{
		if(m_ModPath[i] == PathSep)
			break;

		*p = m_ModPath[i];
		p++;
	}

	n = strlen(name)-1;
	memset(m_ModName, 0, sizeof(m_ModName));
	p = m_ModName;
	for(i = n; i >= 0; i--)
	{
		*p = name[i];
		p++;
	}

Quit:

	DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Modpath: [%s]\r", m_ModPath);
	DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Modname: [%s]\r", m_ModName);
	return;
}

void CGlobal::Init()
{
	PROFILE_HANDLE *pfh = NULL;

#ifdef _WINDOWS_

	SH_ADD_HOOK_STATICFUNC(idFileSystem, BuildOSPath, fileSystem, DM_BuildOSPath, 0);
//	SH_ADD_HOOK_STATICFUNC(idFileSystem, OSPathToRelativePath, fileSystem, DM_OSPathToRelativePath, 0);
//	SH_ADD_HOOK_STATICFUNC(idFileSystem, RelativePathToOSPath, fileSystem, DM_RelativePathToOSPath, 0);

#endif

	GetModName();

#ifdef _WINDOWS_

	std::string iniPath = 
		std::string(m_ModPath) + "\\" + std::string(m_ModName) + ".ini";

#else   // LINUX
	
	std::string iniPath = 
		std::string(getenv("HOME")) + "/.doom3/darkmod/darkmod.ini";
	
#endif

	const char* profilePath = iniPath.c_str();
	DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Trying to open %s\r", profilePath);
	if((pfh = OpenProfile(profilePath, TRUE, FALSE)) == NULL)
	{
		DM_LOG(LC_INIT, LT_INIT)LOGSTRING("%s.ini not found at %s\r", m_ModName, profilePath);
	}

	if(pfh != NULL)
		LoadINISettings(pfh);
	else
		DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Unable to open %s.ini\r", m_ModName);

	CloseProfile(pfh);
	FileVersionDump();
}

void CGlobal::LogPlane(idStr const &Name, idPlane const &Plane)
{
	float a, b, c, d;

	Plane.GetPlaneParams(a, b, c, d);
	LogString("Plane %s:    a: %f   b: %f   c: %f   d: %f\r", Name.c_str(), a, b, c, d);
}

void CGlobal::LogVector(idStr const &Name, idVec3 const &Vector)
{
	LogString("Vector %s:    x: %f   y: %f   z: %f\r", Name.c_str(), Vector.x, Vector.y, Vector.z);
}

void CGlobal::LogMat3(idStr const &Name, idMat3 const &Mat)
{
	idVec3 a, b, c;

	Mat.GetMat3Params(a, b, c);
	LogString("Matrix %s:\r\t%f  %f  %f\r\t%f  %f  %f\r\t%f  %f  %f\r", Name.c_str(), 
		a.x, a.y, a.z,
		b.x, b.y, b.z,
		c.x, c.y, c.z
		);
}

void CGlobal::LogString(char *fmt, ...)
{
	if(m_LogFile == NULL)
		return;

	LC_LogClass lc = m_LogClass;
	LT_LogType lt = m_LogType;

	if(m_ClassArray[lc] == false)
		return;

	if(m_LogArray[lt] == false)
		return;

	va_list arg;
	va_start(arg, fmt);

	fprintf(m_LogFile, "[%s (%4u):%s (%s) FR: %4lu] ", m_Filename, m_Linenumber, LTString[lt], LCString[lc], m_Frame);
	vfprintf(m_LogFile, fmt, arg);
	fprintf(m_LogFile, "\n");
	fflush(m_LogFile);

	va_end(arg);
}

void CGlobal::LoadINISettings(void *p)
{
	PROFILE_HANDLE *pfh = (PROFILE_HANDLE *)p;
	PROFILE_SECTION *ps;
	PROFILE_MAP *pm;
	FILE *logfile;
	bool Frame = false;

	DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Loading INI settings\r");

	// All logclasses are loaded from the INI file. Frame can be switched 
	// on explicitly. If any of the other classes are enabled, then Frame 
	// will also be enabled as a marker in the logfile.
	if(FindSection(pfh, "Debug", &ps) != static_cast<ULONG>(-1))
	{
		if(FindMap(ps, "LogFile", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			struct tm *t;
			time_t timer;

			timer = time(NULL);
			t = localtime(&timer);

			if((logfile = fopen(pm->Value, "w+b")) != NULL)
			{
				DM_LOG(LC_INIT, LT_INIT)LOGSTRING("Switching logfile to [%s].\r", pm->Value);
				if(m_LogFile != NULL)
				{
					fclose(m_LogFile);
					m_LogFile = logfile;
				}

				DM_LOG(LC_INIT, LT_INIT)LOGSTRING("LogFile created at %04u.%02u.%02u %02u:%02u:%02u\r",
							t->tm_year+1900, t->tm_mon, t->tm_mday, 
							t->tm_hour, t->tm_min, t->tm_sec);
				DM_LOG(LC_INIT, LT_INIT)LOGSTRING("DLL compiled on " __DATE__ " " __TIME__ "\r\r");
			}
		}

		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Found Debug section \r");

		if(FindMap(ps, "LogError", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
				m_LogArray[LT_ERROR] = true;

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogError: %c\r", pm->Value[0]);
		}

		if(FindMap(ps, "LogBegin", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
				m_LogArray[LT_BEGIN] = true;

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogBegin: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogEnd", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
				m_LogArray[LT_END] = true;

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogEnd: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogDebug", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
				m_LogArray[LT_DEBUG] = true;

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogDebug: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogWarning", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
				m_LogArray[LT_WARNING] = true;

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogWarning: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogInfo", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
				m_LogArray[LT_INFO] = true;

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogInfo: %c\r", pm->Value[0]);
		}

		if(FindMap(ps, "LogClass_FRAME", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_FRAME] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_FRAME: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_SYSTEM", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_SYSTEM] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_SYSTEM: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_MISC", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_MISC] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_MISC: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_FROBBING", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_FROBBING] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_FROBBING: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_AI", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_AI] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_AI: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_SOUND", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_SOUND] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_SOUND: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_FUNCTION", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_FUNCTION] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_FUNCTION: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_INVENTORY", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_INVENTORY] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_INVENTORY: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_LIGHT", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_LIGHT] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_LIGHT: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_WEAPON", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_WEAPON] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_WEAPON: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_MATH", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_MATH] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_MATH: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_MOVEMENT", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_MOVEMENT] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_MOVEMENT: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_STIM_RESPONSE", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_STIM_RESPONSE] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_STIM_RESPONSE: %c\r", pm->Value[0]);
		}
		if(FindMap(ps, "LogClass_LOCKPICK", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			if(pm->Value[0] == '1')
			{
				m_ClassArray[LC_LOCKPICK] = true;
				Frame = true;
			}

			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("LogClass_LOCKPICK: %c\r", pm->Value[0]);
		}

	}
//	m_ClassArray[LC_FRAME] = Frame;


	if(FindSection(pfh, "GlobalParams", &ps) != static_cast<ULONG>(-1))
	{
		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Found GlobalParams section \r");


		if(FindMap(ps, "DefaultFrobDistance", TRUE, &pm) != static_cast<ULONG>(-1))
			m_DefaultFrobDistance = fabs(atof(pm->Value));

		if(FindMap(ps, "Mantle_JumpHoldMilliseconds", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_jumpHoldMantleTrigger_Milliseconds = atof(pm->Value);
		}

		if(FindMap(ps, "Mantle_HangMilliseconds", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_mantleHang_Milliseconds = atof(pm->Value);
		}

		if(FindMap(ps, "Mantle_PullMilliseconds", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_mantlePull_Milliseconds = atof(pm->Value);
		}

		if(FindMap(ps, "Mantle_ShiftHandsMilliseconds", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_mantleShiftHands_Milliseconds = atof(pm->Value);
		}

		if(FindMap(ps, "Mantle_PushMilliseconds", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_mantlePush_Milliseconds = atof(pm->Value);
		}
				
		if(FindMap(ps, "Mantle_PushMilliseconds", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_mantlePush_Milliseconds = atof(pm->Value);
		}

		if(FindMap(ps, "Mantle_MinimumMetersPerSecondForDamage", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_minimumVelocityForMantleDamage = atof(pm->Value);
		}

		if (FindMap(ps, "Mantle_DamagerPerMetersPerSecondOverMinimum", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_damagePointsPerMetersPerSecondOverMinimum = atof(pm->Value);
		}

		if (FindMap(ps, "Lean_Milliseconds", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_leanMove_Milliseconds = atof(pm->Value);
		}

		if (FindMap(ps, "Lean_Degrees", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_leanMove_DegreesTilt = atof(pm->Value);
		}

		if (FindMap(ps, "WeakLightgem", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_WeakLightgem = atof(pm->Value);
		}

		if (FindMap(ps, "WeakLightgem", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_WeakLightgem = atof(pm->Value);
		}

		if (FindMap(ps, "AICommStimRadius", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_AICommStimRadius = atof(pm->Value);
		}

		if (FindMap (ps, "maxHidingSpotTestsPerAIFrame", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_maxNumHidingSpotPointTestsPerAIFrame = atoi(pm->Value);
			if (m_maxNumHidingSpotPointTestsPerAIFrame < 10)
			{
				DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("maxHidingSpotTestsPerAIFrame cannot be less than 10");
				m_maxNumHidingSpotPointTestsPerAIFrame = 10;
			}
		}

		if (FindMap (ps, "hidingSpotMaxLightQuotient", TRUE, &pm) != static_cast<ULONG>(-1))
		{
			m_hidingSpotMaxLightQuotient = atof(pm->Value);
			DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("m_hidingSpotMaxLightQuotient set to %f", m_hidingSpotMaxLightQuotient);
		}


		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("FrobDistance: %f\r", m_DefaultFrobDistance);

		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Jump hold mantle milliseconds: %f\r", m_jumpHoldMantleTrigger_Milliseconds);
		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Mantle hang milliseconds: %f\r", m_mantleHang_Milliseconds);
		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Mantle pull milliseconds: %f\r", m_mantlePull_Milliseconds);
		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Mantle shift hands milliseconds: %f\r", m_mantleShiftHands_Milliseconds);
		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Mantle push milliseconds: %f\r", m_mantlePush_Milliseconds);

		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Lean milliseconds: %f\r", m_leanMove_Milliseconds);
		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("Lean degrees tilt: %f\r", m_leanMove_DegreesTilt);

		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("AICommStimRadius: %f\r", m_AICommStimRadius);
		DM_LOG(LC_FORCE, LT_FORCE)LOGSTRING("maxHidingSpotTestsPerAIFrame: %d\r", m_maxNumHidingSpotPointTestsPerAIFrame);

	}
}


CLightMaterial *CGlobal::GetMaterial(idStr const &mn)
{
	CLightMaterial *rc = NULL;
	int i, n;

	n = m_LightMaterial.Num();
	for(i = 0; i < n; i++)
	{
		if(m_LightMaterial[i]->m_MaterialName.Icmp(mn) == 0)
		{
			rc = m_LightMaterial[i];
			break;
		}
	}

	DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("GetFallOffTexture returns: [%s] for [%s]\r", (rc == NULL) ? "(null)" : rc->m_MaterialName.c_str(), mn.c_str());
	return rc;
}


int CGlobal::AddImage(idStr const &Name, bool &Added)
{
	int rc = -1;
	CImage *im;
	Added = false;

	if(Name.Length() == 0)
		goto Quit;

	// If the image is already in the list, we have now the
	// index and can immediately return.
	if(GetImage(Name, rc) != NULL)
		goto Quit;

	im = new CImage(Name);

	m_Image.Append(im);
	rc = m_Image.Num()-1;
	Added = true;

Quit:
	return rc;
}

CImage *CGlobal::GetImage(int i)
{
	if(i > m_Image.Num())
		return NULL;
	else
		return m_Image[i];
}

CImage *CGlobal::GetImage(idStr const &Name, int &Index)
{
	int i, n;

	Index = -1;

	n = m_Image.Num();
	for(i = 0; i < n; i++)
	{
		if(m_Image[i]->m_Name.Icmp(Name) == 0)
		{
			Index = i;
			return m_Image[i];
		}
	}

	return NULL;
}


CLightMaterial::CLightMaterial(idStr const &MaterialName, idStr const &TextureName, idStr const &MapName)
{
	bool added;

	m_MaterialName = MaterialName;
	m_FallOffTexture = TextureName;
	m_Map = MapName;
	m_AmbientLight = false;

	m_FallOffIndex = g_Global.AddImage(TextureName, added);
	m_MapIndex = g_Global.AddImage(MapName, added);
}

CLightMaterial::~CLightMaterial()
{
}

unsigned char *CLightMaterial::GetFallOffTexture(int &Width, int &Height, int &Bpp)
{
	unsigned char *rc = NULL;
	CImage *im;

	if(m_FallOffIndex != -1)
	{
		if((im = g_Global.GetImage(m_FallOffIndex)) != NULL)
		{
			DM_LOG(LC_SYSTEM, LT_DEBUG)LOGSTRING("Falloff [%s]\r", im->m_Name.c_str());
			rc = im->GetImage();
			Width = im->m_Width;
			Height = im->m_Height;
			Bpp = im->m_Bpp;
		}
	}

	return(rc);
}

unsigned char *CLightMaterial::GetImage(int &Width, int &Height, int &Bpp)
{
	unsigned char *rc = NULL;
	CImage *im;

	if(m_MapIndex != -1)
	{
		if((im = g_Global.GetImage(m_MapIndex)) != NULL)
		{
			DM_LOG(LC_SYSTEM, LT_DEBUG)LOGSTRING("Image [%s]\r", im->m_Name.c_str());
			rc = im->GetImage();
			Width = im->m_Width;
			Height = im->m_Height;
			Bpp = im->m_Bpp;
		}
	}

	return(rc);
}

CImage::CImage(idStr const &Name)
{
	m_Name = Name;
	m_Image = NULL;
	m_BufferLength = 0L;
	m_ImageId = (ILuint)-1;
	m_Width = 0;
	m_Height = 0;
	m_Loaded = false;
	m_Bpp = 0;
}

CImage::CImage(void)
{
	m_Image = NULL;
	m_BufferLength = 0L;
	m_ImageId = (ILuint)-1;
	m_Width = 0;
	m_Height = 0;
	m_Loaded = false;
	m_Bpp = 0;
}

CImage::~CImage(void)
{
	Unload(true);
}

void CImage::Unload(bool FreeMemory)
{
	m_Loaded = false;
	if(FreeMemory == true)
	{
		if(m_Image != NULL)
			delete [] m_Image;

		m_Image = NULL;
	}

	if(m_ImageId != static_cast<unsigned char>(-1))
		ilDeleteImages(1, &m_ImageId);

	m_ImageId = (ILuint)-1;
}

#ifndef __linux__

bool CImage::LoadImage(HANDLE &Handle)
{
	bool rc = false;

	if(Handle != INVALID_HANDLE_VALUE)
		Unload(false);

	if(m_Loaded == false)
	{
		if(Handle != INVALID_HANDLE_VALUE)
		{
			static char pipe_buf[DARKMOD_LG_RENDERPIPE_BUFSIZE];
			DWORD cbBytesRead, dwBufSize, BufLen, dwLastError;


			DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("Reading from renderpipe [%08lX]\r", Handle);

			dwBufSize = DARKMOD_LG_RENDERPIPE_BUFSIZE;

			BufLen = 0;

			while(1)

			{

				ReadFile(Handle, // handle to pipe

					&pipe_buf[BufLen],						// buffer to receive data

					dwBufSize,								// size of buffer

					&cbBytesRead,							// number of bytes read

					NULL);									// not overlapped I/O

				dwLastError = GetLastError();
				DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("%lu bytes read from renderpipe [%08lX]   %lu (%08lX) %lu\r", cbBytesRead, Handle, BufLen, m_Image, dwLastError);

				BufLen += cbBytesRead;
				dwBufSize -= cbBytesRead;

				if(cbBytesRead == 0 || dwLastError == ERROR_BROKEN_PIPE)
					break;
				
				if(dwBufSize <= 0)
				{
					DM_LOG(LC_SYSTEM, LT_ERROR)LOGSTRING("Bufferoverflow when reading from renderpipe\r");
					goto Quit;
				}
			}


			if(BufLen > m_BufferLength || m_Image == NULL)
			{
				Unload(true);
				m_BufferLength = BufLen;
				if((m_Image = new unsigned char[m_BufferLength]) == NULL)
				{
					DM_LOG(LC_SYSTEM, LT_ERROR)LOGSTRING("Out of memory while allocating %lu bytes for [%s]\r", m_BufferLength, m_Name.c_str());
					goto Quit;
				}
			}
//			DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("Total of %lu bytes read from renderpipe [%s]   %lu (%08lX)\r", cbBytesRead, m_Name.c_str(), m_BufferLength, m_Image);

			memcpy(m_Image, pipe_buf, m_BufferLength);
			InitImageInfo();
			m_Loaded = true;
		}
	}

Quit:
	if(m_Loaded == false && m_Image != NULL)
	{
		delete [] m_Image;
		m_Image = NULL;
	}

	return rc;
}

#endif // __linux__

bool CImage::LoadImage(const char *Filename)
{
	bool rc = false;
	idFile *fl = NULL;

	if(Filename != NULL)
	{
		Unload(false);
		m_Name = Filename;
	}

	if(m_Loaded == false)
	{
		if((fl = fileSystem->OpenFileRead(m_Name)) == NULL)
		{
			DM_LOG(LC_SYSTEM, LT_ERROR)LOGSTRING("Unable to load imagefile [%s]\r", m_Name.c_str());
			goto Quit;
		}

		m_BufferLength = fl->Length();
		if((m_Image = new unsigned char[m_BufferLength]) == NULL)
		{
			DM_LOG(LC_SYSTEM, LT_ERROR)LOGSTRING("Out of memory while allocating %lu bytes for [%s]\r", m_BufferLength, m_Name.c_str());
			goto Quit;
		}
		fl->Read(m_Image, m_BufferLength);
		fileSystem->CloseFile(fl);

		InitImageInfo();
		m_Loaded = true;
//		DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("ImageWidth: %u   ImageHeight: %u   ImageDepth: %u   BPP: %u   Buffer: %u\r", m_Width, m_Height, ilGetInteger(IL_IMAGE_DEPTH), m_Bpp, m_BufferLength);
	}

Quit:
	if(m_Loaded == false && m_Image != NULL)
	{
		delete [] m_Image;
		m_Image = NULL;
	}

	return rc;
}

void CImage::InitImageInfo(void)
{
	ilGenImages(1, &m_ImageId);
	ilBindImage(m_ImageId);

	if(ilLoadL(IL_TYPE_UNKNOWN, m_Image, m_BufferLength) == IL_FALSE)
	{
		DM_LOG(LC_SYSTEM, LT_ERROR)LOGSTRING("Error while loading image [%s]\r", m_Name.c_str());
		goto Quit;
	}

	m_Width = ilGetInteger(IL_IMAGE_WIDTH);
	m_Height = ilGetInteger(IL_IMAGE_HEIGHT);
	m_Bpp = ilGetInteger(IL_IMAGE_BPP);

Quit:
	return;
}

unsigned long CImage::GetBufferLen(void)
{
	return m_BufferLength;
}

unsigned char *CImage::GetImage(void)
{
	unsigned char *rc = NULL;

	if(m_Loaded == true && m_Image != NULL)
	{
		ilBindImage(m_ImageId);
		ilLoadL(IL_TYPE_UNKNOWN, m_Image, m_BufferLength);
		rc = (unsigned char *)ilGetData();
	}

	return rc;
}

/*
const char *DM_OSPathToRelativePath(const char *OSPath)
{
	DM_LOG(LC_LIGHT, LT_INFO)LOGSTRING("DM_OSPathToRelativePath: [%s]\r", (OSPath) ? OSPath: "NULL");
	RETURN_META_VALUE(MRES_HANDLED, NULL);
}

const char *DM_RelativePathToOSPath(const char *relativePath, const char *basePath)
{
	DM_LOG(LC_LIGHT, LT_INFO)LOGSTRING("DM_RelativePathToOSPath: RelativePath [%s]   basePath: [%s]\r", 
		(relativePath) ? relativePath : "NULL",
		(basePath) ? basePath : "NULL"
		);
	RETURN_META_VALUE(MRES_HANDLED, NULL);
}
*/

const char *DM_BuildOSPath(const char *basePath, const char *game, const char *relativePath)
{
	static char p[1024];
	char *pRet = NULL;
	idStr Drive;
	META_RES Ret = MRES_IGNORED;

	if(idStr::Cmpn("\\\\.\\", relativePath, 4) == 0)
	{
		strcpy(p, DARKMOD_LG_RENDERPIPE_NAME);
		Ret = MRES_SUPERCEDE;
		pRet = p;
	}

	RETURN_META_VALUE(Ret, pRet);
}

void CGlobal::GetSurfName(const idMaterial *material, idStr &strIn )
{
	int end;
	int surftype;
	
	if (material == NULL) {
		goto Quit;
	}
	
	end = -1;
	surftype = material->GetSurfaceType();

	if( surftype != SURFTYPE_15 )
	{
		strIn = gameLocal.sufaceTypeNames[ surftype ];
		goto Quit;
	}

	// return the first word of the description if it has surftype_15
	strIn = material->GetDescription();
	end = strIn.Find(' ');

	if ( end == -1 )
	{
		goto Quit;
	}

	strIn = strIn.Left( end );

Quit:
	if( strIn.IsEmpty() )
		strIn = "none";

	//DM_LOG(LC_MISC, LT_DEBUG)LOGSTRING("GetSurfName: Found surface type name %s\r", strIn.c_str());

	return;
}
