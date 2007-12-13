#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "../game/game_local.h"
#include "../idlib/precompiled.h"
#include "framework/filesystem.h"
#include "renderpipeposix.h"

CRenderPipePosix::CRenderPipePosix() {
	// We want the filename of the render "pipe" to be "/dev/shm/tdm_lg_render.tga". To make a relative
	// path to that location, we need to count the slashes (and hence directories) in fs_savepath, and prepend
	// the appropriate number of "../"s to the filename.
	
	// The first parameter here is arbitrary. We're not interested in that filename, we just want fs_savepath.
	const char* ospath = fileSystem->RelativePathToOSPath("somerandomfilename", "fs_savepath");
	
	// Scan until we get a null terminator
	const char* ospath_ptr = ospath;
	int slashes=0;
	while ((*ospath_ptr) != '\0') {
		if ((*ospath_ptr) == '/') slashes++;
		ospath_ptr++;
	}
	
	// Append "../" to the path (slashes-1) times, and we'll be at the root of the filesystem.
	char* filename_ptr = m_filename;
	while (slashes>1) {
		slashes--;
		*filename_ptr = '.'; filename_ptr++;
		*filename_ptr = '.'; filename_ptr++;
		*filename_ptr = '/'; filename_ptr++;
	}
	
	// Finally, append the pathname we want (this includes the null terminator)
	strcpy(filename_ptr, "dev/shm/tdm_lg_render.tga");
	
	// m_filename now contains the required path, so open m_fd to point to it
	// O_CREAT: If the file doesn't exist, create it instead of failing.
	// O_RDONLY: Read-only (we don't need to write using this file descriptor).
	// O_NOATIME: Don't update the access time of the file. Supposedly faster.
	m_fd = open(m_filename, O_CREAT|O_RDONLY|O_NOATIME);
	
	// If an error occurs, save the error code in m_fd, but negative so we can
	// tell it apart from a successfully opened descriptor.
	if (m_fd < 0) {
		gameLocal.Error("Failed to open file: %s", strerror(errno));
		m_fd = -errno;
	}
}

CRenderPipePosix::~CRenderPipePosix() {
	if (m_fd >= 0) close(m_fd);
}

const char* CRenderPipePosix::FileName() {
	return m_filename;
}

int CRenderPipePosix::Read(void *buf, int *size) {
	int bytesRead = 0;
	
	if (m_fd < 0)
	{
		// Error encountered while opening file (see above)
		gameLocal.Error("Error while opening file was: %s", strerror(errno));
		return -m_fd;
	}
	
	while(bytesRead < *size) {
		int retVal = read(m_fd, (void*)((int)buf + bytesRead), *size - bytesRead);
		if (!retVal) break; // EOF
		if (retVal < 0) return errno; // Error
		bytesRead += retVal; // Got data. Loop.
	}
	
	*size = bytesRead;
	
	// Rewind
	if (lseek(m_fd, 0, SEEK_SET)) return errno; // Error
	
	return 0;
}
