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
#include "gtkutil/dialog.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "ifilesystem.h"
#include "ui/common/MaterialDefinitionView.h"
#include "gtkutil/multimonitor.h"
#include "autoptr.h"
#include "iarchive.h"

void ShowMaterialDefinition (const std::string& append)
{
	const std::string& materialName = Material_GetFilename();

	// Construct a shader view and pass the shader name
	ui::MaterialDefinitionView view;
	view.setMaterial(materialName);
	view.append(append);

	GtkWidget* dialog = gtk_dialog_new_with_buttons(_("Material Definition"), GlobalRadiant().getMainWindow(),
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), view.getWidget());

	GdkRectangle rect = gtkutil::MultiMonitor::getMonitorForWindow(GlobalRadiant().getMainWindow());
	gtk_window_set_default_size(GTK_WINDOW(dialog), gint(rect.width / 2), gint(2 * rect.height / 3));

	gtk_widget_show_all(dialog);

	// Show and block
	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
}

void GenerateMaterialFromTexture (void)
{
	const std::string& mapname = Map_Name(g_map);
	if (mapname.empty() || Map_Unnamed(g_map)) {
		// save the map first
		gtkutil::errorDialog(MainFrame_getWindow(), _("You have to save your map before material generation can work"));
		return;
	}

	std::string content;
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(Material_GetFilename()));
	if (file)
		content = file->getString();

	std::string append;
	if (!g_SelectedFaceInstances.empty()) {
		for (FaceInstancesList::iterator i = g_SelectedFaceInstances.m_faceInstances.begin(); i
				!= g_SelectedFaceInstances.m_faceInstances.end(); ++i) {
			const FaceInstance& faceInstance = *(*i);
			const Face &face = faceInstance.getFace();
			std::string texture = face.getShader().getShader();
			texture = texture.rfind("textures/");
			std::string materialDefinition = "material " + texture;
			/* check whether there is already an entry for the selected texture */
			if (content.find(materialDefinition) == std::string::npos)
				append += "{\n\tmaterial " + texture + "\n\t{\n\t}\n}\n";
		}
	}

	ShowMaterialDefinition(append);
}

const std::string Material_GetFilename (void)
{
	const std::string& mapname = Map_Name(g_map);
	std::string materialFileName = "materials/" + os::getFilenameFromPath(mapname);
	materialFileName[materialFileName.length() - 1] = 't'; /* map => mat */
	return materialFileName;
}

void Material_Construct (void)
{
	GlobalCommands_insert("GenerateMaterialFromTexture", FreeCaller<GenerateMaterialFromTexture> (), Accelerator('M'));
	command_connect_accelerator("GenerateMaterialFromTexture");
}

void Material_Destroy (void)
{
}
