#include "ieventmanager.h"

#include "iregistry.h"
#include "iselection.h"
#include "iradiant.h"

#include <iostream>

#include <gdk/gdkevents.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtktextview.h>

#include "xmlutil/Node.h"

#include "MouseEvents.h"
#include "Modifiers.h"
#include "Command.h"
#include "Toggle.h"
#include "WidgetToggle.h"
#include "RegistryToggle.h"
#include "KeyEvent.h"
#include "Accelerator.h"
#include "SaveEventVisitor.h"

#include "string/string.h"

class EventManager :
	public IEventManager
{
	// The handler ID of the connected keyboard handler
	typedef std::map<gulong, GtkObject*> HandlerMap;

	// Needed for string::split
	typedef std::vector<std::string> StringParts;

	// Each command has a name, this is the map where the name->command association is stored
	typedef std::map<const std::string, IEvent*> EventMap;

	// The list of all allocated Accelerators
	typedef std::vector<Accelerator*> AcceleratorList;

	// The list of connect (top-level) windows, whose keypress events are immediately processed
	HandlerMap _handlers;

	// The list of connected dialog window handlers, whose keypress events are
	// processed AFTER the dialog window's default keyboard handler.
	HandlerMap _dialogWindows;

	// The list containing all registered accelerator objects
	AcceleratorList _accelerators;

	// The map of all registered events
	EventMap _events;

	// The GTK accelerator group for the main window
	GtkAccelGroup* _accelGroup;

	Modifiers _modifiers;
	MouseEventManager _mouseEvents;

public:
	// Radiant Module stuff
	typedef IEventManager Type;
	STRING_CONSTANT(Name, "*");

	// Return the static instance
	IEventManager* getTable() {
		return this;
	}

	// Constructor
	EventManager () :
			_modifiers(), _mouseEvents(_modifiers)
	{
		globalOutputStream() << "EventManager started.\n";

		// Create an empty GClosure
		_accelGroup = gtk_accel_group_new();

		globalOutputStream() << "EventManager started.\n";
	}

	// Destructor, free all allocated objects and un-reference the GTK accelerator group
	~EventManager() {
		g_object_unref(_accelGroup);

		saveEventListToRegistry();

		// Remove all accelerators from the heap
		for (AcceleratorList::iterator i = _accelerators.begin(); i != _accelerators.end(); ++i) {
			Accelerator* accelerator = (*i);
			delete accelerator;
		}
		_accelerators.clear();

		// Remove all commands from the heap
		for (EventMap::iterator i = _events.begin(); i != _events.end(); ++i) {
			IEvent* event = i->second;
			delete event;
		}
		_events.clear();

		globalOutputStream() << "EventManager successfully shut down.\n";
	}

	void connectSelectionSystem(SelectionSystem* selectionSystem) {
		_mouseEvents.connectSelectionSystem(selectionSystem);
	}

	// Returns a reference to the mouse event mapper
	IMouseEvents& MouseEvents() {
		return _mouseEvents;
	}

	IAccelerator* addAccelerator(const std::string& key, const std::string& modifierStr) {
		guint keyVal = getGDKCode(key);
		unsigned int modifierFlags = _modifiers.getModifierFlags(modifierStr);

		// Allocate a new accelerator object on the heap
		Accelerator* accelerator = new Accelerator(keyVal, modifierFlags);

		_accelerators.push_back(accelerator);

		// return the pointer to the new accelerator
		return accelerator;
	}

	std::string getAcceleratorStr(const IEvent* event, bool forMenu) {
		std::string returnValue = "";

		IAccelerator* accelerator = findAccelerator(event);

		if (accelerator == NULL)
			return "";

		unsigned int keyVal = accelerator->getKey();
		const std::string keyStr = (keyVal != 0) ? gdk_keyval_name(keyVal) : "";

		if (keyStr != "") {
			// Return a modifier string for a menu
			const std::string modifierStr = getModifierStr(accelerator->getModifiers(), forMenu);

			const std::string connector = (forMenu) ? "-" : "+";

			returnValue = modifierStr;
			returnValue += (modifierStr != "") ? connector : "";
			returnValue += keyStr;
		}

		return returnValue;
	}

	IAccelerator* addAccelerator(GdkEventKey* event) {
		// Create a new accelerator with the given arguments
		Accelerator* accelerator = new Accelerator(event->keyval, _modifiers.getKeyboardFlags(event->state));

		// Add a new Accelerator to the list
		_accelerators.push_back(accelerator);

		// return the reference to the last accelerator in the list
		AcceleratorList::reverse_iterator i = _accelerators.rbegin();

		return (*i);
	}

	IEvent* findEvent(const std::string& name) {
		// Try to lookup the command
		EventMap::iterator i = _events.find(name);

		if (i != _events.end()) {
			// Return the pointer to the command
			return i->second;
		}
		else {
			// Nothing found, return NULL
			return NULL;
		}
	}

	IEvent* findEvent(GdkEventKey* event) {
		// Retrieve the accelerators for this eventkey
		AcceleratorList accelList = findAccelerator(event);

		// Did we find any matching accelerators?
		if (!accelList.empty()) {
			// Take the first found accelerator
			Accelerator* accel = *accelList.begin();

			return accel->getEvent();
		}
		else {
			// No accelerators found
			return NULL;
		}
	}

	std::string getEventName(IEvent* event) {
		// Try to lookup the given eventptr
		for (EventMap::iterator i = _events.begin(); i != _events.end(); ++i) {
			if (i->second == event) {
				return i->first;
			}
		}

		return "";
	}


	// Add the given command to the internal list
	IEvent* addCommand(const std::string& name, const Callback& callback) {

		// Try to find the command and see if it's already registered
		IEvent* foundEvent = findEvent(name);

		if (foundEvent == NULL) {
			// Construct a new command with the given callback
			Command* cmd = new Command(callback);

			// Add the command to the list (implicitly cast the pointer on IEvent*)
			_events[name] = cmd;

			globalOutputStream() << "EventManager: Event " << name << " registered!\n";

			// Return the pointer to the newly created event
			return cmd;
		}
		else {
			globalOutputStream() << "EventManager: Warning: Event " << name << " already registered!\n";
			return foundEvent;
		}
	}

	IEvent* addKeyEvent(const std::string& name, const Callback& keyUpCallback, const Callback& keyDownCallback) {
		// Try to find the command and see if it's already registered
		IEvent* foundEvent = findEvent(name);

		if (foundEvent == NULL) {
			// Construct a new keyevent with the given callback
			KeyEvent* keyevent = new KeyEvent(keyUpCallback, keyDownCallback);

			// Add the command to the list (implicitly cast the pointer on IEvent*)
			_events[name] = keyevent;

			globalOutputStream() << "EventManager: Event " << name << " registered!\n";

			// Return the pointer to the newly created event
			return keyevent;
		}
		else {
			globalOutputStream() << "EventManager: Warning: Event " << name << " already registered!\n";
			return foundEvent;
		}
	}

	IEvent* addWidgetToggle(const std::string& name) {
		// Try to find the command and see if it's already registered
		IEvent* foundEvent = findEvent(name);

		if (foundEvent == NULL) {
			// Construct a new command with the given <onToggled> callback
			WidgetToggle* widgetToggle = new WidgetToggle();

			// Add the command to the list (implicitly cast the pointer on IEvent*)
			_events[name] = widgetToggle;

			globalOutputStream() << "EventManager: Event " << name << " registered!\n";

			// Return the pointer to the newly created event
			return widgetToggle;
		}
		else {
			globalOutputStream() << "EventManager: Warning: Event " << name << " already registered!\n";
			return foundEvent;
		}
	}

	IEvent* addRegistryToggle(const std::string& name, const std::string& registryKey) {
		// Try to find the command and see if it's already registered
		IEvent* foundEvent = findEvent(name);

		if (foundEvent == NULL) {
			// Construct a new command with the given <onToggled> callback
			RegistryToggle* registryToggle = new RegistryToggle(registryKey);

			// Add the command to the list (implicitly cast the pointer on IEvent*)
			_events[name] = registryToggle;

			// Return the pointer to the newly created event
			return registryToggle;
		}
		else {
			globalOutputStream() << "EventManager: Warning: Event " << name << " already registered!\n";
			return foundEvent;
		}
	}

	IEvent* addToggle(const std::string& name, const Callback& onToggled) {
		// Try to find the command and see if it's already registered
		IEvent* foundEvent = findEvent(name);

		if (foundEvent == NULL) {
			// Construct a new command with the given <onToggled> callback
			Toggle* toggle = new Toggle(onToggled);

			// Add the command to the list (implicitly cast the pointer on IEvent*)
			_events[name] = toggle;

			globalOutputStream() << "EventManager: Event " << name << " registered!\n";

			// Return the pointer to the newly created event
			return toggle;
		}
		else {
			globalOutputStream() << "EventManager: Warning: Event " << name << " already registered!\n";
			return foundEvent;
		}
	}

	void setToggled(const std::string& name, const bool toggled) {
		// Try to find the command and see if it's already registered
		Toggle* foundToggle = dynamic_cast<Toggle*>(findEvent(name));

		if (foundToggle != NULL) {
			if (!foundToggle->setToggled(toggled))
				globalErrorStream() << "EventManager: Warning: Event " << name << " is not a Toggle.\n";
		}
	}

	// Connects the given accelerator to the given command (identified by the string)
	void connectAccelerator(IAccelerator* accelerator, const std::string& command) {
		// Sanity check
		if (accelerator != NULL) {
			IEvent* event = findEvent(command);

			if (event != NULL) {
				// Command found, connect it to the accelerator by passing its pointer
				accelerator->connectEvent(event);
			}
			else {
				// Command NOT found
				globalOutputStream() << "EventManager: Unable to lookup command: " << command << "\n";
			}
		}
	}

	void disconnectAccelerator(const std::string& command) {
		IEvent* event = findEvent(command);

		if (event != NULL) {
			// Cycle through the accelerators and check for matches
			for (AcceleratorList::iterator i = _accelerators.begin(); i != _accelerators.end(); ++i) {
				if ((*i)->match(event)) {
					// Connect the accelerator to the empty event (disable the accelerator)
					(*i)->connectEvent(NULL);
					(*i)->setKey(0);
					(*i)->setModifiers(0);
				}
			}
		}
		else {
			// Command NOT found
			globalOutputStream() << "EventManager: Unable to disconnect command: " << command << "\n";
		}
	}

	void disableEvent(const std::string& eventName) {
		IEvent* event = findEvent(eventName);

		if (event != NULL) {
			event->setEnabled(false);
		}
	}

	void enableEvent(const std::string& eventName) {
		IEvent* event = findEvent(eventName);

		if (event != NULL) {
			event->setEnabled(true);
		}
	}

	// Catches the key/mouse press/release events from the given GtkObject
	void connect(GtkObject* object)	{
		// Create and store the handler into the map
		gulong handlerId = g_signal_connect(G_OBJECT(object), "key_press_event", G_CALLBACK(onKeyPress), this);
		_handlers[handlerId] = object;

		handlerId = g_signal_connect(G_OBJECT(object), "key_release_event", G_CALLBACK(onKeyRelease), this);
		_handlers[handlerId] = object;
	}

	void disconnect(GtkObject* object) {
		for (HandlerMap::iterator i = _handlers.begin(); i != _handlers.end();) {
			if (i->second == object) {
				g_signal_handler_disconnect(G_OBJECT(i->second), i->first);
				// Be sure to increment the iterator with a postfix ++, so that the "old"
				_handlers.erase(i++);
			} else {
				++i;
			}
		}
	}

	/* greebo: This connects an dialog window to the event handler. This means the following:
	 *
	 * An incoming key-press event reaches the static method onDialogKeyPress which
	 * passes the key event to the connect dialog FIRST, before the key event has a
	 * chance to be processed by the standard shortcut processor. IF the dialog window
	 * standard handler returns TRUE, that is. If the gtk_window_propagate_key_event()
	 * function returns FALSE, the window couldn't find a use for this specific key event
	 * and the event can be passed safely to the onKeyPress() method.
	 *
	 * This way it is ensured that the dialog window can handle, say, text entries without
	 * firing global shortcuts all the time.
	 */
	void connectDialogWindow(GtkWindow* window) {
		gulong handlerId = g_signal_connect(G_OBJECT(window), "key-press-event",
											G_CALLBACK(onDialogKeyPress), this);

		_dialogWindows[handlerId] = GTK_OBJECT(window);

		handlerId = g_signal_connect(G_OBJECT(window), "key-release-event",
									 G_CALLBACK(onDialogKeyRelease), this);

		_dialogWindows[handlerId] = GTK_OBJECT(window);
	}

	void disconnectDialogWindow(GtkWindow* window) {
		GtkObject* object = GTK_OBJECT(window);

		for (HandlerMap::iterator i = _dialogWindows.begin(); i != _dialogWindows.end(); ) {
			// If the object pointer matches the one stored in the list, remove the handler id
			if (i->second == object) {
				g_signal_handler_disconnect(G_OBJECT(i->second), i->first);
				// Be sure to increment the iterator with a postfix ++, so that the "old" iterator is passed
				_dialogWindows.erase(i++);
			}
			else {
				++i;
			}
		}
	}

	void connectAccelGroup(GtkWindow* window) {
		gtk_window_add_accel_group(window, _accelGroup);
	}

	void loadAccelerators() {
		xml::NodeList shortcutSets = GlobalRegistry().findXPath("user/ui/input//shortcuts");

		// If we have two sets of shortcuts, delete the default ones
		if (shortcutSets.size() > 1) {
			GlobalRegistry().deleteXPath("user/ui/input//shortcuts[@name='default']");
		}

		// Find all accelerators
		xml::NodeList shortcutList = GlobalRegistry().findXPath("user/ui/input/shortcuts//shortcut");

		if (!shortcutList.empty()) {
			globalOutputStream() << "EventManager: Shortcuts found in Registry: " << shortcutList.size() << "\n";
			for (unsigned int i = 0; i < shortcutList.size(); i++) {
				const std::string key = shortcutList[i].getAttributeValue("key");
				const std::string command = shortcutList[i].getAttributeValue("command");

				// Try to lookup the command
				IEvent* event = findEvent(command);

				// Check if valid key / command definitions were found
				if (key != "" && event != NULL) {
					// Get the modifier string (e.g. "SHIFT+ALT")
					const std::string modifierStr = shortcutList[i].getAttributeValue("modifiers");

					if (!duplicateAccelerator(key, modifierStr, event)) {
						// Create the accelerator object
						IAccelerator* accelerator = addAccelerator(key, modifierStr);

						// Connect the newly created accelerator to the command
						accelerator->connectEvent(event);
					}
				}
			}
		}
		else {
			// No accelerator definitions found!
			globalOutputStream() << "EventManager: No shortcut definitions found...\n";
		}
	}

	void removeEvent(const std::string& eventName) {
		// Try to lookup the command
		EventMap::iterator i = _events.find(eventName);

		if (i != _events.end()) {
			// Remove all accelerators beforehand
			disconnectAccelerator(eventName);

			// Remove the event from the list
			_events.erase(i);
		}
	}

	void foreachEvent(IEventVisitor* eventVisitor) {
		// Cycle through the event and pass them to the visitor class
		for (EventMap::iterator i = _events.begin(); i != _events.end(); ++i) {
			const std::string eventName = i->first;
			IEvent* event = i->second;

			eventVisitor->visit(eventName, event);
		}
	}

	// Tries to locate an accelerator, that is connected to the given command
	IAccelerator* findAccelerator(const IEvent* event) {
		// Cycle through the accelerators and check for matches
		for (unsigned int i = 0; i < _accelerators.size(); i++) {
			if (_accelerators[i]->match(event)) {
				// Return the pointer to the found accelerator
				return _accelerators[i];
			}
		}

		return NULL;
	}

	// Returns a bit field with the according modifier flags set
	std::string getModifierStr(const unsigned int& modifierFlags, bool forMenu = false) {
		// Pass the call to the modifiers helper class
		return _modifiers.getModifierStr(modifierFlags, forMenu);
	}

private:

	void saveEventListToRegistry() {
		const std::string rootKey = "user/ui/input";

		// The visitor class to save each event definition into the registry
		// Note: the SaveEventVisitor automatically wipes all the existing shortcuts from the registry
		SaveEventVisitor visitor(rootKey, this);

		foreachEvent(&visitor);
	}

	bool duplicateAccelerator(const std::string& key, const std::string& modifiers, IEvent* event) {
		AcceleratorList list = findAccelerator(key, modifiers);

		for (unsigned int i = 0; i < list.size(); i++) {
			// If one of the accelerators in the list matches the event, return true
			if (list[i]->match(event)) {
				return true;
			}
		}

		return false;
	}

	AcceleratorList findAccelerator(const guint& keyVal, const unsigned int& modifierFlags) {
		AcceleratorList returnList;

		// Cycle through the accelerators and check for matches
		for (unsigned int i = 0; i < _accelerators.size(); i++) {
			if (_accelerators[i]->match(keyVal, modifierFlags)) {
				// Add the pointer to the found accelerators
				returnList.push_back(_accelerators[i]);
			}
		}

		return returnList;
	}

	AcceleratorList findAccelerator(const std::string& key, const std::string& modifierStr) {
		guint keyVal = getGDKCode(key);
		unsigned int modifierFlags = _modifiers.getModifierFlags(modifierStr);

		return findAccelerator(keyVal, modifierFlags);
	}

	// Returns the pointer to the accelerator for the given GdkEvent, but convert the key to uppercase before passing it
	AcceleratorList findAccelerator(GdkEventKey* event) {
		unsigned int keyval = gdk_keyval_to_upper(event->keyval);

		// greebo: I saw this in the original GTKRadiant code, maybe this is necessary to catch GTK_ISO_Left_Tab...
		if (keyval == GDK_ISO_Left_Tab) {
			keyval = GDK_Tab;
		}

		return findAccelerator(keyval, _modifiers.getKeyboardFlags(event->state));
	}

	void updateStatusText(GdkEventKey* event, bool keyPress) {
		// Make a copy of the given event key
		GdkEventKey eventKey = *event;

		// Sometimes the ALT modifier is not set, so this is a workaround for this
		if (eventKey.keyval == GDK_Alt_L || eventKey.keyval == GDK_Alt_R) {
			if (keyPress) {
				eventKey.state |= GDK_MOD1_MASK;
			}
			else {
				eventKey.state &= ~GDK_MOD1_MASK;
			}
		}

		_mouseEvents.updateStatusText(&eventKey);
	}

	// The GTK keypress callback
	static gboolean onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer data) {
		// Convert the passed pointer onto a KeyEventManager pointer
		EventManager* self = reinterpret_cast<EventManager*>(data);

		if (!GTK_IS_WIDGET(widget))
			return FALSE;

		if (GTK_IS_WINDOW(widget)) {
			// Pass the key event to the connected window and see if it can process it (returns TRUE)
			gboolean keyProcessed = gtk_window_propagate_key_event(GTK_WINDOW(widget), event);

			// Get the focus widget, is it an editable widget?
			GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(widget));
			bool isEditableWidget = GTK_IS_EDITABLE(focus) || GTK_IS_TEXT_VIEW(focus);

			// Never propagate keystrokes if editable widgets are focused
			if ((isEditableWidget && event->keyval != GDK_Escape) || keyProcessed) {
				return keyProcessed;
			}
		}

		// Try to find a matching accelerator
		AcceleratorList accelList = self->findAccelerator(event);

		if (!accelList.empty()) {
			// Release any modifiers
			self->_modifiers.setState(0);

			// Pass the execute() call to all found accelerators
			for (unsigned int i = 0; i < accelList.size(); i++) {
				Accelerator* accelerator = dynamic_cast<Accelerator*>(accelList[i]);

				if (accelerator != NULL) {
					// A matching accelerator has been found, pass the keyDown event
					accelerator->keyDown();
				}
			}

			return true;
		}

		self->_modifiers.updateState(event, true);

		self->updateStatusText(event, true);

		return false;
	}

	// The GTK keypress callback
	static gboolean onKeyRelease(GtkWidget* widget, GdkEventKey* event, gpointer data) {
		// Convert the passed pointer onto a KeyEventManager pointer
		EventManager* self = reinterpret_cast<EventManager*>(data);

		if (!GTK_IS_WIDGET(widget))
			return FALSE;

		if (GTK_IS_WINDOW(widget)) {
			// Pass the key event to the connected window and see if it can process it (returns TRUE)
			gboolean keyProcessed = gtk_window_propagate_key_event(GTK_WINDOW(widget), event);

			// Get the focus widget, is it an editable widget?
			GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(widget));
			bool isEditableWidget = GTK_IS_EDITABLE(focus) || GTK_IS_TEXT_VIEW(focus);

			// Never propagate keystrokes if editable widgets are focused
			if ((isEditableWidget && event->keyval != GDK_Escape) || keyProcessed) {
				return keyProcessed;
			}
		}

		// Try to find a matching accelerator
		AcceleratorList accelList = self->findAccelerator(event);

		if (!accelList.empty()) {

			// Pass the execute() call to all found accelerators
			for (unsigned int i = 0; i < accelList.size(); i++) {
				Accelerator* accelerator = dynamic_cast<Accelerator*>(accelList[i]);

				if (accelerator != NULL) {
					// A matching accelerator has been found, pass the keyDown event
					accelerator->keyUp();
				}
			}

			return true;
		}

		self->_modifiers.updateState(event, false);

		self->updateStatusText(event, false);

		return false;
	}

	// The GTK keypress callback
	static gboolean onDialogKeyPress(GtkWindow* window, GdkEventKey* event, EventManager* self) {
		// Pass the key event to the connected dialog window and see if it can process it (returns TRUE)
		gboolean keyProcessed = gtk_window_propagate_key_event(window, event);

		// Get the focus widget, is it an editable widget?
		GtkWidget* focus = gtk_window_get_focus(window);
		bool isEditableWidget = GTK_IS_EDITABLE(focus) || GTK_IS_TEXT_VIEW(focus);

		// never pass onKeyPress event to the accelerator manager if an editable widget is focused
		// the only exception is the ESC key
		if (isEditableWidget && event->keyval != GDK_Escape) {
			return keyProcessed;
		}

		if (!keyProcessed) {
			// The dialog window returned FALSE, pass the key on to the default onKeyPress handler
			self->onKeyPress(GTK_WIDGET(window), event, self);
		}

		// If we return true here, the dialog window could process the key, and the GTK callback chain is stopped
		return keyProcessed;
	}

	// The GTK keyrelease callback
	static gboolean onDialogKeyRelease(GtkWindow* window, GdkEventKey* event, EventManager* self) {
		// Pass the key event to the connected dialog window and see if it can process it (returns TRUE)
		gboolean keyProcessed = gtk_window_propagate_key_event(window, event);

		// Get the focus widget, is it an editable widget?
		GtkWidget* focus = gtk_window_get_focus(window);
		bool isEditableWidget = GTK_IS_EDITABLE(focus) || GTK_IS_TEXT_VIEW(focus);

		if (isEditableWidget && event->keyval != GDK_Escape) {
			// never pass onKeyPress event to the accelerator manager if an editable widget is focused
			return keyProcessed;
		}

		if (!keyProcessed) {
			// The dialog window returned FALSE, pass the key on to the default onKeyPress handler
			self->onKeyRelease(GTK_WIDGET(window), event, self);
		}

		// If we return true here, the dialog window could process the key, and the GTK callback chain is stopped
		return keyProcessed;
	}

	guint getGDKCode(const std::string& keyStr) {
		guint returnValue = gdk_keyval_to_upper(gdk_keyval_from_name(keyStr.c_str()));

		if (returnValue == GDK_VoidSymbol) {
			globalOutputStream() << "EventManager: Warning: Could not recognise key " << keyStr << "\n";
		}

		return returnValue;
	}

	bool isModifier(GdkEventKey* event) {
		return (event->keyval == GDK_Control_L || event->keyval == GDK_Control_R ||
				event->keyval == GDK_Shift_L || event->keyval == GDK_Shift_R ||
				event->keyval == GDK_Alt_L || event->keyval == GDK_Alt_R ||
				event->keyval == GDK_Meta_L || event->keyval == GDK_Meta_R);
	}

	std::string getGDKEventStr(GdkEventKey* event) {
		std::string returnValue("");

		// Don't react on modifiers only (no actual key like A, 2, U, etc.)
		if (isModifier(event)) {
			return returnValue;
		}

		// Convert the GDKEvent state into modifier flags
		const unsigned int modifierFlags = _modifiers.getKeyboardFlags(event->state);

		// Construct the complete string
		returnValue += _modifiers.getModifierStr(modifierFlags, true);
		returnValue += (returnValue != "") ? "-" : "";
		returnValue += gdk_keyval_name(gdk_keyval_to_upper(event->keyval));

		return returnValue;
	}

	unsigned int getModifierState() {
		return _modifiers.getState();
	}
}; // class EventManager

/* EventManager dependencies class.
 */
class EventManagerDependencies :
	public GlobalRadiantModuleRef,
	public GlobalRegistryModuleRef
{
};

/* Required code to register the module with the ModuleServer.
 */

#include "modulesystem/singletonmodule.h"

typedef SingletonModule<EventManager, EventManagerDependencies> EventManagerModule;

typedef Static<EventManagerModule> StaticEventManagerSystemModule;
StaticRegisterModule staticRegisterEventManagerSystem(StaticEventManagerSystemModule::instance());
