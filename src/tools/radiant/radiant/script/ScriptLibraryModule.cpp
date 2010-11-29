#include "ScriptLibrary.h"

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<ScriptLibrary> ScriptLibraryModule;
typedef Static<ScriptLibraryModule> StaticScriptLibraryModule;
StaticRegisterModule staticRegisterScriptLibrary (StaticScriptLibraryModule::instance ());
