/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#ifndef _DOWNLOAD_H_
#define _DOWNLOAD_H_

#include "../Http/HttpRequest.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

/**
 * An object representing a single download.
 * The actual work will be performed in a separate thread,
 * hence the Start() method will not block execution.
 *
 * Check the GetStatus() and GetProgressFraction() methods
 * to get some information about the actual status.
 * 
 * Once the Download object is destroyed any ongoing 
 * worker thread will be terminated.
 *
 * Download data is stored in a temporary file until the whole
 * file is finished - then the temporary will be renamed to
 * the destination filename. If the destination filename is already
 * in use, the move will fail and the download will remain
 * in the temporary file. The temporary file is named the same
 * as the destination filename, but with a prefixed underscore character:
 * e.g. target/directory/_download.pk4
 */
class CDownload
{
private:
	// The target URLs we're trying to download from
	idStringList _urls;

	// The URL index
	int _curUrl;

	// destination file
	idStr _destFilename;

	// Temporary filename for partial download data
	idStr _tempFilename;

public:
	enum DownloadStatus
	{
		NOT_STARTED_YET,
		IN_PROGRESS,
		FAILED,
		SUCCESS,
	};

private:
	DownloadStatus _status;

	// The corresponding HTTP request
	CHttpRequestPtr _request;

	typedef boost::shared_ptr<boost::thread> ThreadPtr;
	ThreadPtr _thread;

	bool _pk4CheckEnabled;

public:
	/** 
	 * greebo: Construct a new Download using the given URL.
	 * The download data will be saved to the given destFilename;
	 */
	CDownload(const idStr& url, const idStr& destFilename);

	/**
	 * greebo: Construct a new Download using the given list of 
	 * alternative URLs. If downloading from the first URL fails
	 * the next one will be tried until no more alternatives are left.
	 * The result will be saved to destFilename.
	 */
	CDownload(const idStringList& urls, const idStr& destFilename);

	~CDownload();

	// Start the download. This will spawn a new thread and execution
	// will return to the calling code.
	void Start();

	// Cancel the download. If the download has already finished, this does nothing.
	void Stop();

	// Enable the validation of the downloaded file
	void EnableValidPK4Check(bool enable);
	
	// The current status of this download
	DownloadStatus GetStatus();

	double GetProgressFraction();

	// Check method
	static bool CheckValidPK4(const idStr& path);

private:
	// Thread entry point
	void Perform();
};
typedef boost::shared_ptr<CDownload> CDownloadPtr;

#endif /* _DOWNLOAD_H_ */
