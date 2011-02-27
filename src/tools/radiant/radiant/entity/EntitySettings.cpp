#include "EntitySettings.h"

#include "../ui/mainframe/mainframe.h" // UpdateAllWindows
#include "radiant_i18n.h"

namespace entity {

EntitySettings::EntitySettings () :
	_showAllLightRadii(GlobalRegistry().getBool(RKEY_SHOW_ALL_LIGHT_RADII)), _showSelectedLightRadii(
			GlobalRegistry().getBool(RKEY_SHOW_SELECTED_LIGHT_RADII)), _showEntityAngles(GlobalRegistry().getBool(
			RKEY_SHOW_ENTITY_ANGLES))
{
	// Register this class as keyobserver
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_SELECTED_LIGHT_RADII);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_ALL_LIGHT_RADII);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_ENTITY_ANGLES);

	GlobalPreferenceSystem().addConstructor(this);
}

EntitySettings::~EntitySettings ()
{
}

EntitySettings& EntitySettings::Instance ()
{
	static EntitySettings _entitySettings;
	return _entitySettings;
}

// RegistryKeyObserver implementation
void EntitySettings::keyChanged (const std::string& key, const std::string& value)
{
	// Update the internal value
	if (key == RKEY_SHOW_ALL_LIGHT_RADII) {
		_showAllLightRadii = (value == "1");
	} else if (key == RKEY_SHOW_SELECTED_LIGHT_RADII) {
		_showSelectedLightRadii = (value == "1");
	} else if (key == RKEY_SHOW_ENTITY_ANGLES) {
		_showEntityAngles = (value == "1");
	}

	// Redraw the scene
	UpdateAllWindows();
}

void EntitySettings::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage* page(group.createPage(_("Entities"), _("Entity Settings")));

	page->appendCheckBox("", _("Show light radius"), RKEY_SHOW_SELECTED_LIGHT_RADII);
	page->appendCheckBox("", _("Force light radius"), RKEY_SHOW_ALL_LIGHT_RADII);
}

} // namespace entity
