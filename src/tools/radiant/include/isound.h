#ifndef ISOUND_H_
#define ISOUND_H_

#include "modulesystem.h"
#include "generic/constant.h"
#include "generic/callbackfwd.h"
#include "autoptr.h"
#include <vector>
#include <string>

const float METERS_PER_UNIT = 0.0254f;
const float UNITS_PER_METER = 1 / METERS_PER_UNIT;

// The min and max radii of a misc_sound entity
class SoundRadii
{
		float minRad, maxRad;
	public:
		//set sound radii either in metres or in inch on initialization might cause a conversion
		SoundRadii (float min = 0, float max = 0, bool inMetres = false)
		{
			if (inMetres) {
				minRad = min * UNITS_PER_METER;
				maxRad = max * UNITS_PER_METER;
			} else {
				minRad = min;
				maxRad = max;
			}
		}

		// set the sound radii in metres or in inch, might cause a conversion
		void setMin (float min, bool inMetres = false)
		{
			if (inMetres) {
				minRad = min * UNITS_PER_METER;
			} else {
				minRad = min;
			}
		}

		void setMax (float max, bool inMetres = false)
		{
			if (inMetres) {
				maxRad = max * UNITS_PER_METER;
			} else {
				maxRad = max;
			}
		}

		float getMin (bool inMetres = false) const
		{
			return (inMetres) ? minRad * METERS_PER_UNIT : minRad;
		}

		float getMax (bool inMetres = false) const
		{
			return (inMetres) ? maxRad * METERS_PER_UNIT : maxRad;
		}
};

/**
 * Sound manager interface.
 */
class ISoundManager
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "sound");

		virtual ~ISoundManager () {}

		/** greebo: Plays the given sound file (defined by its VFS path).
		 *
		 * @returns: TRUE, if the sound file was found at the given VFS path,
		 * 			 FALSE otherwise
		 */
		virtual bool playSound (const std::string& fileName) = 0;

		/** greebo: Stops the currently played sound.
		 */
		virtual void stopSound () = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<ISoundManager> GlobalSoundManagerModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<ISoundManager> GlobalSoundManagerModuleRef;

// Accessor method
inline ISoundManager& GlobalSoundManager ()
{
	return GlobalSoundManagerModule::getTable();
}

#endif /*ISOUND_H_*/
