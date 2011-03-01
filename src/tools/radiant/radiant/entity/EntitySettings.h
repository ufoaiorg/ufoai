#ifndef _ENTITY_SETTINGS_H_
#define _ENTITY_SETTINGS_H_

#include "iregistry.h"
#include "preferencesystem.h"

namespace entity {

namespace {
const std::string RKEY_SHOW_ALL_LIGHT_RADII = "user/ui/showAllLightRadii";
const std::string RKEY_SHOW_SELECTED_LIGHT_RADII = "user/ui/showSelectedLightRadii";
const std::string RKEY_SHOW_ENTITY_ANGLES = "user/ui/xyview/showEntityAngles";
}

/**
 * greebo: A class managing the various settings for entities. It observes
 * the corresponding keyvalues in the registry and updates the internal
 * variables accordingly. This can be used as some sort of "cache"
 * to avoid slow registry queries during rendering, for instance.
 */
class EntitySettings: public RegistryKeyObserver, public PreferenceConstructor
{
		// TRUE if light radii should be drawn even when not selected
		bool _showAllLightRadii;

		// TRUE if light radii should be drawn when selected
		bool _showSelectedLightRadii;

		// true if GenericEntities should render their direction arrows
		bool _showEntityAngles;

		// Private constructor
		EntitySettings ();

		void constructPreferencePage(PreferenceGroup& group);

	public:
		~EntitySettings ();

		// RegistryKeyObserver implementation
		void keyChanged (const std::string& key, const std::string& value);

		bool showAllLightRadii ()
		{
			return _showAllLightRadii;
		}

		bool showSelectedLightRadii ()
		{
			return _showSelectedLightRadii;
		}

		bool showEntityAngles ()
		{
			return _showEntityAngles;
		}

		// Container for the singleton (ptr)
		static EntitySettings& Instance ();
};

} // namespace entity

#endif /* _ENTITY_SETTINGS_H_ */
