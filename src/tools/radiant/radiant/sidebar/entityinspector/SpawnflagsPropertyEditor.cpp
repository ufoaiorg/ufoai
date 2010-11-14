#include "SpawnflagsPropertyEditor.h"
#include "PropertyEditorFactory.h"

#include "ientity.h"
#include "ieclass.h"
#include "eclasslib.h"

#include "radiant_i18n.h"

#include <gtk/gtk.h>

namespace ui {

// Main constructor
SpawnflagsPropertyEditor::SpawnflagsPropertyEditor (Entity* entity, const std::string& name, const std::string& options) :
	_entity(entity), _key(name)
{
	_widget = gtk_vbox_new(FALSE, 0);

	// Spawnflags (4 colums wide max, or window gets too wide.)
	GtkWidget* table = gtk_table_new(4, 4, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);

	const EntityClass& entClass = entity->getEntityClass();
	const int spawnFlags = string::toInt(entity->getKeyValue(_key));

	for (int i = 0; i < MAX_FLAGS; i++) {
		const std::string name = entClass.flagnames[i];
		if (name.empty())
			continue;

		GtkWidget* check = gtk_check_button_new_with_label(name.c_str());

		const int left = i % 4;
		const int right = left + 1;
		const int top = i / 4;
		const int bottom = top + 1;

		gtk_table_attach(GTK_TABLE(table), check, left, right, top, bottom, GTK_FILL, GTK_FILL, 0, 0);
		g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(_onToggle), this);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), (spawnFlags & (1 << i)));

		_widgets[i] = check;
	}

	gtk_box_pack_start(GTK_BOX(_widget), table, TRUE, TRUE, 0);
}

/* GTK CALLBACKS */

void SpawnflagsPropertyEditor::_onToggle (GtkWidget* w, SpawnflagsPropertyEditor* self)
{
	for (SpawnflagWidgets::const_iterator i = self->_widgets.begin(); i != self->_widgets.end(); ++i) {
		if (i->second == w) {
			const std::string oldVal = self->_entity->getKeyValue(self->_key);
			const int oldInt = string::toInt(oldVal);
			const int newVal = oldInt ^ (1 << i->first);
			self->_entity->setKeyValue(self->_key, string::toString(newVal));
			return;
		}
	}
}

} // namespace ui
