#ifndef SPAWNFLAGSPROPERTYEDITOR_H_
#define SPAWNFLAGSPROPERTYEDITOR_H_

#include "PropertyEditor.h"

#include <gtk/gtk.h>
#include <map>

namespace ui {

/**
 * Property editor for selecting the spawnflags.
 */
class SpawnflagsPropertyEditor: public PropertyEditor
{
		// Entity to edit
		Entity* _entity;

		// Keyvalue to set
		std::string _key;

		typedef std::map<int, GtkWidget*> SpawnflagWidgets;
		SpawnflagWidgets _widgets;

	private:

		/* GTK CALLBACKS */
		static void _onToggle (GtkWidget*, SpawnflagsPropertyEditor*);

	public:

		// Default constructor for the map
		SpawnflagsPropertyEditor ()
		{
		}

		// Main constructor
		SpawnflagsPropertyEditor (Entity* entity, const std::string& name, const std::string& options);

		// Clone method for virtual construction
		PropertyEditorPtr createNew (Entity* entity, const std::string& name, const std::string& options)
		{
			return PropertyEditorPtr(new SpawnflagsPropertyEditor(entity, name, options));
		}
};

}

#endif /*SPAWNFLAGSPROPERTYEDITOR_H_*/
