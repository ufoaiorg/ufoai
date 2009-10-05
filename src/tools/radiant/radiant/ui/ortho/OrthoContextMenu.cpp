#include "OrthoContextMenu.h"
#include "EntityClassChooser.h"
#include "gtkutil/IconTextMenuItem.h"

#include "iselection.h"
#include "../../sidebar/surfaceinspector.h" // SurfaceInspector_FitTexture()
#include "../../entity.h" // Entity_createFromSelection(), Entity_connectSelected()
#include "gtkutil/dialog.h"
#include "../../map.h"
#include "imaterial.h"
#include "radiant_i18n.h"
#include "../Icons.h"

namespace ui
{
	namespace
	{
		/* CONSTANTS */
		const char* LIGHT_CLASSNAME = "light";
		const char* MODEL_CLASSNAME = "misc_model";
		const char* SOUND_CLASSNAME = "misc_sound";

		const char* ADD_MODEL_TEXT = _("Create model...");
		const char* ADD_LIGHT_TEXT = _("Create light...");
		const char* ADD_SOUND_TEXT = _("Create sound...");
		const char* ADD_ENTITY_TEXT = _("Create entity...");
		const char* CONNECT_ENTITIES_TEXT = _("Connect entities...");
		const char* FIT_TEXTURE_TEXT = _("Fit textures...");
		const char* GENERATE_MATERIALS_TEXT = _("Generate materials...");
	}

	// Static class function to display the singleton instance.
	void OrthoContextMenu::displayInstance (const Vector3& point)
	{
		static OrthoContextMenu _instance;
		_instance.show(point);
	}

	// Constructor. Create GTK widgets here.
	OrthoContextMenu::OrthoContextMenu () :
		_widget(gtk_menu_new())
	{
		GtkWidget* addModel = gtkutil::IconTextMenuItem(ui::icons::ICON_ADD_MODEL, ADD_MODEL_TEXT);
		GtkWidget* addLight = gtkutil::IconTextMenuItem(ui::icons::ICON_ADD_LIGHT, ADD_LIGHT_TEXT);
		GtkWidget* addSound = gtkutil::IconTextMenuItem(ui::icons::ICON_ADD_SOUND, ADD_SOUND_TEXT);
		GtkWidget* addEntity = gtkutil::IconTextMenuItem(ui::icons::ICON_ADD_ENTITY, ADD_ENTITY_TEXT);

		// Context sensitive menu items
		_connectEntities = gtkutil::IconTextMenuItem(ui::icons::ICON_CONNECT_ENTITIES, CONNECT_ENTITIES_TEXT);
		_fitTexture = gtkutil::IconTextMenuItem(ui::icons::ICON_FIT_TEXTURE, FIT_TEXTURE_TEXT);
		_generateMaterials = gtkutil::IconTextMenuItem(ui::icons::ICON_GENERATE_MATERIALS, GENERATE_MATERIALS_TEXT);

		g_signal_connect(G_OBJECT(addEntity), "activate", G_CALLBACK(callbackAddEntity), this);
		g_signal_connect(G_OBJECT(addLight), "activate", G_CALLBACK(callbackAddLight), this);
		g_signal_connect(G_OBJECT(addModel), "activate", G_CALLBACK(callbackAddModel), this);
		g_signal_connect(G_OBJECT(addSound), "activate", G_CALLBACK(callbackAddSound), this);
		g_signal_connect(G_OBJECT(_connectEntities), "activate", G_CALLBACK(callbackConnectEntities), this);
		g_signal_connect(G_OBJECT(_fitTexture), "activate", G_CALLBACK(callbackFitTexture), this);
		g_signal_connect(G_OBJECT(_generateMaterials), "activate", G_CALLBACK(callbackGenerateMaterials), this);

		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), addModel);
		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), addLight);
		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), addSound);
		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), addEntity);
		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), gtk_separator_menu_item_new());
		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), _connectEntities);
		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), _fitTexture);
		gtk_menu_shell_append(GTK_MENU_SHELL(_widget), _generateMaterials);

		gtk_widget_show_all(_widget);
	}

	// Show the menu
	void OrthoContextMenu::show (const Vector3& point)
	{
		_lastPoint = point;
		checkConnectEntities();
		checkFitTexture();
		checkGenerateMaterial();
		gtk_menu_popup(GTK_MENU(_widget), NULL, NULL, NULL, NULL, 1, GDK_CURRENT_TIME);
	}

	void OrthoContextMenu::checkConnectEntities ()
	{
		int countSelected = GlobalSelectionSystem().countSelected();
		if (countSelected == 2) {
			gtk_widget_set_sensitive(_connectEntities, TRUE);
		} else {
			gtk_widget_set_sensitive(_connectEntities, FALSE);
		}
	}

	void OrthoContextMenu::checkGenerateMaterial ()
	{
		const std::string& mapname = map::getName();
		GlobalSelectionSystem().countSelectedComponents();
		const int countSelectedPrimitives = GlobalSelectionSystem().countSelected();
		const int countSelectedComponents = GlobalSelectionSystem().countSelectedComponents();
		if ((countSelectedPrimitives == 0 && countSelectedComponents == 0) || mapname.empty() || Map_Unnamed(g_map)) {
			gtk_widget_set_sensitive(_generateMaterials, FALSE);
		} else {
			gtk_widget_set_sensitive(_generateMaterials, TRUE);
		}
	}

	void OrthoContextMenu::checkFitTexture ()
	{
		int countSelected = GlobalSelectionSystem().countSelected();
		if (countSelected > 0) {
			gtk_widget_set_sensitive(_fitTexture, TRUE);
		} else {
			gtk_widget_set_sensitive(_fitTexture, FALSE);
		}
	}

	/* GTK CALLBACKS */

	void OrthoContextMenu::callbackGenerateMaterials (GtkMenuItem* item, OrthoContextMenu* self)
	{
		GlobalMaterialSystem()->generateMaterialFromTexture();
	}

	void OrthoContextMenu::callbackConnectEntities (GtkMenuItem* item, OrthoContextMenu* self)
	{
		Entity_connectSelected();
	}

	void OrthoContextMenu::callbackFitTexture (GtkMenuItem* item, OrthoContextMenu* self)
	{
		SurfaceInspector_FitTexture();
	}

	void OrthoContextMenu::callbackAddEntity (GtkMenuItem* item, OrthoContextMenu* self)
	{
		EntityClassChooser::displayInstance(self->_lastPoint);
	}

	void OrthoContextMenu::callbackAddLight (GtkMenuItem* item, OrthoContextMenu* self)
	{
		Entity_createFromSelection(LIGHT_CLASSNAME, self->_lastPoint);
	}

	void OrthoContextMenu::callbackAddModel (GtkMenuItem* item, OrthoContextMenu* self)
	{
		Entity_createFromSelection(MODEL_CLASSNAME, self->_lastPoint);
	}

	void OrthoContextMenu::callbackAddSound (GtkMenuItem* item, OrthoContextMenu* self)
	{
		Entity_createFromSelection(SOUND_CLASSNAME, self->_lastPoint);
	}

} // namespace ui
