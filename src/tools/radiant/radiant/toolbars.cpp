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

#include "radiant_i18n.h"
#include "toolbars.h"
#include "gtkmisc.h"
#include "camwindow.h"

static void File_constructToolbar (GtkToolbar* toolbar, MainFrame *mainframe)
{
	toolbar_append_button(toolbar, _("Open an existing map (CTRL + O)"), "file_open.bmp", "OpenMap");
	mainframe->SetSaveButton(toolbar_append_button(toolbar, _("Save the active map (CTRL + S)"), "file_save.bmp",
			"SaveMap"));
}

static void UndoRedo_constructToolbar (GtkToolbar* toolbar, MainFrame *mainframe)
{
	mainframe->SetUndoButton(toolbar_append_button(toolbar, _("Undo (CTRL + Z)"), "undo.bmp", "Undo"));
	mainframe->SetRedoButton(toolbar_append_button(toolbar, _("Redo (CTRL + Y)"), "redo.bmp", "Redo"));
}

static void RotateFlip_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("x-axis Flip"), "brush_flipx.bmp", "MirrorSelectionX");
	toolbar_append_button(toolbar, _("x-axis Rotate"), "brush_rotatex.bmp", "RotateSelectionX");
	toolbar_append_button(toolbar, _("y-axis Flip"), "brush_flipy.bmp", "MirrorSelectionY");
	toolbar_append_button(toolbar, _("y-axis Rotate"), "brush_rotatey.bmp", "RotateSelectionY");
	toolbar_append_button(toolbar, _("z-axis Flip"), "brush_flipz.bmp", "MirrorSelectionZ");
	toolbar_append_button(toolbar, _("z-axis Rotate"), "brush_rotatez.bmp", "RotateSelectionZ");
}

static void Select_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("Select touching"), "selection_selecttouching.bmp", "SelectTouching");
	toolbar_append_button(toolbar, _("Select inside"), "selection_selectinside.bmp", "SelectInside");
	toolbar_append_button(toolbar, _("Select whole entity"), "selection_selectentities.bmp",
			"ExpandSelectionToEntities");
	toolbar_append_button(toolbar, _("Select all of type"), "selection_selectcompletetall.bmp", "SelectAllOfType");
	toolbar_append_button(toolbar, _("Select all Faces of same texture"), "selection_selectallsametex.bmp",
			"SelectAllFacesOfTex");
}

static void CSG_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("CSG Subtract"), "selection_csgsubtract.bmp", "CSGSubtract");
	toolbar_append_button(toolbar, _("CSG Merge"), "selection_csgmerge.bmp", "CSGMerge");
	toolbar_append_button(toolbar, _("Make Hollow"), "selection_makehollow.bmp", "CSGHollow");
}

static void ComponentModes_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Select _Vertices"), "modify_vertices.bmp", "DragVertices");
	toolbar_append_toggle_button(toolbar, _("Select _Edges"), "modify_edges.bmp", "DragEdges");
	toolbar_append_toggle_button(toolbar, _("Select _Faces"), "modify_faces.bmp", "DragFaces");
}

static void Clipper_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Clipper (X)"), "view_clipper.bmp", "ToggleClipper");
}

static void Filters_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Filter nodraw"), "filter_nodraw.bmp", "FilterNodraw");
	toolbar_append_toggle_button(toolbar, _("Filter actorclip"), "filter_actorclip.bmp", "FilterActorClips");
	toolbar_append_toggle_button(toolbar, _("Filter weaponclip"), "filter_weaponclip.bmp", "FilterWeaponClips");
}

static void XYWnd_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("Change views"), "view_change.bmp", "NextView");
}

static void Manipulators_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Translate (W)"), "select_mousetranslate.bmp", "MouseTranslate");
	toolbar_append_toggle_button(toolbar, _("Rotate (R)"), "select_mouserotate.bmp", "MouseRotate");
	toolbar_append_toggle_button(toolbar, _("Scale"), "select_mousescale.bmp", "MouseScale");
	toolbar_append_toggle_button(toolbar, _("Resize (Q)"), "select_mouseresize.bmp", "MouseDrag");

	Clipper_constructToolbar(toolbar);
}

static void LevelFilters_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("Level 1"), "ufoai_level1.bmp", "FilterLevel1");
	toolbar_append_button(toolbar, _("Level 2"), "ufoai_level2.bmp", "FilterLevel2");
	toolbar_append_button(toolbar, _("Level 3"), "ufoai_level3.bmp", "FilterLevel3");
	toolbar_append_button(toolbar, _("Level 4"), "ufoai_level4.bmp", "FilterLevel4");
	toolbar_append_button(toolbar, _("Level 5"), "ufoai_level5.bmp", "FilterLevel5");
	toolbar_append_button(toolbar, _("Level 6"), "ufoai_level6.bmp", "FilterLevel6");
	toolbar_append_button(toolbar, _("Level 7"), "ufoai_level7.bmp", "FilterLevel7");
	toolbar_append_button(toolbar, _("Level 8"), "ufoai_level8.bmp", "FilterLevel8");
}

GtkToolbar* create_main_toolbar_horizontal (MainFrame *mainframe)
{
	GtkToolbar* toolbar = GTK_TOOLBAR(gtk_toolbar_new());
	gtk_toolbar_set_show_arrow(toolbar, TRUE);
	gtk_toolbar_set_orientation(toolbar, GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);

	gtk_widget_show(GTK_WIDGET(toolbar));

	File_constructToolbar(toolbar, mainframe);

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));

	UndoRedo_constructToolbar(toolbar, mainframe);

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));

	RotateFlip_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	Select_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	ComponentModes_constructToolbar(toolbar);

	if (mainframe->CurrentStyle() == MainFrame::eRegular) {
		gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

		XYWnd_constructToolbar(toolbar);
	}

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	Filters_constructToolbar(toolbar);

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));
	toolbar_append_button(toolbar, _("Refresh Models"), "refresh_models.bmp", "RefreshReferences");
	toolbar_append_button(toolbar, _("Background image"), "background.bmp", "ToggleBackground");

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));
	toolbar_append_button(toolbar, _("Create material entries for selected faces (M)"), "material_generate.bmp",
			"GenerateMaterialFromTexture");

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));
	toolbar_append_toggle_button(toolbar, _("Show pathfinding info"), "pathfinding.bmp", "ShowPathfinding");

	gtk_toolbar_append_space(GTK_TOOLBAR (toolbar));
	LevelFilters_constructToolbar(toolbar);

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

	toolbar_append_toggle_button(toolbar, _("Texture Lock (SHIFT + T)"), "texture_lock.bmp", "TogTexLock");

	return toolbar;
}

