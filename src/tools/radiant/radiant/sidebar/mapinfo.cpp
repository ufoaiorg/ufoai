/**
 * @file mapinfo.cpp
 * @brief Map Information sidebar widget
 */

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

#include "mapinfo.h"
#include "../radiant.h"

#include <map>

#include "../qe3.h"
#include "iselection.h"
#include "signal/isignal.h"
#include "eclasslib.h"
#include "ientity.h"
#include "iscenegraph.h"
#include "gtkutil/dialog.h"
#include "gtkutil/window.h"

typedef std::map<CopiedString, std::size_t> EntityBreakdown;

class EntityBreakdownWalker : public scene::Graph::Walker {
	EntityBreakdown& m_entitymap;
public:
	EntityBreakdownWalker(EntityBreakdown& entitymap)
			: m_entitymap(entitymap) {
	}
	bool pre(const scene::Path& path, scene::Instance& instance) const {
		Entity* entity = Node_getEntity(path.top());
		if (entity != 0) {
			const EntityClass& eclass = entity->getEntityClass();
			if (m_entitymap.find(eclass.name()) == m_entitymap.end())
				m_entitymap[eclass.name()] = 1;
			else
				++m_entitymap[eclass.name()];
		}
		return true;
	}
};

static void Scene_EntityBreakdown (EntityBreakdown& entitymap)
{
	GlobalSceneGraph().traverse(EntityBreakdownWalker(entitymap));
}

static GtkEntry* brushes_entry;
static GtkEntry* entities_entry;
static GtkListStore* entityList;

void MapInfo_Update (void)
{
	// Initialize fields
	EntityBreakdown entitymap;
	Scene_EntityBreakdown(entitymap);

	gtk_list_store_clear(GTK_LIST_STORE(entityList));

	for (EntityBreakdown::iterator i = entitymap.begin(); i != entitymap.end(); ++i) {
		char tmp[16];
		sprintf (tmp, "%u", Unsigned((*i).second));
		GtkTreeIter iter;
		gtk_list_store_append(GTK_LIST_STORE(entityList), &iter);
		gtk_list_store_set(GTK_LIST_STORE(entityList), &iter, 0, (*i).first.c_str(), 1, tmp, -1);
	}

	char tmp[16];
	sprintf(tmp, "%u", Unsigned(g_brushCount.get()));
	gtk_entry_set_text(GTK_ENTRY (brushes_entry), tmp);
	sprintf(tmp, "%u", Unsigned(g_entityCount.get()));
	gtk_entry_set_text(GTK_ENTRY (entities_entry), tmp);
}

/** @todo Click action should be to select all the ents of the given type */
GtkWidget *MapInfo_constructNotebookTab (void)
{
	GtkVBox* vbox = create_dialog_vbox(4, 4);

	{
		GtkHBox* hbox = create_dialog_hbox(4);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

		{
			GtkTable* table = create_dialog_table(2, 2, 4, 4);
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), TRUE, TRUE, 0);

			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(table, GTK_WIDGET(entry), 1, 2, 0, 1,
								(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
								(GtkAttachOptions) (0), 0, 0);
				gtk_entry_set_editable(entry, FALSE);

				brushes_entry = entry;
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(table, GTK_WIDGET(entry), 1, 2, 1, 2,
								(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
								(GtkAttachOptions) (0), 0, 0);
				gtk_entry_set_editable(entry, FALSE);

				entities_entry = entry;
			}
			{
				GtkWidget* label = gtk_label_new(_("Total Brushes"));
				gtk_table_attach(GTK_TABLE (table), label, 0, 1, 0, 1,
								(GtkAttachOptions) (GTK_FILL),
								(GtkAttachOptions) (0), 0, 0);
				gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
			}
			{
				GtkWidget* label = gtk_label_new(_("Total Entities"));
				gtk_table_attach(GTK_TABLE (table), label, 0, 1, 1, 2,
								(GtkAttachOptions) (GTK_FILL),
								(GtkAttachOptions) (0), 0, 0);
				gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
			}
		}
	}
	{
		GtkWidget* label = gtk_label_new(_("Entity breakdown"));
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(label), FALSE, TRUE, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	}
	{
		GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC, 4);
		gtk_box_pack_start(GTK_BOX (vbox), GTK_WIDGET(scr), TRUE, TRUE, 0);

		{
			GtkListStore* store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

			GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
			gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), TRUE);

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Entity"), renderer, "text", 0, (char const*)0);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
				gtk_tree_view_column_set_sort_column_id(column, 0);
			}

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Count"), renderer, "text", 1, (char const*)0);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
				gtk_tree_view_column_set_sort_column_id(column, 1);
			}

			gtk_container_add(GTK_CONTAINER(scr), view);

			entityList = store;
		}
	}

	MapInfo_Update();

	return GTK_WIDGET(vbox);
}

void MapInfo_SelectionChanged (const Selectable& selectable)
{
	MapInfo_Update();
}

/**
 * @sa MapInfo_Destroy
 */
void MapInfo_Construct (void)
{
	typedef FreeCaller1<const Selectable&, MapInfo_SelectionChanged> MapInfoSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(MapInfoSelectionChangedCaller());
}

/**
 * @sa MapInfo_Construct
 */
void MapInfo_Destroy (void)
{
}
