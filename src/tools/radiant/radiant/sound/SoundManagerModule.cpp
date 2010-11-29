#include "SoundManager.h"

#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"

typedef SingletonModule<sound::SoundManager> SoundModule;
typedef Static<SoundModule> StaticSoundModule;
StaticRegisterModule staticRegisterSound (StaticSoundModule::instance ());
