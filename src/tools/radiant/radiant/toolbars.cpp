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
#include "camera/camwindow.h"
#include "ui/Icons.h"
#include "pathfinding.h"

static void File_constructToolbar (GtkToolbar* toolbar, MainFrame *mainframe)
{
	toolbar_append_button(toolbar, _("Open an existing map (CTRL + O)"), ui::icons::ICON_OPEN, "OpenMap");
	mainframe->SetSaveButton(toolbar_append_button(toolbar, _("Save the active map (CTRL + S)"), ui::icons::ICON_SAVE,
			"SaveMap"));
}

static void UndoRedo_constructToolbar (GtkToolbar* toolbar, MainFrame *mainframe)
{
	mainframe->SetUndoButton(toolbar_append_button(toolbar, _("Undo (CTRL + Z)"), ui::icons::ICON_UNDO, "Undo"));
	mainframe->SetRedoButton(toolbar_append_button(toolbar, _("Redo (CTRL + Y)"), ui::icons::ICON_REDO, "Redo"));
}

static void RotateFlip_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("x-axis Flip"), ui::icons::ICON_BRUSH_FLIPX, "MirrorSelectionX");
	toolbar_append_button(toolbar, _("x-axis Rotate"), ui::icons::ICON_BRUSH_ROTATEX, "RotateSelectionX");
	toolbar_append_button(toolbar, _("y-axis Flip"), ui::icons::ICON_BRUSH_FLIPY, "MirrorSelectionY");
	toolbar_append_button(toolbar, _("y-axis Rotate"), ui::icons::ICON_BRUSH_ROTATEY, "RotateSelectionY");
	toolbar_append_button(toolbar, _("z-axis Flip"), ui::icons::ICON_BRUSH_FLIPZ, "MirrorSelectionZ");
	toolbar_append_button(toolbar, _("z-axis Rotate"), ui::icons::ICON_BRUSH_ROTATEZ, "RotateSelectionZ");
}

static void Select_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("Select touching"), ui::icons::ICON_SELECT_TOUCHING, "SelectTouching");
	toolbar_append_button(toolbar, _("Select inside"), ui::icons::ICON_SELECT_INSIDE, "SelectInside");
	toolbar_append_button(toolbar, _("Select whole entity"), ui::icons::ICON_SELECT_ENTITIES,
			"ExpandSelectionToEntities");
	toolbar_append_button(toolbar, _("Select all of type"), ui::icons::ICON_SELECT_COMPLETE, "SelectAllOfType");
	toolbar_append_button(toolbar, _("Select all Faces of same texture"), ui::icons::ICON_SELECT_SAME_TEXTURE,
			"SelectAllFacesOfTex");
}

static void CSG_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("CSG Subtract"), ui::icons::ICON_CSG_SUBTRACT, "CSGSubtract");
	toolbar_append_button(toolbar, _("CSG Merge"), ui::icons::ICON_CSG_MERGE, "CSGMerge");
	toolbar_append_button(toolbar, _("Make Hollow"), ui::icons::ICON_CSG_MAKE_HOLLOW, "CSGHollow");
}

static void ComponentModes_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Select _Vertices"), ui::icons::ICON_MODIFY_VERTICES, "DragVertices");
	toolbar_append_toggle_button(toolbar, _("Select _Edges"), ui::icons::ICON_MODIFY_EDGES, "DragEdges");
	toolbar_append_toggle_button(toolbar, _("Select _Faces"), ui::icons::ICON_MODIFY_FACES, "DragFaces");
}

static void Clipper_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Clipper (X)"), ui::icons::ICON_VIEW_CLIPPER, "ToggleClipper");
}

static void Filters_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Filter nodraw"), ui::icons::ICON_FILTER_NODRAW, "FilterNodraw");
	toolbar_append_toggle_button(toolbar, _("Filter actorclip"), ui::icons::ICON_FILTER_ACTORCLIP, "FilterActorClips");
	toolbar_append_toggle_button(toolbar, _("Filter weaponclip"), ui::icons::ICON_FILTER_WEAPONCLIP,
			"FilterWeaponClips");
}

static void XYWnd_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("Change views"), ui::icons::ICON_VIEW_CHANGE, "NextView");
}

static void Manipulators_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Translate (W)"), ui::icons::ICON_SELECT_MOUSE_TRANSLATE, "MouseTranslate");
	toolbar_append_toggle_button(toolbar, _("Rotate (R)"), ui::icons::ICON_SELECT_MOUSE_ROTATE, "MouseRotate");
	toolbar_append_toggle_button(toolbar, _("Scale"), ui::icons::ICON_SELECT_MOUSE_SCALE, "MouseScale");
	toolbar_append_toggle_button(toolbar, _("Resize (Q)"), ui::icons::ICON_SELECT_MOUSE_RESIZE, "MouseDrag");

	Clipper_constructToolbar(toolbar);
}

static void LevelFilters_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_button(toolbar, _("Level 1"), ui::icons::ICON_LEVEL1, "FilterLevel1");
	toolbar_append_button(toolbar, _("Level 2"), ui::icons::ICON_LEVEL2, "FilterLevel2");
	toolbar_append_button(toolbar, _("Level 3"), ui::icons::ICON_LEVEL3, "FilterLevel3");
	toolbar_append_button(toolbar, _("Level 4"), ui::icons::ICON_LEVEL4, "FilterLevel4");
	toolbar_append_button(toolbar, _("Level 5"), ui::icons::ICON_LEVEL5, "FilterLevel5");
	toolbar_append_button(toolbar, _("Level 6"), ui::icons::ICON_LEVEL6, "FilterLevel6");
	toolbar_append_button(toolbar, _("Level 7"), ui::icons::ICON_LEVEL7, "FilterLevel7");
	toolbar_append_button(toolbar, _("Level 8"), ui::icons::ICON_LEVEL8, "FilterLevel8");
}

GtkToolbar* create_main_toolbar_horizontal (MainFrame *mainframe)
{
	GtkToolbar* toolbar = GTK_TOOLBAR(gtk_toolbar_new());
	gtk_toolbar_set_show_arrow(toolbar, TRUE);
	gtk_toolbar_set_orientation(toolbar, GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);

	gtk_widget_show(GTK_WIDGET(toolbar));

	File_constructToolbar(toolbar, mainframe);

	gtk_toolbar_append_space(toolbar);

	UndoRedo_constructToolbar(toolbar, mainframe);

	gtk_toolbar_append_space(toolbar);

	RotateFlip_constructToolbar(toolbar);

	gtk_toolbar_append_space(toolbar);

	Select_constructToolbar(toolbar);

	gtk_toolbar_append_space(toolbar);

	ComponentModes_constructToolbar(toolbar);

	if (mainframe->CurrentStyle() == MainFrame::eRegular) {
		gtk_toolbar_append_space(toolbar);

		XYWnd_constructToolbar(toolbar);
	}

	gtk_toolbar_append_space(toolbar);

	Filters_constructToolbar(toolbar);

	gtk_toolbar_append_space(toolbar);
	toolbar_append_button(toolbar, _("Refresh Models"), ui::icons::ICON_REFRESH_MODELS, "RefreshReferences");
	toolbar_append_button(toolbar, _("Background image"), ui::icons::ICON_BACKGROUND, "ToggleBackground");

	gtk_toolbar_append_space(toolbar);
	toolbar_append_button(toolbar, _("Create material entries for selected faces (M)"),
			ui::icons::ICON_MATERIALS_GENERATE, "GenerateMaterialFromTexture");

	gtk_toolbar_append_space(toolbar);
	Pathfinding_constructToolbar(toolbar);

	gtk_toolbar_append_space(toolbar);
	LevelFilters_constructToolbar(toolbar);

	gtk_toolbar_append_space(toolbar);
	toolbar_append_toggle_button(toolbar, _("Show model bounding box"), ui::icons::ICON_DRAW_BBOX, "ToggleShowModelBoundingBox");
	toolbar_append_toggle_button(toolbar, _("Show model normals"), ui::icons::ICON_MODEL_SHOWNORMALS, "ToggleShowModelNormals");

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

	gtk_toolbar_append_space(toolbar);

	Manipulators_constructToolbar(toolbar);

	gtk_toolbar_append_space(toolbar);

	CSG_constructToolbar(toolbar);

	gtk_toolbar_append_space(toolbar);

	toolbar_append_toggle_button(toolbar, _("Texture Lock (SHIFT + T)"), ui::icons::ICON_TEXTURE_LOCK, "TogTexLock");

	return toolbar;
}
