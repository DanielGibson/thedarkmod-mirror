/***************************************************************************
 *
 * PROJECT: The Dark Mod - Updater
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#pragma once

#include <string>
#include <map>
#include <set>
#include <istream>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace fs = boost::filesystem;

namespace tdm
{

class IniFile;
typedef boost::shared_ptr<IniFile> IniFilePtr;

class IniFile
{
public:
	typedef std::pair<std::string, std::string> KeyValuePair;
	typedef std::vector<KeyValuePair> KeyValuePairList;

private:
	struct SectionCompareFunctor : 
		public std::binary_function<std::string, std::string, bool>
	{
		bool operator()(const std::string& s1, const std::string& s2) const
		{
			return boost::algorithm::ilexicographical_compare(s1, s2);
		}
	};

	struct KeyCompareFunctor : 
		public std::binary_function<std::string, std::string, bool>
	{
		bool operator()(const KeyValuePair& kvp1, const KeyValuePair& kvp2) const
		{
			return boost::algorithm::ilexicographical_compare(kvp1.first, kvp2.first);
		}
	};

	typedef std::set<KeyValuePair, KeyCompareFunctor> KeyValues;

	// Settings map: the key is a composite of <ConfigSection>.<key>
	// Keys are compared case-insensitively
	typedef std::map<std::string, KeyValues, SectionCompareFunctor> SettingMap;
	SettingMap _settings;

	// Private constructors
	IniFile();
	IniFile(const std::string& str);

public:
	// ---------- Named Constructors -----------------

	// Construct an INI file from the given filename
	// Can return NULL if the given file cannot be read
	static IniFilePtr ConstructFromFile(const fs::path& filename);

	// Construct an INI file from the given input stream
	// Always returns non-NULL
	static IniFilePtr ConstructFromStream(std::istream& stream);

	// Construct an INI file from the given string
	// Always returns non-NULL
	static IniFilePtr ConstructFromString(const std::string& str);

	// Creates an empty INI file
	static IniFilePtr Create();

	// ---------- Public Methods ---------------------

	// Returns TRUE if this file has no sections
	bool IsEmpty() const;

	/** 
	 * Add a named section. This won't do anything if the section already exists.
	 */
	void AddSection(const std::string& name);

	/** 
	 * Remove a named section including all keyvalue pairs belonging to it.
	 * Returns true if the section was found and removed, false otherwise.
	 */
	bool RemoveSection(const std::string& section);

	/**
	 * Returns a keyvalue from this INI file, from the given section,
	 * or an empty string if the key was not found.
	 */
	std::string GetValue(const std::string& section, const std::string& key) const;

	/** 
	 * Sets the given key in the given section to the given value. Always succeeds,
	 * existing values are overwritten.
	 */
	void SetValue(const std::string& section, const std::string& key, const std::string& value);

	/** 
	 * Removes the key from the given section.
	 * Returns TRUE if the key was present, FALSE otherwise
	 */
	bool RemoveKey(const std::string& section, const std::string& key);

	/**
	 * Returns all key/value pairs for the given section.
	 */
	KeyValuePairList GetAllKeyValues(const std::string& section) const;

	class SectionVisitor
	{
	public:
		virtual void VisitSection(const IniFile& iniFile, const std::string& sectionName) = 0;
	};

	/**
	 * Iterates over all sections, and invoking the given functor
	 */
	void ForeachSection(SectionVisitor& visitor) const;

	// Saves the contents into the given text file (target will be overwritten if existing)
	// One can pass a header string which will be prepended at the top of the file, in comment form.
	// The header string should be passed without the # character, these are added automatically.
	void ExportToFile(const fs::path& file, const std::string& headerComments = "") const;

private:
	void ParseFromString(const std::string& str);
};

} // namespace
