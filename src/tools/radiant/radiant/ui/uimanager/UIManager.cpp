#include "UIManager.h"

#include "iregistry.h"
#include "ieventmanager.h"

namespace ui {

UIManager::UIManager() {
}

UIManager::~UIManager() {
}

GtkWidget* UIManager::getMenu(const std::string& name) {
	return _menuManager.getMenu(name);
}

void UIManager::addMenuItem(const std::string& menuPath,
							const std::string& caption,
							const std::string& eventName)
{
	 _menuManager.add(menuPath, caption, eventName);
}

} // namespace ui

// Module stuff

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class UIManagerDependencies :
	public GlobalEventManagerModuleRef,
	public GlobalRegistryModuleRef
{};


class UIManagerAPI
{
	ui::UIManager* _uiManager;

public:
	typedef IUIManager Type;
	STRING_CONSTANT(Name, "*");

	// Constructor
	UIManagerAPI() {
		// allocate a new UIManager instance on the heap (shared_ptr)
		_uiManager = new ui::UIManager;
	}

	IUIManager* getTable() {
		return _uiManager;
	}

	~UIManagerAPI() {
		delete _uiManager;
	}
};

typedef SingletonModule<UIManagerAPI, UIManagerDependencies> UIManagerModule;
typedef Static<UIManagerModule> StaticUIManagerModule;
StaticRegisterModule staticRegisterUIManager(StaticUIManagerModule::instance());
