#include "AutoSaver.h"

#include <iostream>
#include "mapfile.h"
#include "iscenegraph.h"

#include <gdk/gdkwindow.h>

#include "os/file.h"
#include "string/string.h"
#include "map.h"
#include "../ui/mainframe/mainframe.h"
#include "os/path.h"

#include "radiant_i18n.h"

namespace map {

namespace {
	const std::string RKEY_AUTOSAVE_ENABLED = "user/ui/map/autoSaveEnabled";
	const std::string RKEY_AUTOSAVE_INTERVAL = "user/ui/map/autoSaveInterval";
	const std::string RKEY_AUTOSAVE_SNAPSHOTS_ENABLED = "user/ui/map/autoSaveSnapshots";
	const std::string RKEY_AUTOSAVE_SNAPSHOTS_FOLDER = "user/ui/map/snapshotFolder";
	const std::string RKEY_AUTOSAVE_MAX_SNAPSHOT_FOLDER_SIZE = "user/ui/map/maxSnapshotFolderSize";
}

AutoMapSaver::AutoMapSaver() :
	_enabled(GlobalRegistry().get(RKEY_AUTOSAVE_ENABLED) == "1"),
	_snapshotsEnabled(GlobalRegistry().get(RKEY_AUTOSAVE_SNAPSHOTS_ENABLED) == "1"),
	_interval(GlobalRegistry().getInt(RKEY_AUTOSAVE_INTERVAL) * 60),
	_timer(_interval * 1000, onIntervalReached, this),
	_changes(0)
{
	GlobalRegistry().addKeyObserver(this, RKEY_AUTOSAVE_INTERVAL);
	GlobalRegistry().addKeyObserver(this, RKEY_AUTOSAVE_SNAPSHOTS_ENABLED);
	GlobalRegistry().addKeyObserver(this, RKEY_AUTOSAVE_ENABLED);
}

AutoMapSaver::~AutoMapSaver() {
	stopTimer();
}

void AutoMapSaver::keyChanged(const std::string& changedKey, const std::string& newValue) {
	// Stop the current timer
	stopTimer();

	_enabled = (GlobalRegistry().get(RKEY_AUTOSAVE_ENABLED) == "1");
	_snapshotsEnabled = (GlobalRegistry().get(RKEY_AUTOSAVE_SNAPSHOTS_ENABLED) == "1");
	_interval = GlobalRegistry().getInt(RKEY_AUTOSAVE_INTERVAL) * 60;

	// Update the internal timer
	_timer.setTimeout(_interval * 1000);

	// Start the timer with the new interval
	if (_enabled) {
		startTimer();
	}
}

void AutoMapSaver::init() {
	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);
}

void AutoMapSaver::clearChanges() {
	_changes = 0;
}

void AutoMapSaver::startTimer() {
	_timer.enable();
}

void AutoMapSaver::stopTimer() {
	_timer.disable();
}

void AutoMapSaver::saveSnapshot() {
	// Original GtkRadiant comments:
	// we need to do the following
	// 1. make sure the snapshot directory exists (create it if it doesn't)
	// 2. find out what the lastest save is based on number
	// 3. inc that and save the map

	unsigned int maxSnapshotFolderSize =
		GlobalRegistry().getInt(RKEY_AUTOSAVE_MAX_SNAPSHOT_FOLDER_SIZE);

	// Sanity check in case there is something weird going on in the registry
	if (maxSnapshotFolderSize == 0) {
		maxSnapshotFolderSize = 100;
	}

	const std::string& mapName = GlobalMap().getName();
	std::string name = os::getFilenameFromPath(mapName);
	std::string extension = os::getExtension(mapName);

	// Append the the snapshot folder to the path
	std::string snapshotPath = os::stripFilename(mapName) + "/" + GlobalRegistry().get(RKEY_AUTOSAVE_SNAPSHOTS_FOLDER);

	// Check if the folder exists and create it if necessary
	if (file_exists(snapshotPath) || g_mkdir_with_parents(snapshotPath.c_str(), 0755)) {
		// Reset the size counter of the snapshots folder
		unsigned int folderSize = 0;

		// This holds the target path of the snapshot
		std::string filename;

		for (int nCount = 0; nCount < 5000; nCount++) {

			// Construct the base name without numbered extension
			filename = snapshotPath + mapName;

			std::cout << filename << "\n";
			// Now append the number and the map extension to the map name
			filename += ".";
			filename += string::toString(nCount);
			filename += ".map";

			if (file_exists(filename)) {
				// Add to the folder size
				folderSize += file_size(filename);
			}
			else {
				// We've found an unused filename, break the loop
				break;
			}
		}

		globalOutputStream() << "Autosaving snapshot to " << filename << "\n";

		// save in the next available slot
		GlobalMap().saveFile(filename);

		// Display a warning, if the folder size exceeds the limit
		if (folderSize > maxSnapshotFolderSize * 1024 * 1024) {
			globalOutputStream() << "AutoSaver: The snapshot files in " << snapshotPath;
			globalOutputStream() << " total more than " << maxSnapshotFolderSize;
			globalOutputStream() << " MB. You might consider cleaning up.\n";
		}
	}
	else {
		globalErrorStream() << "Snapshot save failed.. unable to create directory";
		globalErrorStream() << snapshotPath << "\n";
	}
}

void AutoMapSaver::checkSave() {

	// Check if the user is currently pressing a mouse button
	GdkModifierType mask;
	gdk_window_get_pointer(0, 0, 0, &mask);

	const unsigned int anyButton = (
		GDK_BUTTON1_MASK | GDK_BUTTON2_MASK |
		GDK_BUTTON3_MASK | GDK_BUTTON4_MASK |
		GDK_BUTTON5_MASK
	);

	// Don't start the save if the user is holding a mouse button
	if ((mask & anyButton) != 0) {
		return;
	}

	if (!GlobalMap().isValid() || !ScreenUpdates_Enabled()) {
		return;
	}

	// Check, if changes have been made since the last autosave
	if (_changes == Node_getMapFile(GlobalSceneGraph().root())->changes()) {
		return;
	}

	_changes = Node_getMapFile(GlobalSceneGraph().root())->changes();

	// Stop the timer before saving
	stopTimer();

	if (_enabled) {
		// only snapshot if not working on an unnamed map
		if (_snapshotsEnabled && !GlobalMap().isUnnamed()) {
			saveSnapshot();
		}
		else {
			if (GlobalMap().isUnnamed()) {
				// Generate an autosave filename
				std::string autoSaveFilename = map::getMapsPath();

				// Try to create the map folder, in case there doesn't exist one
				g_mkdir_with_parents(autoSaveFilename.c_str(), 755);

				// Append the "autosave.map" to the filename
				autoSaveFilename += "autosave.map";

				globalOutputStream() << "Autosaving unnamed map to " << autoSaveFilename << "\n";

				// Invoke the save call
				GlobalMap().saveFile(autoSaveFilename);
			}
			else {
				// Construct the new extension (e.g. ".autosave.map")
				std::string newExtension = ".autosave.map";

				// create the new autosave filename by changing the extension
				std::string autoSaveFilename = os::stripExtension(GlobalMap().getName());
				autoSaveFilename += newExtension;

				globalOutputStream() << "Autosaving map to " << autoSaveFilename << "\n";

				// Invoke the save call
				GlobalMap().saveFile(autoSaveFilename);
			}
		}
	}
	else {
		globalOutputStream() << "Autosave skipped.....\n";
	}

	// Re-start the timer after saving has finished
	startTimer();
}

void AutoMapSaver::constructPreferencePage(PreferenceGroup& group) {
	// Add a page to the given group
	PreferencesPage* page(group.createPage(_("Autosave"), _("Autosave Preferences")));

	// Add the checkboxes and connect them with the registry key and the according observer
	page->appendCheckBox("", _("Enable Autosave"), RKEY_AUTOSAVE_ENABLED);
	page->appendSlider(_("Autosave Interval (in minutes)"), RKEY_AUTOSAVE_INTERVAL, TRUE, 5, 1, 61, 1, 1, 1);

	page->appendCheckBox("", _("Save Snapshots"), RKEY_AUTOSAVE_SNAPSHOTS_ENABLED);
	page->appendEntry(_("Snapshot folder (relative to map folder)"), RKEY_AUTOSAVE_SNAPSHOTS_FOLDER);
	page->appendEntry(_("Max Snapshot Folder size (MB)"), RKEY_AUTOSAVE_MAX_SNAPSHOT_FOLDER_SIZE);
}

gboolean AutoMapSaver::onIntervalReached(gpointer data) {
	// Cast the passed pointer onto a self-pointer
	AutoMapSaver* self = reinterpret_cast<AutoMapSaver*>(data);

	self->checkSave();

	// Return true, so that the timer gets called again
	return true;
}

AutoMapSaver& AutoSaver() {
	static AutoMapSaver _autoSaver;
	return _autoSaver;
}

} // namespace map
