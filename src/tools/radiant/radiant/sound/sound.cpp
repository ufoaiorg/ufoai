#include "isound.h"

#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"
#include "SoundManager.h"

class SoundManagerAPI
{
		ISoundManager * m_soundmanager;
	public:
		typedef ISoundManager Type;
		STRING_CONSTANT(Name, "*");

		SoundManagerAPI () :
			m_soundmanager(0)
		{
			m_soundmanager = new sound::SoundManager();
		}
		~SoundManagerAPI ()
		{
			delete m_soundmanager;
		}

		ISoundManager* getTable ()
		{
			return m_soundmanager;
		}
};

typedef SingletonModule<SoundManagerAPI> SoundModule;
typedef Static<SoundModule> StaticSoundModule;
StaticRegisterModule staticRegisterSound(StaticSoundModule::instance());
