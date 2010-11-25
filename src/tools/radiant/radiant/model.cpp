/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include "picomodel.h"
typedef unsigned char byte;
#include <stdlib.h>
#include <algorithm>
#include <list>

#include "AutoPtr.h"
#include "iscenegraph.h"
#include "irender.h"
#include "iselection.h"
#include "iimage.h"
#include "imodel.h"
#include "igl.h"
#include "ifilesystem.h"
#include "iundo.h"
#include "ifiletypes.h"
#include "preferencesystem.h"
#include "stringio.h"
#include "iarchive.h"

#include "modulesystem/singletonmodule.h"
#include "stream/textstream.h"
#include "string/string.h"
#include "stream/stringstream.h"

#include "gtkutil/widget.h"
#include "picomodel/model.h"

static void PicoPrintFunc (int level, const char *str)
{
	if (str == 0)
		return;
	switch (level) {
	case PICO_NORMAL:
		g_message("%s\n", str);
		break;

	case PICO_VERBOSE:
		g_message("PICO_VERBOSE: %s\n", str);
		break;

	case PICO_WARNING:
		g_warning("PICO_WARNING: %s\n", str);
		break;

	case PICO_ERROR:
		g_warning("PICO_ERROR: %s\n", str);
		break;

	case PICO_FATAL:
		g_critical("PICO_FATAL: %s\n", str);
		break;
	}
}

static void PicoLoadFileFunc (char *name, byte **buffer, int *bufSize)
{
	*bufSize = static_cast<int> (GlobalFileSystem().loadFile(name, (void **)buffer));
}

static void PicoFreeFileFunc (void* file)
{
	GlobalFileSystem().freeFile(file);
}

static void pico_initialise (void)
{
	PicoInit();
	PicoSetMallocFunc(malloc);
	PicoSetFreeFunc(free);
	PicoSetPrintFunc(PicoPrintFunc);
	PicoSetLoadFileFunc(PicoLoadFileFunc);
	PicoSetFreeFileFunc(PicoFreeFileFunc);
}

class PicoModelLoader: public ModelLoader
{
		const picoModule_t* m_module;
	public:
		PicoModelLoader (const picoModule_t* module) :
			m_module(module)
		{
		}
		scene::Node& loadModel (ArchiveFile& file)
		{
			return loadPicoModel(m_module, file);
		}

		// Load the given model from the VFS path
		model::IModelPtr loadModelFromPath (const std::string& name)
		{
			// Open an ArchiveFile to load
			AutoPtr<ArchiveFile> file(GlobalFileSystem().openFile(name));
			if (file) {
				// Load the model and return the RenderablePtr
				return loadIModel(m_module, *file);
			}
			return model::IModelPtr();
		}
};

class ModelPicoDependencies: public GlobalFileSystemModuleRef,
		public GlobalOpenGLModuleRef,
		public GlobalUndoModuleRef,
		public GlobalSceneGraphModuleRef,
		public GlobalShaderCacheModuleRef,
		public GlobalSelectionModuleRef,
		public GlobalFiletypesModuleRef,
		public GlobalPreferenceSystemModuleRef
{
};

class ModelPicoAPI
{
		PicoModelLoader m_modelLoader;
	public:
		typedef ModelLoader Type;

		ModelPicoAPI (const std::string& extension, const picoModule_t* module) :
			m_modelLoader(module)
		{
			std::string filter = "*." + extension;
			GlobalFiletypesModule::getTable().addType(Type::Name(), extension, filetype_t(module->displayName,
					filter));
		}
		ModelLoader* getTable ()
		{
			return &m_modelLoader;
		}
};

class PicoModelAPIConstructor
{
		std::string m_extension;
		const picoModule_t* m_module;
	public:
		PicoModelAPIConstructor (const char* extension, const picoModule_t* module) :
			m_extension(extension), m_module(module)
		{
		}
		const char* getName ()
		{
			return m_extension.c_str();
		}
		ModelPicoAPI* constructAPI (ModelPicoDependencies& dependencies)
		{
			return new ModelPicoAPI(m_extension, m_module);
		}
		void destroyAPI (ModelPicoAPI* api)
		{
			delete api;
		}
};

typedef SingletonModule<ModelPicoAPI, ModelPicoDependencies, PicoModelAPIConstructor> PicoModelModule;

void ModelModules_Init (void)
{
	pico_initialise();
	const picoModule_t** modules = PicoModuleList(0);
	while (*modules != 0) {
		const picoModule_t* module = *modules++;
		if (module->canload && module->load) {
			for (const char* const * ext = module->defaultExts; *ext != 0; ++ext) {
				PicoModelModule *picomodule = new PicoModelModule(PicoModelAPIConstructor(*ext, module));
				StaticModuleRegistryList().instance().addModule(*picomodule);
				/** @todo do we have to delete these PicoModelModules anywhere? */
			}
		}
	}
}
