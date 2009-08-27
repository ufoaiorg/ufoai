#ifndef SOUNDMANAGER_H_
#define SOUNDMANAGER_H_

#include "soundplayer.h"

#include "isound.h"
#include "autoptr.h"

namespace sound
{
	/**
	 * SoundManager implementing class.
	 */
	class SoundManager: public ISoundManager
	{
			// The helper class for playing the sounds
			SoundPlayer _soundPlayer;

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
	};

	typedef AutoPtr<SoundManager> SoundManagerPtr;
}

#endif /*SOUNDMANAGER_H_*/
