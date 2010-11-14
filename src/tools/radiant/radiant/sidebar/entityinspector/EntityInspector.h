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

#if !defined(INCLUDED_ENTITYINSPECTOR_H)
#define INCLUDED_ENTITYINSPECTOR_H

#include <gtk/gtk.h>

#include "ientity.h"
#include "ifilesystem.h"
#include "imodel.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"

#include <map>
#include <set>

#include "os/path.h"
#include "eclasslib.h"
#include "scenelib.h"
#include "generic/callback.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "moduleobserver.h"
#include "convert.h"
#include "stringio.h"
#include "../../ui/modelselector/ModelSelector.h"
#include "../../ui/common/SoundChooser.h"

#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/button.h"
#include "gtkutil/entry.h"
#include "gtkutil/container.h"
#include "gtkutil/TreeModel.h"

#include "../../qe3.h"
#include "../../gtkmisc.h"
#include "../../entity.h"
#include "../../mainframe.h"
#include "../../textureentry.h"

#include "EntityAttribute.h"
#include "AngleAttribute.h"
#include "AnglesAttribute.h"
#include "BooleanAttribute.h"
#include "DirectionAttribute.h"
#include "ListAttribute.h"
#include "ModelAttribute.h"
#include "ParticleAttribute.h"
#include "SoundAttribute.h"
#include "StringAttribute.h"
#include "Vector3Attribute.h"

typedef std::vector<EntityAttribute*> EntityAttributes;

class EntityInspector: public ModuleObserver
{
	private:
		GtkButton *_removeKeyButton;
		GtkButton *_addKeyButton;
		GtkTreeView *_keyValuesTreeView;

		GtkTreeView* _entityClassTreeView;
		GtkTextView* _entityClassComment;

		GtkCheckButton* _entitySpawnflagsCheck[MAX_FLAGS];

		GtkListStore* _entityListStore;
		GtkTreeStore* _entityPropertiesTreeStore;
		const EntityClass* _currentFlags;
		const EntityClass* _currentComment;

		// the number of active spawnflags
		int _spawnflagCount;
		// table: index, match spawnflag item to the spawnflag index (i.e. which bit)
		int _spawnTable[MAX_FLAGS];
		// we change the layout depending on how many spawn flags we need to display
		// the table is a 4x4 in which we need to put the comment box g_entityClassComment and the spawn flags..
		GtkTable* _spawnflagsTable;

		GtkVBox* _attributeBox;

		int _numNewKeys;

		std::string g_currentSelectedKey;

		std::size_t m_unrealised;

	private:

		void GlobalEntityAttributes_clear ();
		void EntityClassList_fill ();
		void EntityClassList_clear ();
		void SetComment (EntityClass* eclass);
		void SurfaceFlags_setEntityClass (EntityClass* eclass);
		void EntityClassList_selectEntityClass (EntityClass* eclass);

		void appendAttribute (const std::string& name, EntityAttribute& attribute);

		void checkAddNewKeys ();

		std::string getTooltipForKey (const std::string& key);
		void selectionChanged (const Selectable&);

		void EntityClassList_createEntity ();

		void EntityKeyValueList_fillValueComboWithClassnames (GtkCellRenderer *renderer);

		void entityKeyValueEdited (bool isValueEdited, const std::string& newValue);
		std::string SelectedEntity_getValueForKey (const std::string& key);

		// Gtk callbacks
		static void addKeyValue (GtkButton *button, EntityInspector* entityInspector);
		static void clearKeyValue (GtkButton * button, EntityInspector* entityInspector);
		static void SpawnflagCheck_toggled (GtkWidget *widget, EntityInspector* entityInspector);
		static void entityValueEdited (GtkCellRendererText *renderer, gchar *path, gchar* new_text,
				EntityInspector *entityInspector);
		static void entityKeyEdited (GtkCellRendererText *renderer, gchar *path, gchar* new_text,
				EntityInspector *entityInspector);
		static void entityKeyEditCanceled (GtkCellRendererText *renderer, EntityInspector *entityInspector);

		static void EntityKeyValueList_selection_changed (GtkTreeSelection* selection, EntityInspector* entityInspector);
		static void EntityKeyValueList_keyEditingStarted (GtkCellRenderer *renderer, GtkCellEditable *editable,
				const gchar *path, EntityInspector *entityInspector);
		static void EntityKeyValueList_valueEditingStarted (GtkCellRenderer *renderer, GtkCellEditable *editable,
				const gchar *path, EntityInspector* entityInspector);

		static gint EntityClassList_keypress (GtkWidget* widget, GdkEventKey* event, EntityInspector* entityInspector);
		static gint EntityClassList_button_press (GtkWidget *widget, GdkEventButton *event, EntityInspector* entityInspector);
		static void EntityClassList_selection_changed (GtkTreeSelection* selection, EntityInspector* entityInspector);

	public:
		EntityAttributes _entityAttributes;

		EntityInspector ();

		void realise (void);

		void unrealise (void);

		GtkWidget *constructNotebookTab ();

		void updateKeyValueList ();
		void updateSpawnflags ();
		void setEntityClass (EntityClass *eclass);
};

EntityInspector& GlobalEntityInspector();

void EntityInspector_Construct ();
void EntityInspector_Destroy ();

#endif
