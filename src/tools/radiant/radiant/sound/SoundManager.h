#ifndef SOUNDMANAGER_H_
#define SOUNDMANAGER_H_

#include "SoundPlayer.h"

#include "isound.h"
#include "AutoPtr.h"
#include "generic/callback.h"
#include "gtkutil/widget.h"

namespace sound
{
	/**
	 * SoundManager implementing class.
	 */
	class SoundManager: public ISoundManager
	{
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

			virtual ~SoundManager ();

			/** greebo: Plays the sound file. Tries to resolve the filename's
			 * 			extension by appending .ogg or .wav and such.
			 */
			virtual bool playSound (const std::string& fileName);

			/** greebo: Stops the playback immediately.
			 */
			virtual void stopSound ();

			/** tachop: Switches Sound Playback Enabled flag.
			 */
			virtual void switchPlaybackEnabledFlag();

			/** tachop: Returns if Sound Playback is enabled.
			 */
			virtual bool isPlaybackEnabled ( ){ return playbackEnabled;};
	};

	typedef AutoPtr<SoundManager> SoundManagerPtr;
}

bool GlobalSoundManager_isPlaybackEnabled(void);

void GlobalSoundManager_switchPlaybackEnabledFlag(void);

extern ToggleItem g_soundPlaybackEnabled_button;

#endif /*SOUNDMANAGER_H_*/
