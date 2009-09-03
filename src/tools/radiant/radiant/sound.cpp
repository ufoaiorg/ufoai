/*!
 *  \file sound.cpp
 *
 *  Created on: 30/08/2009
 *      Author: Martin
 */
#include "modulesystem.h"
#include <modulesystem/moduleregistry.h>
#include "isound.h"
#include <modulesystem/singletonmodule.h>
#include <sound/soundmanager.h>

class SoundManagerAPI
{
		ISoundManager * m_soundmanager;
	public:
		typedef ISoundManager Type;
		STRING_CONSTANT(Name, "*");

		SoundManagerAPI () : m_soundmanager(0)
		{
			m_soundmanager = new sound::SoundManager();
		}

		ISoundManager* getTable ()
		{
			return m_soundmanager;
		}
};

typedef SingletonModule<SoundManagerAPI> SoundModule;
typedef Static<SoundModule> StaticSoundModule;
StaticRegisterModule staticRegisterSound(StaticSoundModule::instance());
