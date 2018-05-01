#pragma once

#include <map>
#include <string>
#include "iregistry.h"

/* greebo: This class establishes connections between values stored in GObjects and
 * registry key/values. It provides methods for connecting the objects to certain keys,
 * and loading/saving the values from/to the registry.
 *
 * The references to the GtkWidgets/GObjects are stored as GObject* pointers, that
 * are cast onto the supported types (e.g. GtkToggleButton, GtkAdjustment, etc.).
 *
 * The stored objects have to be supported by this class, otherwise an error is thrown to std::cout
 *
 * Note to avoid any confusion:
 *
 * "import" is to be understood as importing values FROM the XMLRegistry and storing them TO the widgets.
 * "export" is naturally the opposite: values are exported TO the XMLRegistry (with values FROM the widgets).
 * */

// Forward declarations to avoid including the whole GTK headers
typedef struct _GObject GObject;

namespace gtkutil {

namespace {
typedef std::map<GObject*, std::string> ObjectKeyMap;
}

class RegistryConnector
{
		// The association of widgets and registry keys
		ObjectKeyMap _objectKeyMap;

	public:
		// Connect a GObject to the specified XMLRegistry key
		void connectGObject (GObject* object, const std::string& registryKey);

		// Loads all the values from the registry into the connected GObjects
		void importValues ();

		// Reads the values from the GObjects and writes them into the XMLRegistry
		void exportValues ();

	private:
		// Load the value from the registry and store it into the GObject
		void importKey (GObject* obj, const std::string& registryKey);

		// Retrieve the value from the GObject and save it into the registry
		void exportKey (GObject* obj, const std::string& registryKey);

}; // class RegistryConnector

} // namespace xml
