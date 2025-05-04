#include "MumblePlugin_v_1_0_x.h"

#include "PluginComponents_v_1_0_x.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WOW_EXE "wow.exe" // lowercase
#ifndef _WIN32
#	include <fcntl.h>
#	include <unistd.h>
#	include <ctype.h>
#	include <sys/uio.h>

// Wine process names
#	define WINE_PRELOADER "wine-preloader"
#	define WINE_PROCESS "wine"
#endif

// Add this near the top of your file, after your includes but before using
// procid_t
#ifdef _WIN32
typedef DWORD procid_t;   // Windows process ID type
typedef LPVOID procptr_t; // Windows process pointer type
#else
typedef pid_t procid_t;  // Unix/Linux process ID type
typedef void *procptr_t; // Unix/Linux process pointer type
#endif

struct MumbleAPI_v_1_0_x mumbleAPI;
mumble_plugin_id_t ownID;

// Process ID for the target process (WoW)
static procid_t pPid = 0;

// Function to read memory from a process
static inline bool peekProc(const procptr_t addr, void *dest,
							const size_t len) {
#ifdef _WIN32
	// On Windows, use the Mumble API to read memory
	return (mumbleAPI.readFromProcess(ownID, addr, dest, len)
			== MUMBLE_STATUS_OK);
#else
	// On Linux/Unix, use process_vm_readv
	struct iovec in;
	in.iov_base = (void *) (addr); // Address from target process
	in.iov_len  = len;             // Length

	struct iovec out;
	out.iov_base = dest;
	out.iov_len  = len;

	ssize_t nread = process_vm_readv(pPid, &out, 1, &in, 1, 0);

	return (nread != -1 && (size_t) nread == len);
#endif
}

mumble_error_t mumble_init(mumble_plugin_id_t pluginID) {
	ownID = pluginID;

	if (mumbleAPI.log(ownID, "Wow335 Positional Audio loaded")
		!= MUMBLE_STATUS_OK) {
		// Logging failed -> usually you'd probably want to log things like this
		// in your plugin's logging system (if there is any)
	}

	return MUMBLE_STATUS_OK;
}

void mumble_shutdown() {
	if (mumbleAPI.log(ownID, "Wow335 Positional Audio unloaded")
		!= MUMBLE_STATUS_OK) {
		// Logging failed -> usually you'd probably want to log things like this
		// in your plugin's logging system (if there is any)
	}
}

struct MumbleStringWrapper mumble_getName() {
	static const char *name = "Wow335 Positional Audio";

	struct MumbleStringWrapper wrapper;
	wrapper.data           = name;
	wrapper.size           = strlen(name);
	wrapper.needsReleasing = false;

	return wrapper;
}

mumble_version_t mumble_getAPIVersion() {
	// This constant will always hold the API version  that fits the included
	// header files
	return MUMBLE_PLUGIN_API_VERSION;
}

void mumble_registerAPIFunctions(void *apiStruct) {
	// Provided mumble_getAPIVersion returns MUMBLE_PLUGIN_API_VERSION, this
	// cast will make sure that the passed pointer will be cast to the proper
	// type
	mumbleAPI = MUMBLE_API_CAST(apiStruct);
}

void mumble_releaseResource(const void *pointer) {
	// As we never pass a resource to Mumble that needs releasing, this function
	// should never get called
	printf("Called mumble_releaseResource but expected that this never gets "
		   "called -> Aborting");
	abort();
}

// Below functions are not strictly necessary but every halfway serious plugin
// should implement them nonetheless

mumble_version_t mumble_getVersion() {
	// below should match the description in the manifest.xml and CmakeLists.txt
	// file
	mumble_version_t version;
	version.major = 0;
	version.minor = 1;
	version.patch = 0;

	return version;
}

struct MumbleStringWrapper mumble_getAuthor() {
	static const char *author = "sogladev";

	struct MumbleStringWrapper wrapper;
	wrapper.data           = author;
	wrapper.size           = strlen(author);
	wrapper.needsReleasing = false;

	return wrapper;
}

struct MumbleStringWrapper mumble_getDescription() {
	static const char *description = "A positional audio plugin for World of "
									 "Warcraft (x86) version 3.3.5a.12340";

	struct MumbleStringWrapper wrapper;
	wrapper.data           = description;
	wrapper.size           = strlen(description);
	wrapper.needsReleasing = false;

	return wrapper;
}

// Positional audio

uint32_t mumble_getFeatures() {
	return MUMBLE_FEATURE_POSITIONAL;
}

// Helper function for Linux to check if a Wine process is running WoW
#ifndef _WIN32
bool isWineRunningWow(uint64_t pid) {
	char cmdlinePath[256];
	char buffer[512];
	snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%llu/cmdline",
			 (unsigned long long) pid);

	FILE *cmdlineFile = fopen(cmdlinePath, "r");
	if (!cmdlineFile) {
		return false;
	}

	size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, cmdlineFile);
	fclose(cmdlineFile);

	if (bytesRead == 0) {
		return false;
	}

	// Ensure null-termination
	buffer[bytesRead] = '\0';

	// Make the buffer lowercase for case-insensitive comparison
	for (size_t i = 0; i < bytesRead; i++) {
		buffer[i] = tolower(buffer[i]);
	}

	return (strstr(buffer, WOW_EXE) != NULL);
}
#endif

uint8_t mumble_initPositionalData(const char *const *programNames,
								  const uint64_t *programPIDs,
								  size_t programCount) {
	char logBuffer[256];
#ifdef _WIN32
	// Windows direct check
	bool found = false;
	for (size_t i = 0; i < programCount; i++) {
		if (_stricmp(programNames[i], WOW_EXE) == 0) {
			found = true;
			pPid  = programPIDs[i]; // Store the process ID
			snprintf(logBuffer, sizeof(logBuffer),
					 "Found direct WoW process: %s (PID: %llu)",
					 programNames[i], (unsigned long long) programPIDs[i]);
			mumbleAPI.log(ownID, logBuffer);
			break;
		}
	}
	if (!found) {
		return MUMBLE_PDEC_ERROR_TEMP; // try again later
	}

	return MUMBLE_PDEC_OK;
#else
	// Linux/Wine check
	// If we see very few processes, Mumble likely lacks permissions
	if (programCount < 50) {
		mumbleAPI.log(ownID,
					  "ERROR: Detected only a small number of processes!");
		mumbleAPI.log(ownID, "This usually indicates that Mumble lacks "
							 "permission to read process information.");
		return MUMBLE_PDEC_ERROR_PERM;
	}

	// Case-insensitive check for the game executable name
	bool found = false;
	for (size_t i = 0; i < programCount; i++) {
		// Check for direct match first (rare)
		if (strcasecmp(programNames[i], WOW_EXE) == 0) {
			found = true;
			pPid  = programPIDs[i]; // Store the process ID
			snprintf(logBuffer, sizeof(logBuffer),
					 "Found direct WoW process: %s (PID: %llu)",
					 programNames[i], (unsigned long long) programPIDs[i]);
			mumbleAPI.log(ownID, logBuffer);
			break;
		}
		// Check if this is a Wine process
		else if (strcasecmp(programNames[i], WINE_PRELOADER) == 0
				 || strcasecmp(programNames[i], WINE_PROCESS) == 0) {
			// Check if this Wine process is running WoW
			if (isWineRunningWow(programPIDs[i])) {
				found = true;
				pPid  = programPIDs[i]; // Store the process ID
				snprintf(logBuffer, sizeof(logBuffer),
						 "Found WoW running under Wine: %s (PID: %llu)",
						 programNames[i], (unsigned long long) programPIDs[i]);
				mumbleAPI.log(ownID, logBuffer);
				break;
			}
		}
	}
	if (!found) {
		return MUMBLE_PDEC_ERROR_TEMP; // try again later
	}

	return MUMBLE_PDEC_OK;
#endif
}

void mumble_shutdownPositionalData() {
}

#define SET_TO_ZERO(name) \
	name[0] = 0.0f;       \
	name[1] = 0.0f;       \
	name[2] = 0.0f
bool mumble_fetchPositionalData(float *avatarPos, float *avatarDir,
								float *avatarAxis, float *cameraPos,
								float *cameraDir, float *cameraAxis,
								const char **context, const char **identity) {
	// Memory addresses
	uint32_t state_address          = 0x00BD0792;
	uint32_t avatar_pos_address     = 0x00ADF4E4;
	uint32_t avatar_heading_address = 0x00BEBA70;
	uint32_t camera_pos_address = 0x00ADF4E4; // Same as avatar position in WoW
	uint32_t camera_front_address = 0x00ADF5F0;
	uint32_t camera_top_address   = 0x00ADF554;
	uint32_t player_address       = 0x00C79D18;
	uint32_t mapid_address        = 0x00AB63BC;
	uint32_t leaderguid_address   = 0x00BD1968;

	// Temporary variables to hold data read from the game
	char state                      = 0;
	float avatar_pos_corrector[3]   = { 0.0f, 0.0f, 0.0f };
	float camera_pos_corrector[3]   = { 0.0f, 0.0f, 0.0f };
	float avatar_heading            = 0.0f;
	float camera_front_corrector[3] = { 0.0f, 0.0f, 0.0f };
	float camera_top_corrector[3]   = { 0.0f, 0.0f, 0.0f };
	char player[50]                 = { 0 };
	int mapId                       = 0;
	int leaderGUID                  = 0;

	// Static buffers for context and identity strings
	static char context_buffer[256]  = { 0 };
	static char identity_buffer[256] = { 0 };

	// Reading values from game memory using peekProc
	bool ok =
		peekProc((procptr_t) state_address, &state, 1)
		&& peekProc((procptr_t) avatar_pos_address, avatar_pos_corrector, 12)
		&& peekProc((procptr_t) avatar_heading_address, &avatar_heading, 4)
		&& peekProc((procptr_t) camera_pos_address, camera_pos_corrector, 12)
		&& peekProc((procptr_t) camera_front_address, camera_front_corrector,
					12)
		&& peekProc((procptr_t) camera_top_address, camera_top_corrector, 12)
		&& peekProc((procptr_t) player_address, player, 50)
		&& peekProc((procptr_t) mapid_address, &mapId, 4)
		&& peekProc((procptr_t) leaderguid_address, &leaderGUID, 4);

	// Reset all vectors if any read failed or not in game
	if (!ok || state != 1) {
		SET_TO_ZERO(avatarPos);
		SET_TO_ZERO(avatarDir);
		SET_TO_ZERO(avatarAxis);
		SET_TO_ZERO(cameraPos);
		SET_TO_ZERO(cameraDir);
		SET_TO_ZERO(cameraAxis);

		// Clear context and identity when not in game
		strcpy(context_buffer, "{}");
		strcpy(identity_buffer, "{}");
		*context  = context_buffer;
		*identity = identity_buffer;

		return true; // Return true to keep trying
	}

	// Build context JSON
	snprintf(context_buffer, sizeof(context_buffer), "{\n\"map\": %d\n}",
			 mapId);
	*context = context_buffer;

	// Build identity JSON
	// Ensure null-termination of player name
	player[49] = '\0';

	// Simple sanitize player name for JSON
	for (int i = 0; player[i]; i++) {
		if (player[i] == '"' || player[i] == '\\') {
			player[i] = ' ';
		}
	}

	snprintf(identity_buffer, sizeof(identity_buffer),
			 "{\n\"char\": \"%s\",\n\"leaderguid\": %d\n}",
			 player[0] ? player : "None", leaderGUID);
	*identity = identity_buffer;

	// Convert coordinates from WoW to Mumble coordinate system
	// WoW -> Mumble: X=Z, Y=-X, Z=Y
	avatarPos[0] = -avatar_pos_corrector[1];
	avatarPos[1] = avatar_pos_corrector[2];
	avatarPos[2] = avatar_pos_corrector[0];

	cameraPos[0] = -camera_pos_corrector[1];
	cameraPos[1] = camera_pos_corrector[2];
	cameraPos[2] = camera_pos_corrector[0];

	// Avatar direction from heading
	avatarDir[0] = -sinf(avatar_heading);
	avatarDir[1] = 0.0f;
	avatarDir[2] = cosf(avatar_heading);

	// Avatar axis (up vector)
	avatarAxis[0] = 0.0f;
	avatarAxis[1] = 1.0f;
	avatarAxis[2] = 0.0f;

	// Camera direction (use avatar heading)
	cameraDir[0] = -sinf(avatar_heading);
	cameraDir[1] = 0.0f;
	cameraDir[2] = cosf(avatar_heading);

	// Camera axis (up vector)
	cameraAxis[0] = -camera_top_corrector[1];
	cameraAxis[1] = camera_top_corrector[2];
	cameraAxis[2] = camera_top_corrector[0];

	return true;
}
#undef SET_TO_ZERO
