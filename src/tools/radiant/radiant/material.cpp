/**
 * @file material.cpp
 * @brief Material generation code
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#include "material.h"
#include "radiant_i18n.h"

#include "mainframe.h"
#include "gtkmisc.h"
#include "iselection.h"
#include "brush.h"
#include "commands.h"
#include "map.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/dialog.h"
#include "dialogs/texteditor.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"

/**
 * @param[in] filename Absolut path to the material file
 * @note Make sure to free the returned buffer
 * @return The material file content as string (or NULL on failure)
 */
const char *Material_LoadFile(const char* filename) {
	g_message("Open material file '%s' for read...\n", filename);
	TextFileInputStream file(filename);
	if (!file.failed()) {
		g_message("success\n");
		std::size_t size = file_size(filename);
		char *material = (char *)malloc(size);
		file.read(material, size);
		return material;
	}

	g_warning("failure\n");
	return (const char *)0;
}

void GenerateMaterialFromTexture (void)
{
	const std::string& mapname = Map_Name(g_map);
	if (mapname.empty() || Map_Unnamed(g_map)) {
		// save the map first
		gtk_MessageBox(GTK_WIDGET(MainFrame_getWindow()), _("You have to save your map before material generation can work"));
		return;
	}

	const char *materialFileName = Material_GetFilename();
	int cursorPos = 0;
	StringOutputStream append(256);
	const char *materialFileContent = Material_LoadFile(materialFileName);

	if (!g_SelectedFaceInstances.empty()) {
		for (FaceInstancesList::iterator i = g_SelectedFaceInstances.m_faceInstances.begin(); i != g_SelectedFaceInstances.m_faceInstances.end(); ++i) {
			FaceInstance& faceInstance = *(*i);
			Face &face = faceInstance.getFace();
			const char *texture = face.getShader().getShader();
			texture += strlen("textures/");
			StringOutputStream tmp(256);
			tmp << "material " << texture;
			/* check whether there is already an entry for the selected texture */
			if (!materialFileContent || !strstr(materialFileContent, tmp.c_str()))
				append << "{\n\t" << tmp.c_str() << "\n\t{\n\t}\n}\n";
			/* set cursor position to the first new entry */
			if (materialFileContent && !append.empty() && !cursorPos)
				cursorPos = strlen(materialFileContent);
		}
	}

	DoTextEditor(materialFileName, cursorPos, append.c_str());
}

const char *Material_GetFilename (void)
{
	static char materialFileName[256];
	const std::string& mapname = Map_Name(g_map);
	snprintf(materialFileName, sizeof(materialFileName), "materials/%s", path_get_filename_start(mapname.c_str()));
	materialFileName[strlen(materialFileName) - 1] = 't'; /* map => mat */
	return materialFileName;
}

void Material_Construct (void)
{
	GlobalCommands_insert("GenerateMaterialFromTexture", FreeCaller<GenerateMaterialFromTexture>(), Accelerator('M'));
	command_connect_accelerator("GenerateMaterialFromTexture");
}

void Material_Destroy (void)
{
}
