#pragma once

#include "Util.h"

namespace tdm
{

class DownloadProgressHandler :
	public updater::Updater::DownloadProgress
{
private:
	updater::CurDownloadInfo _info;

	UpdaterDialog& _dialog;

public:
	DownloadProgressHandler(UpdaterDialog& dialog) :
		_dialog(dialog)
	{}

	void OnProgress(const updater::CurDownloadInfo& info)
	{
		if (!_info.file.empty() && info.file != _info.file)
		{
			// Finish up the recent progress meter
			_info.progressFraction = 1.0f;
			PrintProgress();

			// Add a line break when a new file starts
			TraceLog::WriteLine(LOG_STANDARD, "");

			PrintFileInfo(info);
		}
		else if (_info.file.empty())
		{
			// First file
			PrintFileInfo(info);
		}

		_info = info;

		// Print the new info
		PrintProgress();
	}

	void OnFinish()
	{
		if (!_info.file.empty())
		{
			_info.progressFraction = 1.0f;
			PrintProgress();
		}

		// Add a line break when downloads are done
		TraceLog::WriteLine(LOG_STANDARD, "");
	}

private:
	void PrintFileInfo(const updater::CurDownloadInfo& info)
	{
		std::string text = (boost::format(" Downloading from Mirror %s: %s") % info.mirrorDisplayName % info.file.string()).str();

		_dialog.SetProgressText(text);
		TraceLog::WriteLine(LOG_STANDARD, text);
	}

	void PrintProgress()
	{
		TraceLog::Write(LOG_STANDARD, "\r");

		_dialog.SetProgress(_info.progressFraction);
		
		std::size_t numTicks = static_cast<std::size_t>(floor(_info.progressFraction * 40));

		std::string progressBar(numTicks, '=');
		std::string progressSpace(40 - numTicks, ' ');

		TraceLog::Write(LOG_STANDARD, " [" + progressBar + progressSpace + "]");

		TraceLog::Write(LOG_STANDARD, (boost::format(" %2.1f%%") % (_info.progressFraction*100)).str());

		TraceLog::Write(LOG_STANDARD, " at " + Util::GetHumanReadableBytes(static_cast<std::size_t>(_info.downloadSpeed)) + "/sec ");
	}
};

}
