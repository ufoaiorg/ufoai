#pragma once

#include "iuimanager.h"

#include "MenuManager.h"

namespace ui {

class UIManager :
	public IUIManager
{
	// Local helper class taking care of the menu
	MenuManager _menuManager;

public:

	UIManager();

	~UIManager();

	/** greebo: Retrieves the helper class to manipulate the menu.
	 */
	IMenuManager* getMenuManager();
}; // class UIManager

} // namespace ui
