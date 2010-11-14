#ifndef PARTICLEPROPERTYEDITOR_H_
#define PARTICLEPROPERTYEDITOR_H_

#include "PropertyEditor.h"

namespace ui {

/**
 * Property editor for selecting a particle.
 */
class ParticlePropertyEditor: public PropertyEditor
{
		// Entity to edit
		Entity* _entity;

		// Keyvalue to set
		std::string _key;

	private:

		/* GTK CALLBACKS */
		static void _onBrowseButton (GtkWidget*, ParticlePropertyEditor*);

	public:

		// Default constructor for the map
		ParticlePropertyEditor ()
		{
		}

		// Main constructor
		ParticlePropertyEditor (Entity* entity, const std::string& name, const std::string& options);

		// Clone method for virtual construction
		PropertyEditorPtr createNew (Entity* entity, const std::string& name, const std::string& options)
		{
			return PropertyEditorPtr(new ParticlePropertyEditor(entity, name, options));
		}
};

}

#endif /*PARTICLEPROPERTYEDITOR_H_*/
