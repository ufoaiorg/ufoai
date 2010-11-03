#include "MenuItem.h"

#include "ieventmanager.h"
#include "string/string.h"
#include "../mru/MRU.h"

#include "gtkutil/MenuItemAccelerator.h"
#include <gtk/gtk.h>

#include <iostream>

namespace ui {

	namespace {
		typedef std::vector<std::string> StringVector;
	}

MenuItem::MenuItem(MenuItem* parent) :
	_parent(parent),
	_widget(NULL),
	_type(eNothing),
	_constructed(false)
{
	if (_parent == NULL) {
		_type = eRoot;
	}
	else if (_parent->isRoot()) {
		_type = eMenuBar;
	}
}

MenuItem::~MenuItem() {
	for (MenuItemList::iterator i = _children.begin(); i != _children.end(); ) {
		delete (*i++);
	}
}

std::string MenuItem::getName() const {
	return _name;
}

void MenuItem::setName(const std::string& name) {
	_name = name;
}

bool MenuItem::isRoot() const {
	return (_type == eRoot);
}

MenuItem* MenuItem::getParent() const {
	return _parent;
}

void MenuItem::setParent(MenuItem* parent) {
	_parent = parent;
}

void MenuItem::setCaption(const std::string& caption) {
	_caption = caption;
}

std::string MenuItem::getCaption() const {
	return _caption;
}

void MenuItem::setIcon(const std::string& icon) {
	_icon = icon;
}

bool MenuItem::isEmpty() const {
	return (_type != eItem);
}

MenuItem::eType MenuItem::getType() const {
	return _type;
}

int MenuItem::numChildren() const {
	return _children.size();
}

void MenuItem::addChild(MenuItem* newChild) {
	_children.push_back(newChild);
}

std::string MenuItem::getEvent() const {
	return _event;
}

void MenuItem::setEvent(const std::string& eventName) {
	_event = eventName;
}

MenuItem::operator GtkWidget* () {
	// Check for toggle, allocate the GtkWidget*
	if (!_constructed) {
		construct();
	}
	return _widget;
}

MenuItem* MenuItem::find(const std::string& menuPath) {
	// Split the path and analyse it
	StringVector parts;
	string::splitBy(menuPath, parts, "/");

	// Any path items at all?
	if (parts.size() > 0) {
		MenuItem* child = NULL;

		// Path is not empty, try to find the first item among the item's children
		for (unsigned int i = 0; i < _children.size(); i++) {
			if (_children[i]->getName() == parts[0]) {
				child = _children[i];
			}
		}

		// The topmost name seems to be part of the children, pass the call
		if (child != NULL) {
			// Is this the end of the path (no more items)?
			if (parts.size() == 1) {
				// Yes, return the found item
				return child;
			}
			else {
				// No, pass the query down the hierarchy
				std::string childPath("");
				for (unsigned int i = 1; i < parts.size(); i++) {
					childPath += (childPath != "") ? "/" : "";
					childPath += parts[i];
				}
				return child->find(childPath);
			}
		}
	}

	// Nothing found, return NULL pointer
	return NULL;
}

void MenuItem::parseNode(xml::Node& node, MenuItem* thisItem) {
	std::string nodeName = node.getName();

	setName(node.getAttributeValue("name"));
	setCaption(node.getAttributeValue("caption"));

	if (nodeName == "menuItem") {
		_type = eItem;
		// Get the Event pointer according to the event
		setEvent(node.getAttributeValue("command"));
		setIcon(node.getAttributeValue("icon"));
	}
	else if (nodeName == "menuSeparator") {
		_type = eSeparator;
	}
	else if (nodeName == "subMenu") {
		_type = eFolder;

		xml::NodeList subNodes = node.getChildren();
		for (unsigned int i = 0; i < subNodes.size(); i++) {
			if (subNodes[i].getName() != "text") {
				// Allocate a new child menuitem with a pointer to <self>
				MenuItem* newChild = new MenuItem(thisItem);
				// Let the child parse the subnode
				newChild->parseNode(subNodes[i], newChild);

				// Add the child to the list
				_children.push_back(newChild);
			}
		}
	}
	else if (nodeName == "menu") {
		_type = eMenuBar;

		xml::NodeList subNodes = node.getChildren();
		for (unsigned int i = 0; i < subNodes.size(); i++) {
			if (subNodes[i].getName() != "text") {
				// Allocate a new child menuitem with a pointer to <self>
				MenuItem* newChild = new MenuItem(thisItem);
				// Let the child parse the subnode
				newChild->parseNode(subNodes[i], newChild);

				// Add the child to the list
				_children.push_back(newChild);
			}
		}
	}
	else if (nodeName == "menuMRU") {
		 _type = eMRU;
	}
	else {
		_type = eNothing;
		globalErrorStream() << "MenuItem: Unknown node found: " << nodeName << "\n";
	}
}

void MenuItem::construct() {
	if (_type == eMenuBar) {
		_widget = gtk_menu_bar_new();

		for (unsigned int i = 0; i < _children.size(); i++) {
			// Cast each children onto GtkWidget and append it to the menu
			gtk_menu_shell_append(GTK_MENU_SHELL(_widget), *_children[i]);
		}
	}
	else if (_type == eSeparator) {
		_widget = gtk_separator_menu_item_new();
	}
	else if (_type == eFolder) {
		// Create the menuitem
		_widget = gtk_menu_item_new_with_mnemonic(_caption.c_str());
		// Create the submenu
		GtkWidget* subMenu = gtk_menu_new();
		gtk_widget_show(subMenu);
		// Attach the submenu to the menuitem
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(_widget), subMenu);

		for (unsigned int i = 0; i < _children.size(); i++) {
			// Special case: the MRU menu (not a folder, but a series of widgets)
			if (_children[i]->getType() == eMRU) {
				WidgetList widgetList = GlobalMRU().getMenuWidgets();

				for (unsigned int w = 0; w < widgetList.size(); w++) {
					gtk_menu_shell_append(GTK_MENU_SHELL(subMenu), widgetList[w]);
				}
			}
			else {
				// Cast each children onto GtkWidget and append it to the menu
				gtk_menu_shell_append(GTK_MENU_SHELL(subMenu), *_children[i]);
			}
		}
	}
	else if (_type == eItem) {
		// Try to lookup the event name
		IEvent* event = GlobalEventManager().findEvent(_event);

		if (event != NULL) {
			// Retrieve an acclerator string formatted for a menu
			const std::string accelText =
				GlobalEventManager().getAcceleratorStr(event, true);

			// Create a new menuitem
			_widget = gtkutil::TextMenuItemAccelerator(_caption,
														accelText,
														_icon,
														event->isToggle());

			gtk_widget_show_all(_widget);

			// Connect the widget to the event
			event->connectWidget(_widget);
		}
		else {
			globalErrorStream() << "MenuItem: Cannot find associated event: " << _event << "\n";
		}
	}
	else if (_type == eMRU) {
		// Do nothing.
	}
	else if (_type == eRoot) {
		globalErrorStream() << "MenuItem: Cannot instantiate root MenuItem.\n";
	}

	if (_widget != NULL) {
		gtk_widget_show(_widget);
	}

	_constructed = true;
}

} // namespace ui
