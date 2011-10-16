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

#include "MapInfo.h"
#include "radiant_i18n.h"

#include "eclasslib.h"
#include "scenelib.h"
#include "gtkutil/dialog.h"
#include "gtkutil/window.h"
#include "gtkutil/TreeModel.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/IconTextMenuItem.h"
#include "../ui/Icons.h"

namespace sidebar
{
	enum
	{
		ENTITY_NAME, ENTITY_COUNT,

		MAPINFO_COLUMNS
	};

	namespace
	{
		typedef std::map<std::string, int> EntityBreakdown;

		class EntityBreakdownWalker: public scene::Graph::Walker
		{
				EntityBreakdown& m_entitymap;
			public:
				EntityBreakdownWalker (EntityBreakdown& entitymap) :
					m_entitymap(entitymap)
				{
				}
				bool pre (const scene::Path& path, scene::Instance& instance) const
				{
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
	}

	/** @todo Click action should be to select all the ents of the given type */
	MapInfo::MapInfo () :
		_widget(gtk_vbox_new(FALSE, 3)), _store(gtk_list_store_new(MAPINFO_COLUMNS, G_TYPE_STRING, G_TYPE_INT)), _view(
				gtk_tree_view_new_with_model(GTK_TREE_MODEL(_store))), _infoStore(gtk_list_store_new(2, G_TYPE_STRING,
				G_TYPE_INT)), _vboxEntityBreakdown(gtk_vbox_new(FALSE, 0))
	{
		gtk_box_pack_start(GTK_BOX(_widget), createEntityBreakdownTreeView(), TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(_widget), createInfoPanel(), FALSE, FALSE, 0);

		update();
	}

	GtkWidget* MapInfo::createInfoPanel ()
	{
		// Info table. Has key and value columns.
		GtkWidget* infTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_infoStore));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(infTreeView), FALSE);

		GtkCellRenderer* rend;
		GtkTreeViewColumn* col;

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Attribute"), rend, "text", 0, NULL);
		g_object_set(G_OBJECT(rend), "weight", 700, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(infTreeView), col);

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Value"), rend, "text", 1, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(infTreeView), col);

		// Pack into scroll window and return
		return gtkutil::ScrolledFrame(infTreeView);
	}

	GtkWidget* MapInfo::createEntityBreakdownTreeView ()
	{
		GtkWidget* label = gtk_label_new(_("Entity breakdown"));
		gtk_box_pack_start(GTK_BOX(_vboxEntityBreakdown), GTK_WIDGET(label), FALSE, TRUE, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(_view), TRUE);

		GtkCellRenderer* rendererEntityName = gtk_cell_renderer_text_new();
		GtkTreeViewColumn* columnEntityName = gtk_tree_view_column_new_with_attributes(_("Entity"), rendererEntityName,
				"text", 0, (char const*) 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(_view), columnEntityName);
		gtk_tree_view_column_set_sort_column_id(columnEntityName, ENTITY_NAME);

		GtkCellRenderer* rendererEntityCount = gtk_cell_renderer_text_new();
		GtkTreeViewColumn* columnEntityCount = gtk_tree_view_column_new_with_attributes(_("Count"),
				rendererEntityCount, "text", 1, (char const*) 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(_view), columnEntityCount);
		gtk_tree_view_column_set_sort_column_id(columnEntityCount, ENTITY_COUNT);

		gtk_container_add(GTK_CONTAINER(_vboxEntityBreakdown), gtkutil::ScrolledFrame(_view));

		// Return the vertical box with the treeview included
		return _vboxEntityBreakdown;
	}

	MapInfo& MapInfo::Instance ()
	{
		static MapInfo _instance;
		return _instance;
	}

	void MapInfo::update ()
	{
		// Initialize fields
		EntityBreakdown entitymap;
		GlobalSceneGraph().traverse(EntityBreakdownWalker(entitymap));

		gtk_list_store_clear(GTK_LIST_STORE(_store));

		for (EntityBreakdown::iterator i = entitymap.begin(); i != entitymap.end(); ++i) {
			GtkTreeIter iter;
			gtk_list_store_append(GTK_LIST_STORE(_store), &iter);
			gtk_list_store_set(GTK_LIST_STORE(_store), &iter, ENTITY_NAME, i->first.c_str(), ENTITY_COUNT, i->second, -1);
		}

		// Prepare to populate the info table
		gtk_list_store_clear(_infoStore);
		GtkTreeIter iter;

		// Update the text in the info table
		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Total Brushes"), 1, GlobalRadiant().getCounter(counterBrushes).get(), -1);

		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Total Entities"), 1, GlobalRadiant().getCounter(counterEntities).get(), -1);
	}
}
