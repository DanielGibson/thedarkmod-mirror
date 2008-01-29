/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 1435 $
 * $Date: 2008-01-27 18:53:28 +0200 (Di, 27 Jan 2008) $
 * $Author: greebo $
 *
 ***************************************************************************/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "DifficultySettings.h"
#include <vector>

namespace difficulty {

#define PATTERN_DIFF "diff_%d_"
#define PREFIX_CLASS "class_"

#define PATTERN_CLASS "diff_%d_class_%d"
#define PATTERN_CHANGE "diff_%d_change_%d"
#define PATTERN_ARG "diff_%d_arg_%d"

#define APPTYPE_IGNORE "_IGNORE"

Setting::Setting() :
	isValid(false)
{}

void Setting::Save(idSaveGame* savefile)
{
	savefile->WriteBool(isValid);
	savefile->WriteString(className);
	savefile->WriteString(spawnArg);
	savefile->WriteString(argument);
	savefile->WriteInt(static_cast<int>(appType));
}

void Setting::Restore(idRestoreGame* savefile)
{
	savefile->ReadBool(isValid);
	savefile->ReadString(className);
	savefile->ReadString(spawnArg);
	savefile->ReadString(argument);

	int temp;
	savefile->ReadInt(temp);
	appType = static_cast<EApplicationType>(temp);
}

void Setting::Apply(idDict& target)
{
	switch (appType) 
	{
		case EAssign:
			target.Set(spawnArg, argument);
			break;
		case EAdd:
			// Convert the old setting to float, add the argument, convert back to string and set as value
			target.Set(spawnArg, idStr(float(target.GetFloat(spawnArg) + atof(argument))));
			break;
		case EMultiply:
			// Convert the old setting to float, add the argument, convert back to string and set as value
			target.Set(spawnArg, idStr(float(target.GetFloat(spawnArg) * atof(argument))));
			break;
		case EIgnore:
			// Ignore => do nothing
			break;
		default:
			break;
	};
}

void Setting::ParseFromDict(const idDict& dict, int level, int index)
{
	isValid = true; // in dubio pro reo

	// Get the classname, target spawnarg and argument
	className = dict.GetString(va(PATTERN_CLASS, level, index));
	spawnArg = dict.GetString(va(PATTERN_CHANGE, level, index));
	argument = dict.GetString(va(PATTERN_ARG, level, index));

	// Parse the application type
	appType = EAssign;

	if (!argument.IsEmpty())
	{
		// Check for ignore argument
		if (argument == APPTYPE_IGNORE)
		{
			appType = EIgnore;
			argument.Empty(); // clear the argument
		}
		// Check for special modifiers
		else if (argument[0] == '+')
		{
			appType = EAdd;
			// Remove the first character
			argument = idStr(argument, 1, argument.Length());
		}
		else if (argument[0] == '*')
		{
			appType = EMultiply;
			// Remove the first character
			argument = idStr(argument, 1, argument.Length());
		}
		else if (argument[0] == '-')
		{
			appType = EAdd;
			// Leave the "-" sign, it will be the sign of the parsed int
		}
	}

	if (spawnArg.IsEmpty())
	{
		// Spawnarg must not be empty
		isValid = false;
	}

	// classname can be empty (this is valid for entity-specific difficulties)
}

// Static parsing function
idList<Setting> Setting::ParseSettingsFromDict(const idDict& dict, int level)
{
	idList<Setting> list;

	// Cycle through all difficulty settings (looking for "diff_0_class_*")
	idStr prefix = idStr(va(PATTERN_DIFF, level)) + PREFIX_CLASS;
	for (const idKeyValue* keyVal = dict.MatchPrefix(prefix);
		  keyVal != NULL;
		  keyVal = dict.MatchPrefix(prefix, keyVal))
	{
		DM_LOG(LC_DIFFICULTY, LT_INFO).LogString("Parsing keyvalue: %s = %s.\r", keyVal->GetKey().c_str(), keyVal->GetValue().c_str());

		// Get the index from this keyvalue (remove the prefix and convert to int)
		idStr key = keyVal->GetKey();
		key.StripLeadingOnce(prefix);
		if (key.IsNumeric())
		{
			// Extract the index
			int index = atoi(key);

			// Parse the settings with the given index
			Setting s;
			s.ParseFromDict(dict, level, index);

			// Check for validity and insert into map
			if (s.isValid)
			{
				list.Append(s);
			}
		}
		else
		{
			gameLocal.Warning("Found invalid difficulty settings index: %s.\r", keyVal->GetKey().c_str());
			DM_LOG(LC_DIFFICULTY, LT_ERROR).LogString("Found invalid difficulty settings index: %s.\r", keyVal->GetKey().c_str());
		}
	}

	return list;
}

// =======================================================================

void DifficultySettings::Clear()
{
	_settings.clear();
}

void DifficultySettings::SetLevel(int level)
{
	_level = level;
}

int DifficultySettings::GetLevel() const
{
	return _level;
}

void DifficultySettings::LoadFromEntityDef(const idDict& defDict)
{
	// Parse all settings from the given entityDef
	idList<Setting> settings = Setting::ParseSettingsFromDict(defDict, _level);

	// Copy all settings into the SettingsMap
	for (int i = 0; i < settings.Num(); i++)
	{
		_settings.insert(SettingsMap::value_type(settings[i].className.c_str(), settings[i]));
	}
}

void DifficultySettings::LoadFromMapEntity(idMapEntity* ent)
{
	// Search the epairs for difficulty settings
	idList<Setting> settings = Setting::ParseSettingsFromDict(ent->epairs, _level);

	// greebo: Go through all found settings and remove all default
	// settings with the same class/spawnarg combination
	for (int i = 0; i < settings.Num(); i++)
	{
		// We need std::string for STL map lookups, not idStr
		std::string className = settings[i].className.c_str();

		// Search all stored settings matching this classname
		for (SettingsMap::iterator found = _settings.find(className);
			 found != _settings.upper_bound(className) && found != _settings.end();
			 /* in-loop increment */)
		{
			if (found->second.spawnArg == settings[i].spawnArg)
			{
				// Spawnarg and classname match, remove it and post-increment the iterator
				_settings.erase(found++);
			}
			else
			{
				found++; // no match, step forward
			}
		}
	}

	// Now copy all spawnargs into the SettingsMap
	for (int i = 0; i < settings.Num(); i++)
	{
		_settings.insert(SettingsMap::value_type(settings[i].className.c_str(), settings[i]));
	}
}

void DifficultySettings::ApplySettings(idDict& target)
{
	std::string eclass = target.GetString("classname");

	if (eclass.empty()) {
		return; // no classname, no rules
	}

	// greebo: First, get the list of entity-specific difficulty settings from the dictionary
	// Everything processed here will be ignored in the second run (where the default settings are applied)
	typedef std::map<std::string, int> ChangeList;
	ChangeList entityChanges;

	// TODO: First sweep is missing here

	// Get the inheritancechain for the given target dict
	InheritanceChain inheritanceChain = GetInheritanceChain(target);

	// Go through the inheritance chain front to back and apply the settings
	for (InheritanceChain::iterator c = inheritanceChain.begin(); c != inheritanceChain.end(); c++)
	{
		std::string className = c->c_str();

		// Process the list of default settings that apply to this entity class,
		// but ignore all keys that have been addressed by the entity-specific settings.
		for (SettingsMap::iterator i = _settings.find(className);
			 i != _settings.upper_bound(className) && i != _settings.end();
			 i++)
		{
			Setting& setting = i->second;

			// Get the name of the target spawnarg
			std::string spawnArg = setting.spawnArg.c_str();
		
			if (entityChanges.find(spawnArg) != entityChanges.end())
			{
				// This target spawnarg has already been processed in the first run, skip it
				continue;
			}

			// We have green light, apply the setting
			setting.Apply(target);
		}
	}
}

// Save/Restore methods
void DifficultySettings::Save(idSaveGame* savefile)
{
	savefile->WriteInt(static_cast<int>(_settings.size()));
	for (SettingsMap::iterator i = _settings.begin(); i != _settings.end(); i++)
	{
		idStr className(i->second.className.c_str());
		// Save the key and the value
		savefile->WriteString(className); // key
		i->second.Save(savefile); // value
	}
}

void DifficultySettings::Restore(idRestoreGame* savefile)
{
	int num;
	savefile->ReadInt(num);
	for (int i = 0; i < num; i++)
	{
		idStr className;
		savefile->ReadString(className);

		// Insert an empty structure into the map
		SettingsMap::iterator inserted = _settings.insert(
			SettingsMap::value_type(className.c_str(), Setting())
		);

		// Now restore the struct itself
		inserted->second.Restore(savefile);
	}
}

DifficultySettings::InheritanceChain DifficultySettings::GetInheritanceChain(const idDict& dict)
{
	InheritanceChain inheritanceChain;

	// Add the classname itself to the end of the list
	inheritanceChain.push_back(dict.GetString("classname"));

	// greebo: Extract the inherit value from the raw declaration text, 
	// as the "inherit" key has been removed in the given "dict"
	for (idStr inherit = GetInheritValue(dict.GetString("classname")); 
		 !inherit.IsEmpty(); 
		 inherit = GetInheritValue(inherit))
	{
		// Has parent, add to list
		inheritanceChain.push_front(inherit);
	}

	/*gameLocal.Printf("Inheritance chain: ");
	for (InheritanceChain::iterator i = inheritanceChain.begin(); i != inheritanceChain.end(); i++)
	{
		gameLocal.Printf("%s - ", (*i).c_str());
	}*/

	// TODO: Cache this chain

	return inheritanceChain;
}

idStr DifficultySettings::GetInheritValue(const idStr& className)
{
	// Get the raw declaration, in the parsed entitydefs, all "inherit" keys have been remoed
	const idDecl* decl = declManager->FindType(DECL_ENTITYDEF, className, false);

	if (decl == NULL)
	{
		return ""; // no declaration found...
	}

	// Get the raw text from the declaration manager
	std::string buffer;
	buffer.resize(decl->GetTextLength()+1);
	decl->GetText(&buffer[0]);

	// Find the "inherit" key and parse the value
	std::size_t pos = buffer.find("\"inherit\"");
	if (pos == std::string::npos) {
		return ""; // not found
	}

	pos += 9; // skip "inherit"

	while (buffer[pos] != '"' && pos < buffer.size()) {
		pos++; // skip everything till first "
	}

	pos++; // skip "

	idStr inherit;

	while (buffer[pos] != '"' && pos < buffer.size()) {
		inherit += buffer[pos];
		pos++;
	}
	
	return inherit;
}

} // namespace difficulty
