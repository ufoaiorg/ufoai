#include "MenuManager.h"

#include "iregistry.h"

#include <gtk/gtkwidget.h>
#include <iostream>

namespace ui {

	namespace {
		// The menu root key in the registry
		const std::string RKEY_MENU_ROOT = "user/ui/menu";
		const std::string TYPE_ITEM = "item";
		typedef std::vector<std::string> StringVector;
	}

// Allocate the root item (type is set automatically)
MenuManager::MenuManager() : _root(new MenuItem(NULL))
{
	globalOutputStream() << "MenuManager: Loading menu from registry.\n";
	loadFromRegistry();
	globalOutputStream() << "MenuManager: Finished loading.\n";
}

MenuManager::~MenuManager() {
}

void MenuManager::loadFromRegistry() {
	xml::NodeList menuNodes = GlobalRegistry().findXPath(RKEY_MENU_ROOT);

	if (menuNodes.size() > 0) {
		for (unsigned int i = 0; i < menuNodes.size(); i++) {
			std::string name = menuNodes[i].getAttributeValue("name");

			// Allocate a new MenuItem with a NULL parent (i.e. root MenuItem)
			MenuItem* menubar = new MenuItem(_root);
			menubar->setName(name);

			// Populate the root menuitem using the current node
			menubar->parseNode(menuNodes[i], menubar);

			// Add the menubar as child of the root (child is already parented to _root)
			_root->addChild(menubar);
		}
	}
	else {
		globalErrorStream() << "MenuManager: Could not find menu root in registry.\n";
	}
}

GtkWidget* MenuManager::getMenu(const std::string& name) {
	MenuItem* foundMenu = _root->find(name);

	if (foundMenu != NULL) {
		return *foundMenu;
	}
	else {
		globalErrorStream() << "MenuManager: Warning: Menu " << name << " not found!\n";
		return NULL;
	}
}


void MenuManager::add(const std::string& menuPath,
					  const std::string& caption,
					  const std::string& eventName)
{
	MenuItem* found = _root->find(menuPath);

	if (found == NULL) {
		// Insert the menu item

	}
	else {
		globalErrorStream() << "MenuItem: " << menuPath << " already exists.\n";
	}
}

} // namespace ui
