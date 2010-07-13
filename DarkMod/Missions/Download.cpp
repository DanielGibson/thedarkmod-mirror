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

static bool init_version = FileVersionList("$Id$", init_version);

#include "Download.h"
#include "../Http/HttpConnection.h"
#include "../ZipLoader/ZipLoader.h"
#include "MissionManager.h"

#include <boost/bind.hpp>

CDownload::CDownload(const idStr& url, const idStr& destFilename) :
	_curUrl(0),
	_destFilename(destFilename),
	_status(NOT_STARTED_YET),
	_pk4CheckEnabled(false)
{
	_urls.Append(url);
}

CDownload::CDownload(const idStringList& urls, const idStr& destFilename) :
	_urls(urls),
	_curUrl(0),
	_destFilename(destFilename),
	_status(NOT_STARTED_YET),
	_pk4CheckEnabled(false)
{}

CDownload::~CDownload()
{
	Stop();
}

CDownload::DownloadStatus CDownload::GetStatus()
{
	return _status;
}

void CDownload::Start()
{
	if (_status != NOT_STARTED_YET)
	{
		return; // don't allow double starts
	}

	// Construct the temporary filename
	idStr filename;
	_destFilename.ExtractFileName(filename);
	_destFilename.ExtractFilePath(_tempFilename);

	// /path/to/fms/_filename.pk4 (including underscore)
	_tempFilename += "_" + filename;
	DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Downloading to temporary file %s.\r", _tempFilename.c_str());

	_status = IN_PROGRESS;
	_thread = ThreadPtr(new boost::thread(boost::bind(&CDownload::Perform, this)));
}

void CDownload::Stop()
{
	if (_thread != NULL && _request != NULL)
	{
		// Set the URL index beyond the list size to prevent 
		// the worker thread from proceeding to the next URL
		_curUrl = _urls.Num();

		// Cancel the request
		_request->Cancel();

		// Wait for the thread to finish
		_thread->join();

		_thread.reset();
		_request.reset();

		// Don't reset successful stati
		if (_status != SUCCESS)
		{
			_status = FAILED;
		}

		// Remove temporary file
		CMissionManager::DoRemoveFile(_tempFilename.c_str());
	}
}

double CDownload::GetProgressFraction()
{
	return _request != NULL ? _request->GetProgressFraction() : 0.0;
}

void CDownload::EnableValidPK4Check(bool enable)
{
	_pk4CheckEnabled = enable;
}

void CDownload::Perform()
{
	while (_curUrl < _urls.Num())
	{
		// Remove any previous temporary file
		CMissionManager::DoRemoveFile(_tempFilename.c_str());

		const idStr& url = _urls[_curUrl];

		// Create a new request
		_request = gameLocal.m_HttpConnection->CreateRequest(url.c_str(), _tempFilename.c_str());
	
		// Start the download, blocks until finished or aborted
		_request->Perform();

		if (_request->GetStatus() == CHttpRequest::OK)
		{
			// Check the downloaded file
			if (_pk4CheckEnabled)
			{
				bool valid = CheckValidPK4(_tempFilename);

				if (!valid)
				{
					_curUrl++;
					continue;
				}
			}

			// Move temporary file to the real one
			if (CMissionManager::DoMoveFile(_tempFilename.c_str(), _destFilename.c_str()))
			{
				_status = SUCCESS;
			}
			else
			{
				// Move failed
				_status = FAILED;
			}

			// Download succeeded, exit the loop
			break;
		}
		else 
		{
			// Download error
			if (_request->GetStatus() == CHttpRequest::ABORTED)
			{
				DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Download from %s aborted.\r", _urls[_curUrl].c_str());
			}
			else
			{
				DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Connection Error.\r");
			}

			// Proceed to the next URL
			_curUrl++;
		}
	} // while

	// Have we run out of URLs
	if (_curUrl >= _urls.Num())
	{
		// This was our last URL, set the status to FAILED
		_status = FAILED;
	}
}

bool CDownload::CheckValidPK4(const idStr& path)
{
	CZipFilePtr zipFile = CZipLoader::Instance().OpenFile(path);

	return (zipFile != NULL);
}
