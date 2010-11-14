#include "PropertyEditorFactory.h"

#include "gtkutil/image.h"

#include "iradiant.h"
#include "Vector3PropertyEditor.h"
#include "BooleanPropertyEditor.h"
#include "EntityPropertyEditor.h"
#include "ColourPropertyEditor.h"
#include "SkinPropertyEditor.h"
#include "SoundPropertyEditor.h"
#include "FloatPropertyEditor.h"
#include "ModelPropertyEditor.h"
#include "ParticlePropertyEditor.h"
#include "SpawnflagsPropertyEditor.h"

namespace ui {

// Initialisation
PropertyEditorFactory::PropertyEditorMap PropertyEditorFactory::_peMap;

// Register the classes
void PropertyEditorFactory::registerClasses ()
{
	_peMap["vector3"] = new Vector3PropertyEditor();
	_peMap["boolean"] = new BooleanPropertyEditor();
	_peMap["entity"] = new EntityPropertyEditor();
	_peMap["spawnflags"] = new SpawnflagsPropertyEditor();
	_peMap["colour"] = new ColourPropertyEditor();
	_peMap["skin"] = new SkinPropertyEditor();
	_peMap["sound"] = new SoundPropertyEditor();
	_peMap["float"] = new FloatPropertyEditor();
	_peMap["model"] = new ModelPropertyEditor();
	_peMap["particle"] = new ParticlePropertyEditor();
}

PropertyEditorFactory::~PropertyEditorFactory ()
{
	for (PropertyEditorFactory::PropertyEditorMap::iterator i = _peMap.begin(); i != _peMap.begin(); ++i) {
		delete i->second;
	}
}

// Create a PropertyEditor from the given name.
PropertyEditorPtr PropertyEditorFactory::create (const std::string& className, Entity* entity, const std::string& key,
		const std::string& options)
{
	// Register the PropertyEditors if the map is empty
	if (_peMap.empty()) {
		registerClasses();
	}

	// Search for the named property editor type
	PropertyEditorMap::iterator iter(_peMap.find(className));

	// If the type is not found, return NULL otherwise create a new instance of
	// the associated derived type.
	if (iter == _peMap.end()) {
		return NULL;
	} else {
		return iter->second->createNew(entity, key, options);
	}
}

// Return a GdkPixbuf containing the icon for the given property type

GdkPixbuf* PropertyEditorFactory::getPixbufFor (const std::string& type)
{
	std::string iconName = "icon_" + type + ".png";
	return gtkutil::getLocalPixbuf(iconName);
}

}
