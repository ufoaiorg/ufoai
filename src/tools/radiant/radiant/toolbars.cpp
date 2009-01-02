/**
 * @file toolbars.cpp
 * @brief Toolbars code for horizontal and vertical bars
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

#include "toolbars.h"
#include "gtkmisc.h"
#include "camwindow.h"

static void File_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, "Open an existing map (CTRL + O)", "file_open.bmp", "OpenMap");
	toolbar_append_button(toolbar, "Save the active map (CTRL + S)", "file_save.bmp", "SaveMap");
}

static void UndoRedo_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, "Undo (CTRL + Z)", "undo.bmp", "Undo");
	toolbar_append_button(toolbar, "Redo (CTRL + Y)", "redo.bmp", "Redo");
}

static void RotateFlip_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, "x-axis Flip", "brush_flipx.bmp", "MirrorSelectionX");
	toolbar_append_button(toolbar, "x-axis Rotate", "brush_rotatex.bmp", "RotateSelectionX");
	toolbar_append_button(toolbar, "y-axis Flip", "brush_flipy.bmp", "MirrorSelectionY");
	toolbar_append_button(toolbar, "y-axis Rotate", "brush_rotatey.bmp", "RotateSelectionY");
	toolbar_append_button(toolbar, "z-axis Flip", "brush_flipz.bmp", "MirrorSelectionZ");
	toolbar_append_button(toolbar, "z-axis Rotate", "brush_rotatez.bmp", "RotateSelectionZ");
}

static void Select_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, "Select touching", "selection_selecttouching.bmp", "SelectTouching");
	toolbar_append_button(toolbar, "Select inside", "selection_selectinside.bmp", "SelectInside");
	toolbar_append_button(toolbar, "Select whole entity", "selection_selectentities.bmp", "ExpandSelectionToEntities");
	toolbar_append_button(toolbar, "Select all of type", "selection_selectcompletetall.bmp", "SelectAllOfType");
}

static void CSG_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, "CSG Subtract (SHIFT + U)", "selection_csgsubtract.bmp", "CSGSubtract");
	toolbar_append_button(toolbar, "CSG Merge (CTRL + U)", "selection_csgmerge.bmp", "CSGMerge");
	toolbar_append_button(toolbar, "Hollow", "selection_makehollow.bmp", "CSGHollow");
}

static void ComponentModes_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, "Select Vertices (V)", "modify_vertices.bmp", "DragVertices");
	toolbar_append_toggle_button(toolbar, "Select Edges (E)", "modify_edges.bmp", "DragEdges");
	toolbar_append_toggle_button(toolbar, "Select Faces (F)", "modify_faces.bmp", "DragFaces");
}

static void Clipper_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, "Clipper (X)", "view_clipper.bmp", "ToggleClipper");
}

static void Filters_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, "Filter nodraw", "filter_nodraw.bmp", "FilterNodraw");
	toolbar_append_toggle_button(toolbar, "Filter actorclip", "filter_actorclip.bmp", "FilterActorClips");
	toolbar_append_toggle_button(toolbar, "Filter weaponclip", "filter_weaponclip.bmp", "FilterWeaponClips");
}

static void XYWnd_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, "Change views", "view_change.bmp", "NextView");
}

static void Manipulators_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, "Translate (W)", "select_mousetranslate.bmp", "MouseTranslate");
	toolbar_append_toggle_button(toolbar, "Rotate (R)", "select_mouserotate.bmp", "MouseRotate");
	toolbar_append_toggle_button(toolbar, "Scale", "select_mousescale.bmp", "MouseScale");
	toolbar_append_toggle_button(toolbar, "Resize (Q)", "select_mouseresize.bmp", "MouseDrag");

	Clipper_constructToolbar(toolbar);
}

GtkToolbar* create_main_toolbar_horizontal (MainFrame::EViewStyle style)
{
	GtkToolbar* toolbar = GTK_TOOLBAR(gtk_toolbar_new());
	gtk_toolbar_set_show_arrow(toolbar, TRUE);
	gtk_toolbar_set_orientation(toolbar, GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);

	gtk_widget_show(GTK_WIDGET(toolbar));

	File_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));

	UndoRedo_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));

	RotateFlip_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	Select_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	ComponentModes_constructToolbar(toolbar);

	if (style == MainFrame::eRegular) {
		gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

		XYWnd_constructToolbar(toolbar);
	}

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	Filters_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));
	toolbar_append_button(toolbar, "Refresh Models", "refresh_models.bmp", "RefreshReferences");
	toolbar_append_button(toolbar, "Background image", "background.bmp", "ToggleBackground");

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));
	toolbar_append_button(toolbar, "Create material entries for selected faces (M)", "material_generate.bmp", "GenerateMaterialFromTexture");

	return toolbar;
}

GtkToolbar* create_main_toolbar_vertical (MainFrame::EViewStyle style)
{
	GtkToolbar* toolbar = GTK_TOOLBAR(gtk_toolbar_new());
	gtk_toolbar_set_show_arrow(toolbar, TRUE);
	gtk_toolbar_set_orientation(toolbar, GTK_ORIENTATION_VERTICAL);
	gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);

	gtk_widget_show(GTK_WIDGET(toolbar));

	CamWnd_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	Manipulators_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));

	CSG_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	toolbar_append_toggle_button(toolbar, "Texture Lock (SHIFT +T)", "texture_lock.bmp", "TogTexLock");

	if (style != MainFrame::eRegular) {
		toolbar_append_button(toolbar, "Texture Browser (T)", "texture_browser.bmp", "ToggleTextures");
	}

	return toolbar;
}

