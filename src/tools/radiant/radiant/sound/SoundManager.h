#ifndef SOUNDMANAGER_H_
#define SOUNDMANAGER_H_

#include "SoundPlayer.h"

#include "isound.h"
#include "AutoPtr.h"
#include "generic/callback.h"
#include "gtkutil/widget.h"

namespace sound {
/**
 * SoundManager implementing class.
 */
class SoundManager: public ISoundManager
{
	public:

		typedef ISoundManager Type;
		STRING_CONSTANT(Name, "*");

		ISoundManager* getTable ()
		{
			return this;
		}

	private:

		// The helper class for playing the sounds
		SoundPlayer _soundPlayer;

		// Enables or Disables playback globally for the plugin.
		bool playbackEnabled;

		// In case the plugin is disabled while playing, this
		// file will be resumed if reenabled.
		std::string resumingFileNameToBePlayed;

	public:
		/**
		 * Main constructor.
		 */
		SoundManager ();

		~SoundManager ();

		/** greebo: Plays the sound file. Tries to resolve the filename's
		 * 			extension by appending .ogg or .wav and such.
		 */
		bool playSound (const std::string& fileName);

		/** greebo: Stops the playback immediately.
		 */
		void stopSound ();

		/** tachop: Switches Sound Playback Enabled flag.
		 */
		void switchPlaybackEnabledFlag ();

		/** tachop: Returns if Sound Playback is enabled.
		 */
		bool isPlaybackEnabled ();
};

typedef AutoPtr<SoundManager> SoundManagerPtr;
}

void GlobalSoundManager_switchPlaybackEnabledFlag ();

#endif /*SOUNDMANAGER_H_*/
