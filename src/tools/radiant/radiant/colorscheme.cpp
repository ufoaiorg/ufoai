/**
 * @file colorscheme.cpp
 */


#include "radiant_i18n.h"
#include "colorscheme.h"
#include "mainframe.h"
#include "camera/camwindow.h"
#include "sidebar/sidebar.h"
#include "xyview/xywindow.h"
#include "eclasslib.h"
#include "commands.h"
#include "settings/preferences.h"
#include "gtkmisc.h"
#include "gtkutil/menu.h"
#include "brush/brushmodule.h"
#include "moduleobservers.h"

/**
 * @brief Make COLOR_BRUSHES override worldspawn eclass colour.
 */
static void SetWorldspawnColour (const Vector3& colour)
{
	EntityClass* worldspawn = GlobalEntityClassManager().findOrInsert("worldspawn", true);
	eclass_release_state(worldspawn);
	worldspawn->color = colour;
	eclass_capture_state(worldspawn);
}

class WorldspawnColourEntityClassObserver: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		WorldspawnColourEntityClassObserver () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				SetWorldspawnColour(g_xywindow_globals.color_brushes);
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
			}
		}
};

static WorldspawnColourEntityClassObserver g_WorldspawnColourEntityClassObserver;

typedef Callback1<Vector3&> GetColourCallback;
typedef Callback1<const Vector3&> SetColourCallback;

class ChooseColour
{
		GetColourCallback m_get;
		SetColourCallback m_set;
	public:
		ChooseColour (const GetColourCallback& get, const SetColourCallback& set) :
			m_get(get), m_set(set)
		{
		}
		void operator() (void)
		{
			Vector3 colour;
			m_get(colour);
			color_dialog(GTK_WIDGET(GlobalRadiant().getMainWindow()), colour);
			m_set(colour);
		}
};

void Colour_get (const Vector3& colour, Vector3& other)
{
	other = colour;
}
typedef ConstReferenceCaller1<Vector3, Vector3&, Colour_get> ColourGetCaller;

void Colour_set (Vector3& colour, const Vector3& other)
{
	colour = other;
	SceneChangeNotify();
}
typedef ReferenceCaller1<Vector3, const Vector3&, Colour_set> ColourSetCaller;

void BrushColour_set (const Vector3& other)
{
	g_xywindow_globals.color_brushes = other;
	SetWorldspawnColour(g_xywindow_globals.color_brushes);
	SceneChangeNotify();
}
typedef FreeCaller1<const Vector3&, BrushColour_set> BrushColourSetCaller;

void ClipperColour_set (const Vector3& other)
{
	g_xywindow_globals.color_clipper = other;
	Brush_clipperColourChanged();
	SceneChangeNotify();
}
typedef FreeCaller1<const Vector3&, ClipperColour_set> ClipperColourSetCaller;

void TextureBrowserColour_get (Vector3& other)
{
	other = GlobalTextureBrowser().getBackgroundColour();
}
typedef FreeCaller1<Vector3&, TextureBrowserColour_get> TextureBrowserColourGetCaller;

void TextureBrowserColour_set (const Vector3& other)
{
	GlobalTextureBrowser().setBackgroundColour(other);
}
typedef FreeCaller1<const Vector3&, TextureBrowserColour_set> TextureBrowserColourSetCaller;

class ColoursMenu
{
	public:
		ChooseColour m_textureback;
		ChooseColour m_xyback;
		ChooseColour m_gridmajor;
		ChooseColour m_gridminor;
		ChooseColour m_gridmajor_alt;
		ChooseColour m_gridminor_alt;
		ChooseColour m_gridtext;
		ChooseColour m_gridblock;
		ChooseColour m_cameraback;
		ChooseColour m_brush;
		ChooseColour m_selectedbrush;
		ChooseColour m_selectedbrush3d;
		ChooseColour m_clipper;
		ChooseColour m_viewname;

		ColoursMenu () :
			m_textureback(TextureBrowserColourGetCaller(), TextureBrowserColourSetCaller()), m_xyback(ColourGetCaller(
					g_xywindow_globals.color_gridback), ColourSetCaller(g_xywindow_globals.color_gridback)),
					m_gridmajor(ColourGetCaller(g_xywindow_globals.color_gridmajor), ColourSetCaller(
							g_xywindow_globals.color_gridmajor)), m_gridminor(ColourGetCaller(
							g_xywindow_globals.color_gridminor), ColourSetCaller(g_xywindow_globals.color_gridminor)),
					m_gridmajor_alt(ColourGetCaller(g_xywindow_globals.color_gridmajor_alt), ColourSetCaller(
							g_xywindow_globals.color_gridmajor_alt)), m_gridminor_alt(ColourGetCaller(
							g_xywindow_globals.color_gridminor_alt), ColourSetCaller(
							g_xywindow_globals.color_gridminor_alt)), m_gridtext(ColourGetCaller(
							g_xywindow_globals.color_gridtext), ColourSetCaller(g_xywindow_globals.color_gridtext)),
					m_gridblock(ColourGetCaller(g_xywindow_globals.color_gridblock), ColourSetCaller(
							g_xywindow_globals.color_gridblock)), m_cameraback(ColourGetCaller(
							g_camwindow_globals.color_cameraback),
							ColourSetCaller(g_camwindow_globals.color_cameraback)), m_brush(ColourGetCaller(
							g_xywindow_globals.color_brushes), BrushColourSetCaller()), m_selectedbrush(
							ColourGetCaller(g_xywindow_globals.color_selbrushes), ColourSetCaller(
									g_xywindow_globals.color_selbrushes)), m_selectedbrush3d(ColourGetCaller(
							g_camwindow_globals.color_selbrushes3d), ColourSetCaller(
							g_camwindow_globals.color_selbrushes3d)), m_clipper(ColourGetCaller(
							g_xywindow_globals.color_clipper), ClipperColourSetCaller()), m_viewname(ColourGetCaller(
							g_xywindow_globals.color_viewname), ColourSetCaller(g_xywindow_globals.color_viewname))
		{
		}
};

GtkMenuItem* create_colours_menu (void)
{
	GtkMenuItem* colours_menu_item = new_sub_menu_item_with_mnemonic(_("Colors"));
	GtkMenu* menu_in_menu = GTK_MENU(gtk_menu_item_get_submenu(colours_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu_in_menu);

	GtkMenu* menu_3 = create_sub_menu_with_mnemonic(menu_in_menu, _("Themes"));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu_3);

	create_menu_item_with_mnemonic(menu_3, _("QE4 Original"), "ColorSchemeOriginal");
	create_menu_item_with_mnemonic(menu_3, _("Q3Radiant Original"), "ColorSchemeQER");
	create_menu_item_with_mnemonic(menu_3, _("Black and Green"), "ColorSchemeBlackAndGreen");
	create_menu_item_with_mnemonic(menu_3, _("Maya/Max/Lightwave Emulation"), "ColorSchemeYdnar");

	menu_separator(menu_in_menu);

	create_menu_item_with_mnemonic(menu_in_menu, _("_Texture Background..."), "ChooseTextureBackgroundColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Grid Background..."), "ChooseGridBackgroundColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Grid Major..."), "ChooseGridMajorColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Grid Minor..."), "ChooseGridMinorColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Grid Major Small..."), "ChooseSmallGridMajorColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Grid Minor Small..."), "ChooseSmallGridMinorColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Grid Text..."), "ChooseGridTextColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Grid Block..."), "ChooseGridBlockColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Default Brush..."), "ChooseBrushColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Camera Background..."), "ChooseCameraBackgroundColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Selected Brush..."), "ChooseSelectedBrushColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Selected Brush (Camera)..."), "ChooseCameraSelectedBrushColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Clipper..."), "ChooseClipperColor");
	create_menu_item_with_mnemonic(menu_in_menu, _("Active View name..."), "ChooseOrthoViewNameColor");

	return colours_menu_item;
}

static ColoursMenu g_ColoursMenu;

void ColorScheme_Original (void)
{
	GlobalTextureBrowser().setBackgroundColour(Vector3(0.25f, 0.25f, 0.25f));

	g_camwindow_globals.color_selbrushes3d = Vector3(1.0f, 0.0f, 0.0f);
	g_camwindow_globals.color_cameraback = Vector3(0.25f, 0.25f, 0.25f);
	CamWnd_Update(*g_pParentWnd->GetCamWnd());

	g_xywindow_globals.color_gridback = Vector3(1.0f, 1.0f, 1.0f);
	g_xywindow_globals.color_gridminor = Vector3(0.75f, 0.75f, 0.75f);
	g_xywindow_globals.color_gridmajor = Vector3(0.5f, 0.5f, 0.5f);
	g_xywindow_globals.color_gridminor_alt = Vector3(0.5f, 0.0f, 0.0f);
	g_xywindow_globals.color_gridmajor_alt = Vector3(1.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_gridblock = Vector3(0.0f, 0.0f, 1.0f);
	g_xywindow_globals.color_gridtext = Vector3(0.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_selbrushes = Vector3(1.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_clipper = Vector3(0.0f, 0.0f, 1.0f);
	g_xywindow_globals.color_brushes = Vector3(0.0f, 0.0f, 0.0f);
	SetWorldspawnColour(g_xywindow_globals.color_brushes);
	g_xywindow_globals.color_viewname = Vector3(0.5f, 0.0f, 0.75f);
	XY_UpdateAllWindows();
}

void ColorScheme_QER (void)
{
	GlobalTextureBrowser().setBackgroundColour(Vector3(0.25f, 0.25f, 0.25f));

	g_camwindow_globals.color_cameraback = Vector3(0.25f, 0.25f, 0.25f);
	g_camwindow_globals.color_selbrushes3d = Vector3(1.0f, 0.0f, 0.0f);
	CamWnd_Update(*g_pParentWnd->GetCamWnd());

	g_xywindow_globals.color_gridback = Vector3(1.0f, 1.0f, 1.0f);
	g_xywindow_globals.color_gridminor = Vector3(1.0f, 1.0f, 1.0f);
	g_xywindow_globals.color_gridmajor = Vector3(0.5f, 0.5f, 0.5f);
	g_xywindow_globals.color_gridblock = Vector3(0.0f, 0.0f, 1.0f);
	g_xywindow_globals.color_gridtext = Vector3(0.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_selbrushes = Vector3(1.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_clipper = Vector3(0.0f, 0.0f, 1.0f);
	g_xywindow_globals.color_brushes = Vector3(0.0f, 0.0f, 0.0f);
	SetWorldspawnColour(g_xywindow_globals.color_brushes);
	g_xywindow_globals.color_viewname = Vector3(0.5f, 0.0f, 0.75f);
	XY_UpdateAllWindows();
}

void ColorScheme_Black (void)
{
	GlobalTextureBrowser().setBackgroundColour(Vector3(0.25f, 0.25f, 0.25f));

	g_camwindow_globals.color_cameraback = Vector3(0.25f, 0.25f, 0.25f);
	g_camwindow_globals.color_selbrushes3d = Vector3(1.0f, 0.0f, 0.0f);
	CamWnd_Update(*g_pParentWnd->GetCamWnd());

	g_xywindow_globals.color_gridback = Vector3(0.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_gridminor = Vector3(0.2f, 0.2f, 0.2f);
	g_xywindow_globals.color_gridmajor = Vector3(0.3f, 0.5f, 0.5f);
	g_xywindow_globals.color_gridblock = Vector3(0.0f, 0.0f, 1.0f);
	g_xywindow_globals.color_gridtext = Vector3(1.0f, 1.0f, 1.0f);
	g_xywindow_globals.color_selbrushes = Vector3(1.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_clipper = Vector3(0.0f, 0.0f, 1.0f);
	g_xywindow_globals.color_brushes = Vector3(1.0f, 1.0f, 1.0f);
	SetWorldspawnColour(g_xywindow_globals.color_brushes);
	g_xywindow_globals.color_viewname = Vector3(0.7f, 0.7f, 0.0f);
	XY_UpdateAllWindows();
}

/**
 * @brief ydnar: to emulate maya/max/lightwave color schemes
 */
void ColorScheme_Ydnar (void)
{
	GlobalTextureBrowser().setBackgroundColour(Vector3(0.25f, 0.25f, 0.25f));

	g_camwindow_globals.color_cameraback = Vector3(0.25f, 0.25f, 0.25f);
	g_camwindow_globals.color_selbrushes3d = Vector3(1.0f, 0.0f, 0.0f);
	CamWnd_Update(*g_pParentWnd->GetCamWnd());

	g_xywindow_globals.color_gridback = Vector3(0.77f, 0.77f, 0.77f);
	g_xywindow_globals.color_gridminor = Vector3(0.83f, 0.83f, 0.83f);
	g_xywindow_globals.color_gridmajor = Vector3(0.89f, 0.89f, 0.89f);
	g_xywindow_globals.color_gridblock = Vector3(1.0f, 1.0f, 1.0f);
	g_xywindow_globals.color_gridtext = Vector3(0.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_brushes = Vector3(0.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_selbrushes = Vector3(1.0f, 0.0f, 0.0f);
	g_xywindow_globals.color_clipper = Vector3(0.0f, 0.0f, 1.0f);
	SetWorldspawnColour(g_xywindow_globals.color_brushes);
	g_xywindow_globals.color_viewname = Vector3(0.5f, 0.0f, 0.75f);
	XY_UpdateAllWindows();
}

void ColorScheme_registerCommands (void)
{
	GlobalCommands_insert("ColorSchemeOriginal", FreeCaller<ColorScheme_Original> ());
	GlobalCommands_insert("ColorSchemeQER", FreeCaller<ColorScheme_QER> ());
	GlobalCommands_insert("ColorSchemeBlackAndGreen", FreeCaller<ColorScheme_Black> ());
	GlobalCommands_insert("ColorSchemeYdnar", FreeCaller<ColorScheme_Ydnar> ());
	GlobalCommands_insert("ChooseTextureBackgroundColor", makeCallback(g_ColoursMenu.m_textureback));
	GlobalCommands_insert("ChooseGridBackgroundColor", makeCallback(g_ColoursMenu.m_xyback));
	GlobalCommands_insert("ChooseGridMajorColor", makeCallback(g_ColoursMenu.m_gridmajor));
	GlobalCommands_insert("ChooseGridMinorColor", makeCallback(g_ColoursMenu.m_gridminor));
	GlobalCommands_insert("ChooseSmallGridMajorColor", makeCallback(g_ColoursMenu.m_gridmajor_alt));
	GlobalCommands_insert("ChooseSmallGridMinorColor", makeCallback(g_ColoursMenu.m_gridminor_alt));
	GlobalCommands_insert("ChooseGridTextColor", makeCallback(g_ColoursMenu.m_gridtext));
	GlobalCommands_insert("ChooseGridBlockColor", makeCallback(g_ColoursMenu.m_gridblock));
	GlobalCommands_insert("ChooseBrushColor", makeCallback(g_ColoursMenu.m_brush));
	GlobalCommands_insert("ChooseCameraBackgroundColor", makeCallback(g_ColoursMenu.m_cameraback));
	GlobalCommands_insert("ChooseSelectedBrushColor", makeCallback(g_ColoursMenu.m_selectedbrush));
	GlobalCommands_insert("ChooseCameraSelectedBrushColor", makeCallback(g_ColoursMenu.m_selectedbrush3d));
	GlobalCommands_insert("ChooseClipperColor", makeCallback(g_ColoursMenu.m_clipper));
	GlobalCommands_insert("ChooseOrthoViewNameColor", makeCallback(g_ColoursMenu.m_viewname));
}

void ColorScheme_Init (void)
{
	GlobalEntityClassManager().attach(g_WorldspawnColourEntityClassObserver);
}

void ColorScheme_Destroy (void)
{
	GlobalEntityClassManager().detach(g_WorldspawnColourEntityClassObserver);
}
