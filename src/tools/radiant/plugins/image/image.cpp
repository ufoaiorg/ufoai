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

#include "image.h"

#include "ifilesystem.h"
#include "iimage.h"

#include "jpeg.h"
#include "tga.h"

#include "modulesystem/singletonmodule.h"

class ImageDependencies : public GlobalFileSystemModuleRef {
};

class ImageTGAAPI {
	_QERPlugImageTable m_imagetga;
public:
	typedef _QERPlugImageTable Type;
	STRING_CONSTANT(Name, "tga");

	ImageTGAAPI() {
		m_imagetga.loadImage = LoadTGA;
	}
	_QERPlugImageTable* getTable() {
		return &m_imagetga;
	}
};

typedef SingletonModule<ImageTGAAPI> ImageTGAModule;

ImageTGAModule g_ImageTGAModule;


class ImageJPGAPI {
	_QERPlugImageTable m_imagejpg;
public:
	typedef _QERPlugImageTable Type;
	STRING_CONSTANT(Name, "jpg");

	ImageJPGAPI() {
		m_imagejpg.loadImage = LoadJPG;
	}
	_QERPlugImageTable* getTable() {
		return &m_imagejpg;
	}
};

typedef SingletonModule<ImageJPGAPI, ImageDependencies> ImageJPGModule;

ImageJPGModule g_ImageJPGModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules(ModuleServer& server) {
	initialiseModule(server);

	g_ImageTGAModule.selfRegister();
	g_ImageJPGModule.selfRegister();
}
