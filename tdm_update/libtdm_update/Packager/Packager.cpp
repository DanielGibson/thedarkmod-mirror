#include "Packager.h"

#include "../Constants.h"
#include "../Util.h"
#include "../ExceptionSafeThread.h"

#include <boost/thread.hpp>

namespace tdm
{

namespace packager
{

// Pass the program options to this class
Packager::Packager(const PackagerOptions& options) :
	_options(options)
{}

void Packager::GatherBaseSet()
{
	std::set<std::string> ignoreList;
	ignoreList.insert(TDM_CRC_INFO_FILE);

	_baseSet = ReleaseFileSet::LoadFromFolder(_options.Get("basedir"), ignoreList);
}

void Packager::GatherHeadSet()
{
	std::set<std::string> ignoreList;
	ignoreList.insert(TDM_CRC_INFO_FILE);

	_headSet = ReleaseFileSet::LoadFromFolder(_options.Get("headdir"), ignoreList);
}

void Packager::CalculateSetDifference()
{
	typedef std::vector<std::pair<std::string, ReleaseFile> > DiffContainer;

	// PK4s which have been removed since base
	{
		DiffContainer toBeRemoved;
	
		// Compute the difference for files to be removed (found in base, but not in head)
		std::set_difference(_baseSet.begin(), _baseSet.end(), _headSet.begin(), _headSet.end(), 
			std::back_inserter(toBeRemoved), _baseSet.value_comp());
		
		TraceLog::WriteLine(LOG_STANDARD, (boost::format("PK4s to be removed: %d") % toBeRemoved.size()).str());

		for (DiffContainer::iterator i = toBeRemoved.begin(); i != toBeRemoved.end(); ++i)
		{
			if (!i->second.isArchive)
			{
				TraceLog::WriteLine(LOG_VERBOSE, i->first + " is not an archive, skipping that file.");
				continue;
			}

			TraceLog::WriteLine(LOG_VERBOSE, "PK4 to be removed: " + i->second.file.file_string());
			_difference.pk4sToBeRemoved.insert(i->second);
		}
	}

	// PK4s which have been added since base
	{
		DiffContainer toBeAdded;
		// Compute the difference for files to be added (found in head, but not in base)
		std::set_difference(_headSet.begin(), _headSet.end(), _baseSet.begin(), _baseSet.end(), 
			std::back_inserter(toBeAdded), _headSet.value_comp());
		
		TraceLog::WriteLine(LOG_STANDARD, (boost::format("PK4s to be added: %d") % toBeAdded.size()).str());

		for (DiffContainer::iterator i = toBeAdded.begin(); i != toBeAdded.end(); ++i)
		{
			if (!i->second.isArchive)
			{
				TraceLog::WriteLine(LOG_VERBOSE, i->first + " is not an archive, skipping that file.");
				continue;
			}

			TraceLog::WriteLine(LOG_VERBOSE, "PK4 to be added: " + i->second.file.file_string());
			_difference.pk4sToBeAdded.insert(i->second);
		}
	}

	// Calculate member difference
	{
		DiffContainer memberDiff;

		for (ReleaseFileSet::const_iterator h = _headSet.begin(); h != _headSet.end(); ++h)
		{
			const ReleaseFile& headFile = h->second;

			if (!headFile.isArchive)
			{
				TraceLog::WriteLine(LOG_VERBOSE, h->first + " is not an archive, skipping that file.");
				continue;
			}

			ReleaseFileSet::const_iterator b = _baseSet.find(h->first);

			// Check if this file is in the base set in the first place
			if (b == _baseSet.end()) 
			{
				TraceLog::WriteLine(LOG_VERBOSE, h->first + " is not in base set, skipping detailed comparison.");
				continue;
			}

			const ReleaseFile& baseFile = b->second;

			// Compare the release files (note the non-trivial ==/!= operators)
			if (headFile == baseFile)
			{
				TraceLog::WriteLine(LOG_STANDARD, (boost::format("%s (HEAD) is equal to %s (BASE)") % h->first % b->first).str());
				continue;
			}

			TraceLog::WriteLine(LOG_STANDARD, (boost::format("%s (HEAD) is different from %s (BASE)") % h->first % b->first).str());

			// In-depth PK4 comparison

			_difference.pk4Differences[h->first] = UpdatePackage::PK4Difference();

			UpdatePackage::PK4Difference& pk4Diff = _difference.pk4Differences[h->first];

			pk4Diff.checksumBefore = baseFile.crc;
			pk4Diff.checksumAfter = headFile.crc;
			
			// Find all files that are in base, but not in head, these should be removed
			std::vector<ReleaseFile> filesToBeRemoved;

			// Compute the difference for files to be removed (found in base file, but not in head file)
			std::set_difference(baseFile.members.begin(), baseFile.members.end(), headFile.members.begin(), headFile.members.end(), 
				std::back_inserter(filesToBeRemoved));

			for (std::vector<ReleaseFile>::const_iterator i = filesToBeRemoved.begin(); i != filesToBeRemoved.end(); ++i)
			{
				TraceLog::WriteLine(LOG_VERBOSE, "  PK4 member to be removed: " + i->file.string());

				pk4Diff.membersToBeRemoved.insert(*i);
			}

			std::size_t equalMembers = 0;
			
			for (std::set<ReleaseFile>::const_iterator hf = headFile.members.begin(); hf != headFile.members.end(); ++hf)
			{
				std::set<ReleaseFile>::const_iterator bf = baseFile.members.find(*hf);

				if (bf == baseFile.members.end()) 
				{
					// New file
					TraceLog::WriteLine(LOG_VERBOSE, "  PK4 member to be added: " + hf->file.string());

					pk4Diff.membersToBeAdded.insert(*hf);
				}
				else
				{
					// File in base as well as in head, compare crc and size
					if (*bf != *hf)
					{
						TraceLog::WriteLine(LOG_VERBOSE, "  PK4 member has been changed: " + hf->file.string());

						pk4Diff.membersToBeReplaced.insert(*hf);
					}
					else
					{
						TraceLog::WriteLine(LOG_VERBOSE, "  PK4 member is equal to base: " + hf->file.string());
						equalMembers++;
					}
				}
			}

			TraceLog::WriteLine(LOG_STANDARD, (boost::format("  %d files changed, %d files to be added, %d files to be removed, %d files equal.") % 
				pk4Diff.membersToBeReplaced.size() % pk4Diff.membersToBeAdded.size() % pk4Diff.membersToBeRemoved.size() % equalMembers).str());
		}
	}
}

void Packager::CreateUpdatePackage()
{
	std::string updatePackageFileName = (boost::format("tdm_update_%s_to_%s.zip") % _options.Get("baseversion") % _options.Get("headversion")).str();

	fs::path headDir = _options.Get("headdir");
	fs::path outputDir = _options.Get("outputdir");

	fs::path updatePackagePath = outputDir / updatePackageFileName; 

	_difference.filename = updatePackagePath;

	TraceLog::WriteLine(LOG_STANDARD, "Creating update package at " + updatePackagePath.file_string());

	ZipFileWritePtr updatePackage = Zip::OpenFileWrite(updatePackagePath, Zip::CREATE);

	if (updatePackage == NULL)
	{
		throw FailureException("Couldn't create " + updatePackagePath.file_string());
	}

	IniFilePtr updateDesc = IniFile::Create();

	updateDesc->SetValue("Info", "from_version", _options.Get("baseversion"));
	updateDesc->SetValue("Info", "to_version", _options.Get("headversion"));

	// Pack in new files
	for (std::set<ReleaseFile>::const_iterator i = _difference.pk4sToBeAdded.begin(); i != _difference.pk4sToBeAdded.end(); ++i)
	{
		TraceLog::WriteLine(LOG_STANDARD, "Storing PK4: " + i->file.string());

		updatePackage->DeflateFile(headDir / i->file, i->file.string(), ZipFileWrite::STORE);

		updateDesc->SetValue("Add PK4s", i->file.string(), CRC::ToString(i->crc));
	}

	// Add information about removed PK4s
	for (std::set<ReleaseFile>::const_iterator i = _difference.pk4sToBeRemoved.begin(); i != _difference.pk4sToBeRemoved.end(); ++i)
	{
		TraceLog::WriteLine(LOG_STANDARD, "PK4 marked for removal: " + i->file.string());

		updateDesc->SetValue("Remove PK4s", i->file.string(), "remove");
	}

	// Add sections for removed PK4 members
	for (UpdatePackage::Pk4DifferenceMap::const_iterator i = _difference.pk4Differences.begin(); 
		 i != _difference.pk4Differences.end(); ++i)
	{
		TraceLog::WriteLine(LOG_STANDARD, "Changed PK4: " + i->first);

		std::string section = "Change " + i->first;

		// Override for changed ZIP files: these will be extracted, so changes directly affect the filesystem
		if (File::IsZip(i->first))
		{
			section = "Non-Archive Files";
		}

		if (File::IsPK4(i->first))
		{
			// Add checksum information for PK4s
			updateDesc->SetValue(section, "checksum_before", CRC::ToString(i->second.checksumBefore));
			updateDesc->SetValue(section, "checksum_after", CRC::ToString(i->second.checksumAfter));
		}

		for (std::set<ReleaseFile>::const_iterator m = i->second.membersToBeRemoved.begin(); 
			 m != i->second.membersToBeRemoved.end(); ++m)
		{
			TraceLog::WriteLine(LOG_STANDARD, "  Member marked for removal: " + m->file.string());

			updateDesc->SetValue(section, m->file.string(), "remove");
		}

		// Quick continue if no added or changed members
		if (i->second.membersToBeAdded.empty() && i->second.membersToBeReplaced.empty())
		{
			continue;
		}

		// Open the source PK4
		ZipFileReadPtr sourcePk4 = Zip::OpenFileRead(headDir / i->first);

		for (std::set<ReleaseFile>::const_iterator m = i->second.membersToBeAdded.begin(); 
			 m != i->second.membersToBeAdded.end(); ++m)
		{
			TraceLog::WriteLine(LOG_STANDARD, "  Member added: " + m->file.string());

			updateDesc->SetValue(section, m->file.string(), "add");

			// Pack that added file into the update PK4
			updatePackage->CopyFileFromZip(sourcePk4, m->file.string(), m->file.string());
		}

		for (std::set<ReleaseFile>::const_iterator m = i->second.membersToBeReplaced.begin(); 
			 m != i->second.membersToBeReplaced.end(); ++m)
		{
			TraceLog::WriteLine(LOG_STANDARD, "  Member changed: " + m->file.string());

			updateDesc->SetValue(section, m->file.string(), "replace");

			// Pack that added file into the update PK4
			updatePackage->CopyFileFromZip(sourcePk4, m->file.string(), m->file.string());
		}
	}

	// Pack the update INI file into the package
	fs::path iniPath = outputDir / TDM_UDPATE_INFO_FILE;

	updateDesc->ExportToFile(iniPath);

	updatePackage->DeflateFile(iniPath, TDM_UDPATE_INFO_FILE);

	// Remove the ini file afterwards
	File::Remove(iniPath);
}

void Packager::CreateVersionInformation()
{
	if (_baseSet.empty())
	{
		throw FailureException("No base information, cannot create version information.");
	}

	assert(_options.IsSet("baseversion"));

	fs::path outputDir = _options.Get("outputdir");

	fs::path versionInfoFile = outputDir / TDM_VERSION_INFO_FILE;

	IniFilePtr versionInfo;

	if (fs::exists(versionInfoFile))
	{
		// Load existing version information
		TraceLog::WriteLine(LOG_STANDARD, "Loading existing version information file: " + versionInfoFile.file_string());
		versionInfo = IniFile::ConstructFromFile(versionInfoFile);
	}
	else
	{
		// Create a new ini file
		versionInfo = IniFile::Create();
	}

	// The list of files for which no CRC should be calculated during version check
	std::set<std::string> noCrcFiles;
	noCrcFiles.insert("doomconfig.cfg");
	noCrcFiles.insert("dmargs.txt");

	// Merge the information into the file
	for (ReleaseFileSet::const_iterator f = _baseSet.begin(); f != _baseSet.end(); ++f)
	{
		std::string section = (boost::format("Version%s File %s") % _options.Get("baseversion") % f->second.file.string()).str();

		if (File::IsPK4(f->second.file))
		{
			versionInfo->SetValue(section, "crc", CRC::ToString(f->second.crc));
			versionInfo->SetValue(section, "filesize", boost::lexical_cast<std::string>(f->second.filesize));
		}
		// Traverse ZIP file members (the actual ZIP files will be ignored when checking the local version)
		else if (File::IsZip(f->second.file))
		{
			for (std::set<ReleaseFile>::const_iterator m = f->second.members.begin(); m != f->second.members.end(); ++m)
			{
				std::string memberSection = (boost::format("Version%s File %s") % _options.Get("baseversion") % m->file.string()).str();

				versionInfo->SetValue(memberSection, "crc", CRC::ToString(m->crc));
				versionInfo->SetValue(memberSection, "filesize", boost::lexical_cast<std::string>(m->filesize));

				if (noCrcFiles.find(boost::algorithm::to_lower_copy(m->file.file_string())) != noCrcFiles.end())
				{
					versionInfo->SetValue(memberSection, "allow_local_modifications", "1");
				}
			}
		}
	}

	// Save the file
	TraceLog::WriteLine(LOG_STANDARD, "Saving version information file: " + versionInfoFile.file_string());
	versionInfo->ExportToFile(versionInfoFile);
}

void Packager::RegisterUpdatePackage(const fs::path& packagePath)
{
	fs::path path = packagePath;

	if (!fs::exists(path)) 
	{
		path = _options.Get("outputdir");
		path /= packagePath;
	}

	if (!fs::exists(path)) 
	{
		throw FailureException("No package found at this path.");
	}

	ZipFileReadPtr package = Zip::OpenFileRead(path);

	if (package == NULL)
	{
		throw FailureException("Cannot open this package.");
	}

	TraceLog::WriteLine(LOG_STANDARD, "Loading update info file from package: " + path.file_string());

	std::string updateInfoStr = package->LoadTextFile(TDM_UDPATE_INFO_FILE);

	IniFilePtr iniFile = IniFile::ConstructFromString(updateInfoStr);

	if (iniFile->IsEmpty())
	{
		throw FailureException("Cannot load update info file from that package.");
	}

	std::string fromVersion = iniFile->GetValue("Info", "from_version");
	std::string toVersion = iniFile->GetValue("Info", "to_version");

	if (fromVersion.empty() || toVersion.empty())
	{
		throw FailureException("Cannot find from_version and to_version information in update package.");
	}

	// Open the target file the information should be merged in
	fs::path targetPath = _options.Get("outputdir");
	targetPath /= TDM_VERSION_INFO_FILE;

	IniFilePtr targetFile = IniFile::ConstructFromFile(targetPath);

	if (targetFile == NULL)
	{
		TraceLog::WriteLine(LOG_STANDARD, "Cannot find target version info file, creating afresh: " + targetPath.file_string());

		targetFile = IniFile::Create();
	}

	TraceLog::WriteLine(LOG_STANDARD, "Registering update package from version " + fromVersion + " to version " + toVersion);

	std::string section = (boost::format("UpdatePackage from %s to %s") % fromVersion % toVersion).str();

	// Store the information
	targetFile->SetValue(section, "package", path.leaf());
	targetFile->SetValue(section, "filesize", boost::lexical_cast<std::string>(fs::file_size(path)));
	targetFile->SetValue(section, "crc", CRC::ToString(CRC::GetCrcForFile(path)));

	TraceLog::WriteLine(LOG_STANDARD, "Saving INI file: " + targetPath.file_string());

	targetFile->ExportToFile(targetPath);
}

void Packager::LoadManifest()
{
	fs::path manifestPath = _options.Get("darkmoddir");
	manifestPath /= TDM_MANIFEST_PATH;
	manifestPath /= _options.Get("name") + TDM_MANIFEST_EXTENSION;

	TraceLog::Write(LOG_STANDARD, "Loading manifest at: " + manifestPath.file_string() + "...");

	_manifest.LoadFromFile(manifestPath);

	TraceLog::WriteLine(LOG_STANDARD, "");
	TraceLog::WriteLine(LOG_STANDARD, (boost::format("The manifest contains %d files.") % _manifest.size()).str());
}

void Packager::CheckRepository()
{
	TraceLog::Write(LOG_STANDARD, "Checking if the darkmod repository is complete...");

	std::size_t missingFiles = 0;

	fs::path darkmodPath = _options.Get("darkmoddir");

	for (ReleaseManifest::iterator i = _manifest.begin(); i != _manifest.end(); ++i)
	{
		if (!fs::exists(darkmodPath / i->sourceFile))
		{
			missingFiles++;
		}
	}

	TraceLog::WriteLine(LOG_STANDARD, "");

	if (missingFiles > 0)
	{
		TraceLog::Error((boost::format("The manifest contains %d files which are missing in your darkmod path.") % missingFiles).str());
	}
}

void Packager::LoadPk4Mapping()
{
	fs::path mappingFile = _options.Get("darkmoddir");
	mappingFile /= TDM_MANIFEST_PATH;
	mappingFile /= (boost::format("%s_pk4s%s") % _options.Get("name") % TDM_MANIFEST_EXTENSION).str(); // e.g. darkmod_pk4s.txt

	TraceLog::Write(LOG_STANDARD, "Loading PK4 mapping file: " + mappingFile.file_string() + "...");

	_pk4Mappings.LoadFromFile(mappingFile);

	TraceLog::WriteLine(LOG_STANDARD, "");
	TraceLog::WriteLine(LOG_STANDARD, (boost::format("The mapping file defines %d PK4s.") % _pk4Mappings.size()).str());
}

void Packager::SortFilesIntoPk4s()
{
	TraceLog::WriteLine(LOG_STANDARD, "Sorting files into PK4s...");

	_package.clear();

	fs::path darkmodPath = _options.Get("darkmoddir");

	// Go through each file in the manifest and check which pattern applies
	for (ReleaseManifest::iterator i = _manifest.begin(); i != _manifest.end(); /* in-loop increment */)
	{
		bool matched = false;

		// The patterns are applied in the order they appear in the darkmod_pk4s.txt file
		for (Pk4Mappings::const_iterator m = _pk4Mappings.begin(); !matched && m != _pk4Mappings.end(); ++m)
		{
			// Does this filename match any of the patterns of this PK4 file?
			for (Patterns::const_iterator p = m->second.begin(); p != m->second.end(); ++p)
			{
				if (boost::regex_search(i->destFile.string(), *p))
				{
					// Match
					ManifestFiles& files = _package.insert(Package::value_type(m->first, ManifestFiles())).first->second;

					//TraceLog::WriteLine(LOG_STANDARD, "Putting file " + i->destFile.string() + " => " + m->first);

					// Copy that file into the release package
					files.push_back(*i);

					// Remove the file from our manifest
					_manifest.erase(i++);

					matched = true;
					break;
				}
			}
		}

		if (!matched) 
		{
			// If the file is a PK4 file itself, just add it to the list
			if (File::IsArchive(i->destFile))
			{
				_package.insert(Package::value_type(i->sourceFile.string(), ManifestFiles()));

				_manifest.erase(i++);
			}
			else if (fs::is_directory(darkmodPath / i->destFile)) // Check if this is a folder
			{
				// Unmatched folders can be removed
				_manifest.erase(i++);
			}
			else
			{
				TraceLog::WriteLine(LOG_STANDARD, "Could not match file: " + i->destFile.string());
				++i;
			}
		}
	}

	TraceLog::WriteLine(LOG_STANDARD, "done");

	TraceLog::WriteLine(LOG_STANDARD, (boost::format("%d entries in the manifest could not be matched, check the logs.") % _manifest.size()).str());
}

void Packager::ProcessPackageElement(Package::const_iterator p)
{
	fs::path outputDir = _options.Get("outputdir");

	if (!fs::exists(outputDir))
	{
		fs::create_directories(outputDir);
	}

	fs::path darkmodPath = _options.Get("darkmoddir");

	// Target file
	fs::path pk4Path = outputDir / p->first;

	// Make sure all folders exist
	fs::path pk4Folder = pk4Path;
	pk4Folder.remove_leaf().remove_leaf();

	fs::create_directories(pk4Folder);

	// Remove destination file before writing
	File::Remove(pk4Path);

	// Copy-only switch for PK4 files mentioned in the manifest (those have 0 members to compress, like tdm_game01.pk4)
	if (File::IsArchive(p->first) && p->second.empty())
	{
		TraceLog::WriteLine(LOG_STANDARD, (boost::format("Copying file: %s") % pk4Path.string()).str());

		if (!File::Copy(darkmodPath / p->first, pk4Path))
		{
			TraceLog::Error((boost::format("Could not copy file: %s") % pk4Path.string()).str());
		}

		return;
	}

	TraceLog::WriteLine(LOG_STANDARD, (boost::format("Compressing package: %s") % pk4Path.string()).str());
	
	ZipFileWritePtr pk4 = Zip::OpenFileWrite(pk4Path, Zip::CREATE);

	if (pk4 == NULL)
	{
		throw FailureException("Failed to process element: " + p->first);
	}

	for (ManifestFiles::const_iterator m = p->second.begin(); m != p->second.end(); ++m)
	{
		fs::path sourceFile = darkmodPath / m->sourceFile;
		const fs::path& targetFile = m->destFile;

		// Make sure folders get added as such
		if (fs::is_directory(sourceFile))
		{
			continue;
		}

		ZipFileWrite::CompressionMethod method = ZipFileWrite::DEFLATE_MAX;

		TraceLog::WriteLine(LOG_VERBOSE, (boost::format("Deflating file %s.") % sourceFile.string()).str());

		pk4->DeflateFile(sourceFile, targetFile.string(), method);
	}
}

void Packager::CreatePackage()
{
	// Create worker threads to compress stuff into the target PK4s

	unsigned numHardwareThreads = boost::thread::hardware_concurrency();

	if (numHardwareThreads == 0) 
	{
		numHardwareThreads = 1;
	}

	TraceLog::WriteLine(LOG_STANDARD, (boost::format("Using %d threads to compress files.") % numHardwareThreads).str());

	std::vector<ExceptionSafeThreadPtr> threads(numHardwareThreads);

	Package::const_iterator i = _package.begin();

	// Keep pushing threads until we've reached the end of the list
	while (i != _package.end())
	{
		// Go through each thread and find a free one
		for (std::size_t threadNum = 0; threadNum < threads.size(); ++threadNum)
		{
			if (threads[threadNum] == NULL || threads[threadNum]->done())
			{
				// Allocate a new thread using the package being pointed at
				threads[threadNum].reset(new ExceptionSafeThread(boost::bind(&Packager::ProcessPackageElement, this, i)));

				// Next candidate
				++i;

				break;
			}
		}

		// Sleep 50 msec before attempting to create a new thread
		Util::Wait(50);
	}

	// No more unassigned packages, wait till all threads are done
	bool stillProcessing = true;

	while (stillProcessing)
	{
		stillProcessing = false;

		// As long as we've still one processing thread, set the bool back to true
		for (std::size_t threadNum = 0; threadNum < threads.size(); ++threadNum)
		{
			if (threads[threadNum] != NULL && !threads[threadNum]->done())
			{
				stillProcessing = true;
				break;
			}
		}

		Util::Wait(50);
	}

	TraceLog::WriteLine(LOG_STANDARD, "All threads done.");
}

} // namespace 

} // namespace
