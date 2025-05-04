#include "MumblePlugin_v_1_0_x.h"

#include "PluginComponents_v_1_0_x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WOW_EXE "wow.exe" // lowercase
#ifndef _WIN32
#	include <fcntl.h>
#	include <unistd.h>
#	include <ctype.h>

// Wine process names
#	define WINE_PRELOADER "wine-preloader"
#	define WINE_PROCESS "wine"
#endif

struct MumbleAPI_v_1_0_x mumbleAPI;
mumble_plugin_id_t ownID;

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

#define SET_TO_ZERO(name) \
	name[0] = 0.0f;       \
	name[1] = 0.0f;       \
	name[2] = 0.0f
bool mumble_fetchPositionalData(float *avatarPos, float *avatarDir,
								float *avatarAxis, float *cameraPos,
								float *cameraDir, float *cameraAxis,
								const char **context, const char **identity) {
	// If unable to provide positional data, this function should return false
	// and reset all given values to 0 / empty Strings
	SET_TO_ZERO(avatarPos);
	SET_TO_ZERO(avatarDir);
	SET_TO_ZERO(avatarAxis);
	SET_TO_ZERO(cameraPos);
	SET_TO_ZERO(cameraDir);
	SET_TO_ZERO(cameraAxis);
	*context  = "context!";
	*identity = "identity!";

	// This function returns whether it can continue to deliver positional data
	return false;
}
#undef SET_TO_ZERO

void mumble_shutdownPositionalData() {
}
