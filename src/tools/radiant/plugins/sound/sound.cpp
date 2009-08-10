#include "soundmanager.h"
#include "iplugin.h"
#include "ifilesystem.h"
#include "typesystem.h"
#include "modulesystem/singletonmodule.h"

class SoundManagerPluginDependencies : public GlobalFileSystemModuleRef {
};

class SoundManagerModule: public TypeSystemRef
{
		_QERPluginTable m_plugin;
	public:
		typedef _QERPluginTable Type;
		STRING_CONSTANT(Name, "SoundManager");

		SoundManagerModule ()
		{
/*			m_plugin.m_pfnQERPlug_Init = &sound::init;
			m_plugin.m_pfnQERPlug_GetName = &sound::getName;
			m_plugin.m_pfnQERPlug_GetCommandList = &sound::getCommandList;
			m_plugin.m_pfnQERPlug_GetCommandTitleList = &sound::getCommandTitleList;
			m_plugin.m_pfnQERPlug_Dispatch = &sound::dispatch;*/
		}
		_QERPluginTable* getTable ()
		{
			return &m_plugin;
		}
};

typedef SingletonModule<SoundManagerModule, SoundManagerPluginDependencies> SingletonSoundManagerModule;

SingletonSoundManagerModule g_SoundManagerModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules (ModuleServer& server)
{
	initialiseModule(server);

	g_SoundManagerModule.selfRegister();
};
