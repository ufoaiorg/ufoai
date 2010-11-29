#include "SoundManager.h"

#include "ifilesystem.h"
#include "archivelib.h"
#include "generic/callback.h"
#include "os/path.h"

#include <iostream>

namespace sound {
// Constructor
SoundManager::SoundManager ()
{
	playbackEnabled = false;
	resumingFileNameToBePlayed = ""; //  "maledeath01.ogg";  for testing :)
}

// Destructor
SoundManager::~SoundManager ()
{
}

bool SoundManager::playSound (const std::string& fileName)
{
	if (!playbackEnabled)
		return true;

	// Make a copy of the filename
	std::string name = fileName;

	// Try to open the file as it is
	AutoPtr<ArchiveFile> file(GlobalFileSystem().openFile(name));
	std::cout << "Trying: " << name << std::endl;
	if (file) {
		// File found, play it
		std::cout << "Found file: " << name << std::endl;
		resumingFileNameToBePlayed = fileName;
		if (playbackEnabled)
			_soundPlayer.play(*file);
		return true;
	}

	// File not found, try to strip the extension
	std::string root = os::getExtension(name);

	// Try to open the .ogg variant
	name = root + ".ogg";
	std::cout << "Trying: " << name << std::endl;
	AutoPtr<ArchiveFile> oggFile(GlobalFileSystem().openFile(name));
	if (oggFile) {
		std::cout << "Found file: " << name << std::endl;
		resumingFileNameToBePlayed = fileName;
		if (playbackEnabled)
			_soundPlayer.play(*oggFile);
		return true;
	}

	// Try to open the file with .wav extension
	name = root + ".wav";
	std::cout << "Trying: " << name << std::endl;
	AutoPtr<ArchiveFile> wavFile(GlobalFileSystem().openFile(name));
	if (wavFile) {
		std::cout << "Found file: " << name << std::endl;
		resumingFileNameToBePlayed = fileName;
		if (playbackEnabled)
			_soundPlayer.play(*wavFile);
		return true;
	}

	// File not found
	return false;
}

void SoundManager::stopSound ()
{
	_soundPlayer.stop();
	resumingFileNameToBePlayed = "";
}

void SoundManager::switchPlaybackEnabledFlag ()
{
	playbackEnabled = !playbackEnabled;
	if (playbackEnabled && resumingFileNameToBePlayed.length())
		playSound(resumingFileNameToBePlayed);
}

bool SoundManager::isPlaybackEnabled ()
{
	return playbackEnabled;
}

} // namespace sound

void GlobalSoundManager_switchPlaybackEnabledFlag ()
{
	GlobalSoundManager().switchPlaybackEnabledFlag();
}
