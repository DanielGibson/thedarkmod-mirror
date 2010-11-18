/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 4055 $
 * $Date: 2010-07-13 13:17:09 +0200 (Di, 13 Jul 2010) $
 * $Author: greebo $
 *
 ***************************************************************************/

#include "HttpRequest.h"
#include "HttpConnection.h"

#include <cstring>
#include "../TraceLog.h"
#include "../Constants.h"

#ifdef WIN32
#include <winsock2.h> // greebo: need to include winsock2 before curl/curl.h
#endif

#include <curl/curl.h>

namespace tdm
{

HttpRequest::HttpRequest(HttpConnection& conn, const std::string& url) :
	_conn(conn),
	_url(url),
	_handle(NULL),
	_status(NOT_PERFORMED_YET),
	_cancelFlag(false),
	_progress(0),
	_downloadedBytes(0)
{}

HttpRequest::HttpRequest(HttpConnection& conn, const std::string& url, const std::string& destFilename) :
	_conn(conn),
	_url(url),
	_handle(NULL),
	_status(NOT_PERFORMED_YET),
	_destFilename(destFilename),
	_cancelFlag(false),
	_progress(0),
	_downloadedBytes(0)
{}

void HttpRequest::InitRequest()
{
	// Init the curl session
	_handle = curl_easy_init();

	// specify URL to get
	curl_easy_setopt(_handle, CURLOPT_URL, _url.c_str());

	// Connect the callback
	if (!_destFilename.empty())
	{
		curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, HttpRequest::WriteFileCallback);
	}
	else
	{
		curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, HttpRequest::WriteMemoryCallback);
	}

	// We pass ourselves as user data pointer to the callback function
	curl_easy_setopt(_handle, CURLOPT_WRITEDATA, this);

	// Set agent
	std::string agent = "The Dark Mod Updater / libtdm_update v" + std::string(LIBTDM_UPDATE_VERSION);
	curl_easy_setopt(_handle, CURLOPT_USERAGENT, agent.c_str());

	// Get the proxy from the HttpConnection class
	if (_conn.HasProxy())
	{
		curl_easy_setopt(_handle, CURLOPT_PROXY, _conn.GetProxyHost().c_str());
		curl_easy_setopt(_handle, CURLOPT_PROXYUSERPWD, (_conn.GetProxyUsername() + ":" + _conn.GetProxyPassword()).c_str());
	}
}

void HttpRequest::Perform()
{
	_errorMessage.clear();

	tdm::TraceLog::WriteLine(LOG_VERBOSE, "Initiating Download from " + _url);

	InitRequest();

	_progress = 0;
	_status = IN_PROGRESS;

	// Check target file
	if (!_destFilename.empty())
	{
		_destStream.open(_destFilename.c_str(), std::ofstream::out|std::ofstream::binary);
	}

	CURLcode result = curl_easy_perform(_handle);

	if (!_destFilename.empty())
	{
		_destStream.flush();
		_destStream.close();
	}

	if (_cancelFlag)
	{
		_status = ABORTED;
	}
	else
	{
		switch (result)
		{
		case CURLE_OK:
			_status = OK;
			_progress = 1.0;
			tdm::TraceLog::WriteLine(LOG_VERBOSE, "Download successful: " + _url);
			break;
		default:
			_status = FAILED;
			_errorMessage = curl_easy_strerror(result);
			tdm::TraceLog::WriteLine(LOG_VERBOSE, "Download failed: " + _errorMessage);
		};
	}

	curl_easy_cleanup(_handle);

	_handle = NULL;
}

void HttpRequest::Cancel()
{
	// The memory callback will catch this flag
	_cancelFlag = true;
}

HttpRequest::RequestStatus HttpRequest::GetStatus()
{
	return _status;
}

std::string HttpRequest::GetErrorMessage()
{
	return _errorMessage;
}

double HttpRequest::GetProgressFraction()
{
	return _progress;
}

double HttpRequest::GetDownloadSpeed()
{
	return _downloadSpeed;
}

std::size_t HttpRequest::GetDownloadedBytes()
{
	return _downloadedBytes;
}

std::string HttpRequest::GetResultString()
{
	return _buffer.empty() ? "" : std::string(&_buffer.front());
}

void HttpRequest::UpdateProgress()
{
	double size;
	double downloaded;
	CURLcode result = curl_easy_getinfo(_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);

	if (result != CURLE_OK) 
	{
		_progress = 0;
		return;
	}

	result = curl_easy_getinfo(_handle, CURLINFO_SIZE_DOWNLOAD, &downloaded);

	if (result != CURLE_OK) 
	{
		_progress = 0;
		return;
	}

	_downloadedBytes = static_cast<std::size_t>(downloaded);

	_progress = downloaded / size;

	if (_progress > 1.0)
	{
		_progress = 1.0;
	}

	result = curl_easy_getinfo(_handle, CURLINFO_SPEED_DOWNLOAD, &_downloadSpeed);

	if (result != CURLE_OK) 
	{
		_downloadSpeed = 0;
		return;
	}
}

size_t HttpRequest::WriteMemoryCallback(void* ptr, size_t size, size_t nmemb, HttpRequest* self)
{
	if (self->_cancelFlag)
	{
		return 0; // cancel the process
	}

	// Needed size
	std::size_t bytesToCopy = size * nmemb;

	std::vector<char>& buf = self->_buffer; // shortcut 

	std::size_t appendPosition = buf.size() > 0 ? buf.size() - 2 : 0;

	// The first allocation should request one extra byte for the trailing \0
	self->_buffer.resize(buf.size() > 0 ? buf.size() + bytesToCopy : bytesToCopy + 1);

	// Push the bytes
	std::memcpy(&(buf[appendPosition]), ptr, bytesToCopy);

	// Append trailing \0 if possible
	if (buf.size() > 0)
	{
		buf[buf.size() - 1] = 0;
	}

	self->UpdateProgress();

	return static_cast<size_t>(bytesToCopy);
}

size_t HttpRequest::WriteFileCallback(void* ptr, size_t size, size_t nmemb, HttpRequest* self)
{
	if (self->_cancelFlag)
	{
		return 0; // cancel the process
	}

	// Needed size
	std::size_t bytesToCopy = size * nmemb;

	self->_destStream.write(static_cast<const char*>(ptr), bytesToCopy);

	self->UpdateProgress();

	return static_cast<size_t>(bytesToCopy);
}

}
