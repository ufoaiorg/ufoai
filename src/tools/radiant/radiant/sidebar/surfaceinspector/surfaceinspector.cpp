#include "surfaceinspector.h"

#include "radiant_i18n.h"
#include "igamemanager.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "ieventmanager.h"

#include "gtkutil/window/TransientWindow.h"
#include "gtkutil/IconTextButton.h"
#include "gtkutil/ControlButton.h"
#include "gtkutil/LeftAlignedLabel.h"
#include "gtkutil/LeftAlignment.h"
#include "gtkutil/window/DialogWindow.h"
#include "gtkutil/dialog.h"
#include "gtkutil/image.h"

#include "selectionlib.h"
#include "math/FloatTools.h"

#include "../../textool/TexTool.h"
#include "../../brush/TextureProjection.h"
#include "../../brush/brushmanip.h"
#include "../../selection/algorithm/Primitives.h"
#include "../../selection/algorithm/Shader.h"
#include "../../ui/mainframe/mainframe.h"
#include "../../textureentry.h"
#include "../../select.h"

namespace ui {

namespace {
const std::string WINDOW_TITLE = _("Surface Inspector");
const std::string LABEL_PROPERTIES = _("Texture Properties");
const std::string LABEL_OPERATIONS = _("Texture Operations");
const std::string LABEL_SURFACEFLAGS = _("Surfaceflags");
const std::string LABEL_CONTENTFLAGS = _("Contentflags");

const std::string HSHIFT = "horizshift";
const std::string VSHIFT = "vertshift";
const std::string HSCALE = "horizscale";
const std::string VSCALE = "vertscale";
const std::string ROTATION = "rotation";

const std::string LABEL_HSHIFT = _("Horiz. Shift:");
const std::string LABEL_VSHIFT = _("Vert. Shift:");
const std::string LABEL_HSCALE = _("Horiz. Scale:");
const std::string LABEL_VSCALE = _("Vert. Scale:");
const std::string LABEL_ROTATION = _("Rotation:");
const char* LABEL_SHADER = _("Shader:");
const char* FOLDER_ICON = "folder16.png";
const char* LABEL_STEP = _("Step:");

const char* LABEL_FIT_TEXTURE = _("Fit Texture:");
const char* LABEL_FIT = _("Fit");

const char* LABEL_FLIP_TEXTURE = _("Flip Texture:");
const char* LABEL_FLIPX = _("Flip Horizontal");
const char* LABEL_FLIPY = _("Flip Vertical");

const char* LABEL_APPLY_TEXTURE = _("Modify Texture:");
const char* LABEL_NATURAL = _("Natural");

const char* LABEL_DEFAULT_SCALE = _("Default Scale:");
const char* LABEL_TEXTURE_LOCK = _("Texture Lock");

const std::string RKEY_TEXTURES_SKIPCOMMON = "user/ui/textures/skipCommon";
const std::string RKEY_ENABLE_TEXTURE_LOCK = "user/ui/brush/textureLock";
const std::string RKEY_DEFAULT_TEXTURE_SCALE = "user/ui/textures/defaultTextureScale";

const std::string RKEY_ROOT = "user/ui/textures/surfaceInspector/";
const std::string RKEY_HSHIFT_STEP = RKEY_ROOT + "hShiftStep";
const std::string RKEY_VSHIFT_STEP = RKEY_ROOT + "vShiftStep";
const std::string RKEY_HSCALE_STEP = RKEY_ROOT + "hScaleStep";
const std::string RKEY_VSCALE_STEP = RKEY_ROOT + "vScaleStep";
const std::string RKEY_ROTATION_STEP = RKEY_ROOT + "rotStep";

const std::string RKEY_WINDOW_STATE = RKEY_ROOT + "window";

const double MAX_FLOAT_RESOLUTION = 1.0E-5;

const char* contentflagNamesDefault[32] = { "cont1", "cont2", "cont3", "cont4", "cont5", "cont6", "cont7", "cont8",
		"cont9", "cont10", "cont11", "cont12", "cont13", "cont14", "cont15", "cont16", "cont17", "cont18", "cont19",
		"cont20", "cont21", "cont22", "cont23", "cont24", "cont25", "cont26", "cont27", "cont28", "cont29", "cont30",
		"cont31", "cont32" };

const char* surfaceflagNamesDefault[32] = { "surf1", "surf2", "surf3", "surf4", "surf5", "surf6", "surf7", "surf8",
		"surf9", "surf10", "surf11", "surf12", "surf13", "surf14", "surf15", "surf16", "surf17", "surf18", "surf19",
		"surf20", "surf21", "surf22", "surf23", "surf24", "surf25", "surf26", "surf27", "surf28", "surf29", "surf30",
		"surf31", "surf32" };
}

SurfaceInspector::SurfaceInspector () :
	_callbackActive(false), _selectionInfo(GlobalSelectionSystem().getSelectionInfo())
{
	// Create all the widgets and pack them into the window
	populateWindow();

	// Connect the defaultTexScale and texLockButton widgets to "their" registry keys
	_connector.connectGtkObject(GTK_OBJECT(_defaultTexScale), RKEY_DEFAULT_TEXTURE_SCALE);
	_connector.connectGtkObject(GTK_OBJECT(_texLockButton), RKEY_ENABLE_TEXTURE_LOCK);

	// Connect the step values to the according registry values
	_connector.connectGtkObject(GTK_OBJECT(_manipulators[HSHIFT].step), RKEY_HSHIFT_STEP);
	_connector.connectGtkObject(GTK_OBJECT(_manipulators[VSHIFT].step), RKEY_VSHIFT_STEP);
	_connector.connectGtkObject(GTK_OBJECT(_manipulators[HSCALE].step), RKEY_HSCALE_STEP);
	_connector.connectGtkObject(GTK_OBJECT(_manipulators[VSCALE].step), RKEY_VSCALE_STEP);
	_connector.connectGtkObject(GTK_OBJECT(_manipulators[ROTATION].step), RKEY_ROTATION_STEP);

	// Load the values from the Registry
	_connector.importValues();

	// Be notified upon key changes
	GlobalRegistry().addKeyObserver(this, RKEY_ENABLE_TEXTURE_LOCK);
	GlobalRegistry().addKeyObserver(this, RKEY_DEFAULT_TEXTURE_SCALE);

	GlobalEventManager().addCommand("FitTexture", MemberCaller<SurfaceInspector, &SurfaceInspector::fitTexture> (*this));
	GlobalEventManager().addCommand("CopyShader", FreeCaller<selection::algorithm::pickShaderFromSelection> ());
	GlobalEventManager().addCommand("PasteShader", FreeCaller<selection::algorithm::pasteShaderToSelection> ());
	GlobalEventManager().addCommand("FlipTextureX", FreeCaller<selection::algorithm::flipTextureS> ());
	GlobalEventManager().addCommand("FlipTextureY", FreeCaller<selection::algorithm::flipTextureT> ());
}

SurfaceInspector::~SurfaceInspector ()
{
	delete _manipulators[HSHIFT].larger;
	delete _manipulators[HSHIFT].smaller;
	delete _manipulators[VSHIFT].larger;
	delete _manipulators[VSHIFT].smaller;
	delete _manipulators[HSCALE].larger;
	delete _manipulators[HSCALE].smaller;
	delete _manipulators[VSCALE].larger;
	delete _manipulators[VSCALE].smaller;
	delete _manipulators[ROTATION].larger;
	delete _manipulators[ROTATION].smaller;
}

void SurfaceInspector::init ()
{
	// Update the widget status
	update();

	// Get the relevant Events from the Manager and connect the widgets
	connectEvents();
}

void SurfaceInspector::shutdown ()
{
	// Delete all the current window states from the registry
	GlobalRegistry().deleteXPath(RKEY_WINDOW_STATE);

	// Create a new node
	xml::Node node(GlobalRegistry().createKey(RKEY_WINDOW_STATE));

	GlobalSelectionSystem().removeObserver(this);
}

void SurfaceInspector::connectEvents ()
{
	// Connect the ToggleTexLock item to the according command
	GlobalEventManager().findEvent("TogTexLock")->connectWidget(_texLockButton);
	GlobalEventManager().findEvent("FlipTextureX")->connectWidget(_flipTexture.flipX);
	GlobalEventManager().findEvent("FlipTextureY")->connectWidget(_flipTexture.flipY);
	GlobalEventManager().findEvent("TextureNatural")->connectWidget(_applyTex.natural);

	GlobalEventManager().findEvent("TexShiftLeft")->connectWidget(*_manipulators[HSHIFT].smaller);
	GlobalEventManager().findEvent("TexShiftRight")->connectWidget(*_manipulators[HSHIFT].larger);
	GlobalEventManager().findEvent("TexShiftUp")->connectWidget(*_manipulators[VSHIFT].larger);
	GlobalEventManager().findEvent("TexShiftDown")->connectWidget(*_manipulators[VSHIFT].smaller);
	GlobalEventManager().findEvent("TexScaleLeft")->connectWidget(*_manipulators[HSCALE].smaller);
	GlobalEventManager().findEvent("TexScaleRight")->connectWidget(*_manipulators[HSCALE].larger);
	GlobalEventManager().findEvent("TexScaleUp")->connectWidget(*_manipulators[VSCALE].larger);
	GlobalEventManager().findEvent("TexScaleDown")->connectWidget(*_manipulators[VSCALE].smaller);
	GlobalEventManager().findEvent("TexRotateClock")->connectWidget(*_manipulators[ROTATION].larger);
	GlobalEventManager().findEvent("TexRotateCounter")->connectWidget(*_manipulators[ROTATION].smaller);

	// Be sure to connect these signals after the buttons are connected
	// to the events, so that the update() call gets invoked after the actual event has been fired.
	g_signal_connect(G_OBJECT(_fitTexture.button), "clicked", G_CALLBACK(onFit), this);
	g_signal_connect(G_OBJECT(_flipTexture.flipX), "clicked", G_CALLBACK(doUpdate), this);
	g_signal_connect(G_OBJECT(_flipTexture.flipY), "clicked", G_CALLBACK(doUpdate), this);
	g_signal_connect(G_OBJECT(_applyTex.natural), "clicked", G_CALLBACK(doUpdate), this);
	g_signal_connect(G_OBJECT(_defaultTexScale), "value-changed", G_CALLBACK(onDefaultScaleChanged), this);

	for (ManipulatorMap::iterator i = _manipulators.begin(); i != _manipulators.end(); ++i) {
		GtkWidget* smaller = *(i->second.smaller);
		GtkWidget* larger = *(i->second.larger);

		g_signal_connect(G_OBJECT(smaller), "clicked", G_CALLBACK(doUpdate), this);
		g_signal_connect(G_OBJECT(larger), "clicked", G_CALLBACK(doUpdate), this);
	}
}

void SurfaceInspector::keyChanged (const std::string&, const std::string&)
{
	// Avoid callback loops
	if (_callbackActive) {
		return;
	}

	_callbackActive = true;

	// Disable this event to prevent double-firing
	GlobalEventManager().findEvent("TogTexLock")->setEnabled(false);

	// Tell the registryconnector to import the values from the Registry
	_connector.importValues();

	// Re-enable the event
	GlobalEventManager().findEvent("TogTexLock")->setEnabled(true);

	_callbackActive = false;
}

const std::string& SurfaceInspector::getSurfaceFlagName (std::size_t bit) const
{
	const std::string& value = GlobalGameManager().getKeyValue(surfaceflagNamesDefault[bit]);
	return value;
}

const std::string& SurfaceInspector::getContentFlagName (std::size_t bit) const
{
	const std::string& value = GlobalGameManager().getKeyValue(contentflagNamesDefault[bit]);
	return value;
}

void SurfaceInspector::populateWindow ()
{
	// Create the overall vbox
	_dialogVBox = gtk_vbox_new(false, 6);

	// Create the title label (bold font)
	GtkWidget* topLabel = gtkutil::LeftAlignedLabel(std::string("<span weight=\"bold\">") + LABEL_PROPERTIES
			+ "</span>");
	gtk_box_pack_start(GTK_BOX(_dialogVBox), topLabel, true, true, 0);

	// Setup the table with default spacings
	GtkTable* table = GTK_TABLE(gtk_table_new(6, 2, false));
	gtk_table_set_col_spacings(table, 12);
	gtk_table_set_row_spacings(table, 6);

	// Pack it into an alignment so that it is indented
	GtkWidget* alignment = gtkutil::LeftAlignment(GTK_WIDGET(table), 18, 1.0);
	gtk_box_pack_start(GTK_BOX(_dialogVBox), GTK_WIDGET(alignment), true, true, 0);

	// Create the entry field and pack it into the first table row
	GtkWidget* shaderLabel = gtkutil::LeftAlignedLabel(LABEL_SHADER);
	gtk_table_attach_defaults(table, shaderLabel, 0, 1, 0, 1);

	_shaderEntry = gtk_entry_new();
	g_signal_connect(G_OBJECT(_shaderEntry), "key-press-event", G_CALLBACK(onKeyPress), this);
	GlobalTextureEntryCompletion::instance().connect(GTK_ENTRY(_shaderEntry));

	// Create the icon button to open the ShaderChooser
	_selectShaderButton = gtkutil::IconTextButton("", gtkutil::getLocalPixbuf(FOLDER_ICON), false);
	// Override the size request
	gtk_widget_set_size_request(_selectShaderButton, -1, -1);
	g_signal_connect(G_OBJECT(_selectShaderButton), "clicked", G_CALLBACK(onShaderSelect), this);

	GtkWidget* hbox = gtk_hbox_new(false, 0);
	gtk_box_pack_start(GTK_BOX(hbox), _shaderEntry, true, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), _selectShaderButton, false, false, 0);

	gtk_table_attach_defaults(table, hbox, 1, 2, 0, 1);

	// Populate the table with the according widgets
	_manipulators[HSHIFT] = createManipulatorRow(LABEL_HSHIFT, table, 1, false);
	_manipulators[VSHIFT] = createManipulatorRow(LABEL_VSHIFT, table, 2, true);
	_manipulators[HSCALE] = createManipulatorRow(LABEL_HSCALE, table, 3, false);
	_manipulators[VSCALE] = createManipulatorRow(LABEL_VSCALE, table, 4, true);
	_manipulators[ROTATION] = createManipulatorRow(LABEL_ROTATION, table, 5, false);

	// ======================== Texture Operations ====================================

	// Create the texture operations label (bold font)
	GtkWidget* operLabel = gtkutil::LeftAlignedLabel(std::string("<span weight=\"bold\">") + LABEL_OPERATIONS
			+ "</span>");
	gtk_misc_set_padding(GTK_MISC(operLabel), 0, 2); // Small spacing to the top/bottom
	gtk_box_pack_start(GTK_BOX(_dialogVBox), operLabel, true, true, 0);

	// Setup the table with default spacings
	GtkTable* operTable = GTK_TABLE(gtk_table_new(4, 2, false));
	gtk_table_set_col_spacings(operTable, 12);
	gtk_table_set_row_spacings(operTable, 6);

	// Pack this into another alignment
	GtkWidget* operAlignment = gtkutil::LeftAlignment(GTK_WIDGET(operTable), 18, 1.0);

	// Pack the table into the dialog
	gtk_box_pack_start(GTK_BOX(_dialogVBox), GTK_WIDGET(operAlignment), true, true, 0);

	// ------------------------ Fit Texture -----------------------------------

	_fitTexture.hbox = gtk_hbox_new(false, 6);

	// Create the "Fit Texture" label
	_fitTexture.label = gtkutil::LeftAlignedLabel(LABEL_FIT_TEXTURE);
	gtk_table_attach_defaults(operTable, _fitTexture.label, 0, 1, 0, 1);

	_fitTexture.widthAdj = gtk_adjustment_new(1.0f, 0.0f, 1000.0f, 1.0f, 1.0f, 0.0f);
	_fitTexture.heightAdj = gtk_adjustment_new(1.0f, 0.0f, 1000.0f, 1.0f, 1.0f, 0.0f);

	// Create the width entry field
	_fitTexture.width = gtk_spin_button_new(GTK_ADJUSTMENT(_fitTexture.widthAdj), 1.0f, 4);
	gtk_widget_set_size_request(_fitTexture.width, 55, -1);
	gtk_box_pack_start(GTK_BOX(_fitTexture.hbox), _fitTexture.width, false, false, 0);

	// Create the "x" label
	GtkWidget* xLabel = gtk_label_new("x");
	gtk_misc_set_alignment(GTK_MISC(xLabel), 0.5f, 0.5f);
	gtk_box_pack_start(GTK_BOX(_fitTexture.hbox), xLabel, false, false, 0);

	// Create the height entry field
	_fitTexture.height = gtk_spin_button_new(GTK_ADJUSTMENT(_fitTexture.heightAdj), 1.0f, 4);
	gtk_widget_set_size_request(_fitTexture.height, 55, -1);
	gtk_box_pack_start(GTK_BOX(_fitTexture.hbox), _fitTexture.height, false, false, 0);

	_fitTexture.button = gtk_button_new_with_label(LABEL_FIT);
	gtk_widget_set_size_request(_fitTexture.button, 30, -1);
	gtk_box_pack_start(GTK_BOX(_fitTexture.hbox), _fitTexture.button, true, true, 0);

	gtk_table_attach_defaults(operTable, _fitTexture.hbox, 1, 2, 0, 1);

	// ------------------------ Operation Buttons ------------------------------

	// Create the "Flip Texture" label
	_flipTexture.label = gtkutil::LeftAlignedLabel(LABEL_FLIP_TEXTURE);
	gtk_table_attach_defaults(operTable, _flipTexture.label, 0, 1, 1, 2);

	_flipTexture.hbox = gtk_hbox_new(true, 6);
	_flipTexture.flipX = gtk_button_new_with_label(LABEL_FLIPX);
	_flipTexture.flipY = gtk_button_new_with_label(LABEL_FLIPY);
	gtk_box_pack_start(GTK_BOX(_flipTexture.hbox), _flipTexture.flipX, true, true, 0);
	gtk_box_pack_start(GTK_BOX(_flipTexture.hbox), _flipTexture.flipY, true, true, 0);

	gtk_table_attach_defaults(operTable, _flipTexture.hbox, 1, 2, 1, 2);

	// Create the "Apply Texture" label
	_applyTex.label = gtkutil::LeftAlignedLabel(LABEL_APPLY_TEXTURE);
	gtk_table_attach_defaults(operTable, _applyTex.label, 0, 1, 2, 3);

	_applyTex.hbox = gtk_hbox_new(true, 6);
	_applyTex.natural = gtk_button_new_with_label(LABEL_NATURAL);
	gtk_box_pack_start(GTK_BOX(_applyTex.hbox), _applyTex.natural, true, true, 0);

	gtk_table_attach_defaults(operTable, _applyTex.hbox, 1, 2, 2, 3);

	// Default Scale
	GtkWidget* defaultScaleLabel = gtkutil::LeftAlignedLabel(LABEL_DEFAULT_SCALE);
	gtk_table_attach_defaults(operTable, defaultScaleLabel, 0, 1, 3, 4);

	GtkWidget* hbox2 = gtk_hbox_new(true, 6);

	// Create the default texture scale spinner
	GtkObject* defaultAdj = gtk_adjustment_new(GlobalRegistry().getFloat(RKEY_DEFAULT_TEXTURE_SCALE), 0.0f, 1000.0f,
			0.1f, 0.1f, 0.0f);
	_defaultTexScale = gtk_spin_button_new(GTK_ADJUSTMENT(defaultAdj), 1.0f, 4);
	gtk_widget_set_size_request(_defaultTexScale, 55, -1);
	gtk_box_pack_start(GTK_BOX(hbox2), _defaultTexScale, true, true, 0);

	// Texture Lock Toggle
	_texLockButton = gtk_toggle_button_new_with_label(LABEL_TEXTURE_LOCK);
	gtk_box_pack_start(GTK_BOX(hbox2), _texLockButton, true, true, 0);

	gtk_table_attach_defaults(operTable, hbox2, 1, 2, 3, 4);

	// ======================== Surface Flags ====================================

	// Create the surface flags label (bold font)
	GtkWidget* surfaceFlagsLabel = gtkutil::LeftAlignedLabel(std::string("<span weight=\"bold\">") + LABEL_SURFACEFLAGS
			+ "</span>");
	gtk_misc_set_padding(GTK_MISC(surfaceFlagsLabel), 0, 2); // Small spacing to the top/bottom
	gtk_box_pack_start(GTK_BOX(_dialogVBox), surfaceFlagsLabel, true, true, 0);

	const std::string& valueEnablingFields = GlobalGameManager().getKeyValue("surfaceinspector_enable_value");

	int surfaceFlagsAmount = 0;
	for (int i = 0; i < 32; ++i) {
		const std::string& name = getSurfaceFlagName(i);
		if (!name.empty())
			surfaceFlagsAmount++;
	}

	int surfaceFlagsRows = (surfaceFlagsAmount + 3) / 4;
	int surfaceFlagsCols = 4;
	// Setup the table with default spacings
	GtkTable* surfaceFlagsTable = GTK_TABLE(gtk_table_new(surfaceFlagsRows, surfaceFlagsCols, false));
	gtk_table_set_col_spacings(surfaceFlagsTable, 0);
	gtk_table_set_row_spacings(surfaceFlagsTable, 0);

	surfaceFlagsAmount = 0;
	for (int i = 0; i < 32; ++i) {
		const std::string& name = getSurfaceFlagName(i);
		if (name.empty()) {
			_surfaceFlags[i] = NULL;
		} else {
			GtkWidget* check = gtk_check_button_new_with_label(name.c_str());
			gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(check), FALSE);
			gtk_widget_show(check);
			int c = surfaceFlagsAmount / surfaceFlagsRows;
			int r = surfaceFlagsAmount % surfaceFlagsRows;
			surfaceFlagsAmount++;
			gtk_table_attach_defaults(surfaceFlagsTable, check, c, c + 1, r, r + 1);
			_surfaceFlags[i] = GTK_CHECK_BUTTON(check);
			guint handler_id;
			if (valueEnablingFields.find(name) != std::string::npos) {
				g_object_set_data(G_OBJECT(check), "valueEnabler", gint_to_pointer(true));
				handler_id = g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(
								onValueToggle), this);
			} else {
				handler_id = g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(onApplyFlagsToggle), this);
			}
			g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(handler_id));
		}
	}

	// Pack this into another alignment
	GtkWidget* surfaceFlagsAlignment = gtkutil::LeftAlignment(GTK_WIDGET(surfaceFlagsTable), 18, 1.0);

	// Pack the table into the dialog
	gtk_box_pack_start(GTK_BOX(_dialogVBox), GTK_WIDGET(surfaceFlagsAlignment), true, true, 0);

	// Pack the value text entry into the dialog
	gtk_box_pack_start(GTK_BOX(_dialogVBox), _valueEntryWidget.getWidget(), true, true, 0);
	g_signal_connect(G_OBJECT(_valueEntryWidget.getWidget()), "key-press-event", G_CALLBACK(onValueEntryKeyPress), this);
	g_signal_connect(G_OBJECT(_valueEntryWidget.getWidget()), "focus-out-event", G_CALLBACK(onValueEntryFocusOut), this);

	// ======================== Content Flags ====================================

	// Create the content flags label (bold font)
	GtkWidget* contentFlagsLabel = gtkutil::LeftAlignedLabel(std::string("<span weight=\"bold\">") + LABEL_CONTENTFLAGS
			+ "</span>");
	gtk_misc_set_padding(GTK_MISC(contentFlagsLabel), 0, 2); // Small spacing to the top/bottom
	gtk_box_pack_start(GTK_BOX(_dialogVBox), contentFlagsLabel, true, true, 0);

	int contentFlagsAmount = 0;
	for (int i = 0; i < 32; ++i) {
		const std::string& name = getContentFlagName(i);
		if (!name.empty())
			contentFlagsAmount++;
	}

	int contentFlagsRows = (contentFlagsAmount + 3) / 4;
	int contentFlagsCols = 4;
	// Setup the table with default spacings
	GtkTable* contentFlagsTable = GTK_TABLE(gtk_table_new(contentFlagsRows, contentFlagsCols, false));
	gtk_table_set_col_spacings(contentFlagsTable, 0);
	gtk_table_set_row_spacings(contentFlagsTable, 0);

	contentFlagsAmount = 0;
	for (int i = 0; i < 32; ++i) {
		const std::string& name = getContentFlagName(i);
		if (name.empty()) {
			_contentFlags[i] = NULL;
		} else {
			GtkWidget* check = gtk_check_button_new_with_label(name.c_str());
			gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(check), FALSE);
			gtk_widget_show(check);
			int c = contentFlagsAmount / contentFlagsRows;
			int r = contentFlagsAmount % contentFlagsRows;
			contentFlagsAmount++;
			gtk_table_attach_defaults(contentFlagsTable, check, c, c + 1, r, r + 1);
			_contentFlags[i] = GTK_CHECK_BUTTON(check);
			guint handler_id = g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(onApplyFlagsToggle), this);
			g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(handler_id));
		}
	}

	// Pack this into another alignment
	GtkWidget* contentFlagsAlignment = gtkutil::LeftAlignment(GTK_WIDGET(contentFlagsTable), 18, 1.0);

	// Pack the table into the dialog
	gtk_box_pack_start(GTK_BOX(_dialogVBox), GTK_WIDGET(contentFlagsAlignment), true, true, 0);
}

SurfaceInspector::ManipulatorRow SurfaceInspector::createManipulatorRow (const std::string& label, GtkTable* table,
		int row, bool vertical)
{
	ManipulatorRow manipRow;

	manipRow.hbox = gtk_hbox_new(false, 6);

	// Create the label
	manipRow.label = gtkutil::LeftAlignedLabel(label);
	gtk_table_attach_defaults(table, manipRow.label, 0, 1, row, row + 1);

	// Create the entry field
	manipRow.value = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(manipRow.value), 7);

	manipRow.valueChangedHandler
			= g_signal_connect(G_OBJECT(manipRow.value), "key-press-event", G_CALLBACK(onValueKeyPress), this);

	gtk_box_pack_start(GTK_BOX(manipRow.hbox), manipRow.value, true, true, 0);

	if (vertical) {
		GtkWidget* vbox = gtk_vbox_new(true, 0);

		manipRow.larger = ControlButtonPtr(new gtkutil::ControlButton("arrow_up.png"));
		gtk_widget_set_size_request(*manipRow.larger, 30, 12);
		gtk_box_pack_start(GTK_BOX(vbox), *manipRow.larger, false, false, 0);

		manipRow.smaller = ControlButtonPtr(new gtkutil::ControlButton("arrow_down.png"));
		gtk_widget_set_size_request(*manipRow.smaller, 30, 12);
		gtk_box_pack_start(GTK_BOX(vbox), *manipRow.smaller, false, false, 0);

		gtk_box_pack_start(GTK_BOX(manipRow.hbox), vbox, false, false, 0);
	} else {
		GtkWidget* hbox = gtk_hbox_new(true, 0);

		manipRow.smaller = ControlButtonPtr(new gtkutil::ControlButton("arrow_left.png"));
		gtk_widget_set_size_request(*manipRow.smaller, 15, 24);
		gtk_box_pack_start(GTK_BOX(hbox), *manipRow.smaller, false, false, 0);

		manipRow.larger = ControlButtonPtr(new gtkutil::ControlButton("arrow_right.png"));
		gtk_widget_set_size_request(*manipRow.larger, 15, 24);
		gtk_box_pack_start(GTK_BOX(hbox), *manipRow.larger, false, false, 0);

		gtk_box_pack_start(GTK_BOX(manipRow.hbox), hbox, false, false, 0);
	}

	// Create the label
	manipRow.steplabel = gtkutil::LeftAlignedLabel(LABEL_STEP);
	gtk_box_pack_start(GTK_BOX(manipRow.hbox), manipRow.steplabel, false, false, 0);

	// Create the entry field
	manipRow.step = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(manipRow.step), 5);
	g_signal_connect(G_OBJECT(manipRow.step), "changed", G_CALLBACK(onStepChanged), this);

	gtk_box_pack_start(GTK_BOX(manipRow.hbox), manipRow.step, false, false, 0);

	// Pack the hbox into the table
	gtk_table_attach_defaults(table, manipRow.hbox, 1, 2, row, row + 1);

	// Return the filled structure
	return manipRow;
}

SurfaceInspector& SurfaceInspector::Instance ()
{
	// The static instance
	static SurfaceInspector _inspector;

	return _inspector;
}

void SurfaceInspector::emitShader ()
{
	std::string shaderName = gtk_entry_get_text(GTK_ENTRY(_shaderEntry));
	if (shaderName.empty())
		return;

	// Apply it to the selection
	UndoableCommand undo("textureNameSetSelected " + shaderName);
	Select_SetShader(shaderName, GlobalRegistry().getBool(RKEY_TEXTURES_SKIPCOMMON));

	// Update the TexTool instance as well
	ui::TexTool::Instance().draw();
}

void SurfaceInspector::emitTexDef ()
{
	TexDef shiftScaleRotate;

	shiftScaleRotate._shift[0] = string::toFloat(gtk_entry_get_text(GTK_ENTRY(_manipulators[HSHIFT].value)));
	shiftScaleRotate._shift[1] = string::toFloat(gtk_entry_get_text(GTK_ENTRY(_manipulators[VSHIFT].value)));
	shiftScaleRotate._scale[0] = string::toFloat(gtk_entry_get_text(GTK_ENTRY(_manipulators[HSCALE].value)));
	shiftScaleRotate._scale[1] = string::toFloat(gtk_entry_get_text(GTK_ENTRY(_manipulators[VSCALE].value)));
	shiftScaleRotate._rotate = string::toFloat(gtk_entry_get_text(GTK_ENTRY(_manipulators[ROTATION].value)));

	TextureProjection projection(shiftScaleRotate);

	// Apply it to the selection
	selection::algorithm::applyTextureProjectionToFaces(projection);

	// Update the TexTool instance as well
	ui::TexTool::Instance().draw();
}

void SurfaceInspector::updateFlags ()
{
	ContentsFlagsValue flags(0, 0, 0, false);

	if (GlobalSelectionSystem().areFacesSelected()) {
		Scene_BrushGetFlags_Component_Selected(GlobalSceneGraph(), flags);
	} else {
		Scene_BrushGetFlags_Selected(GlobalSceneGraph(), flags);
	}

	_selectedFlags = flags;
}

void SurfaceInspector::updateTexDef ()
{
	TextureProjection curProjection = selection::algorithm::getSelectedTextureProjection();

	// Calculate the "fake" texture properties (shift/scale/rotation)
	TexDef texdef = curProjection.m_texdef;
	Vector2 shaderDims = selection::algorithm::getSelectedFaceShaderSize();

	if (shaderDims != Vector2(0, 0)) {
		// normalize again to hide the ridiculously high scale values that get created when using texlock
		texdef._shift[0] = float_mod(texdef._shift[0], shaderDims[0]);
		texdef._shift[1] = float_mod(texdef._shift[1], shaderDims[1]);
	}

	// Snap the floating point variables to the max resolution to avoid things like "1.45e-14"
	texdef._shift[0] = float_snapped(static_cast<double> (texdef._shift[0]), MAX_FLOAT_RESOLUTION);
	texdef._shift[1] = float_snapped(static_cast<double> (texdef._shift[1]), MAX_FLOAT_RESOLUTION);
	texdef._scale[0] = float_snapped(static_cast<double> (texdef._scale[0]), MAX_FLOAT_RESOLUTION);
	texdef._scale[1] = float_snapped(static_cast<double> (texdef._scale[1]), MAX_FLOAT_RESOLUTION);
	texdef._rotate = float_snapped(static_cast<double> (texdef._rotate), MAX_FLOAT_RESOLUTION);

	// Load the values into the widgets
	gtk_entry_set_text(GTK_ENTRY(_manipulators[HSHIFT].value), string::toString(texdef._shift[0]).c_str());
	gtk_entry_set_text(GTK_ENTRY(_manipulators[VSHIFT].value), string::toString(texdef._shift[1]).c_str());

	gtk_entry_set_text(GTK_ENTRY(_manipulators[HSCALE].value), string::toString(texdef._scale[0]).c_str());
	gtk_entry_set_text(GTK_ENTRY(_manipulators[VSCALE].value), string::toString(texdef._scale[1]).c_str());

	gtk_entry_set_text(GTK_ENTRY(_manipulators[ROTATION].value), string::toString(texdef._rotate).c_str());
}

void SurfaceInspector::onGtkIdle ()
{
	// Perform the pending update
	update();
}

void SurfaceInspector::queueUpdate ()
{
	// Request an idle callback to perform the update when GTK is idle
	requestIdleCallback();
}

void SurfaceInspector::update ()
{
	bool valueSensitivity = (_selectionInfo.totalCount > 0);
	bool fitSensitivity = (_selectionInfo.totalCount > 0);
	bool flipSensitivity = (_selectionInfo.totalCount > 0);
	bool applySensitivity = (_selectionInfo.totalCount > 0);
	bool contentFlagSensitivity = (_selectionInfo.totalCount > 0 && !GlobalSelectionSystem().areFacesSelected());
	bool surfaceFlagSensitivity = (_selectionInfo.totalCount > 0);

	gtk_widget_set_sensitive(_manipulators[HSHIFT].value, valueSensitivity);
	gtk_widget_set_sensitive(_manipulators[VSHIFT].value, valueSensitivity);
	gtk_widget_set_sensitive(_manipulators[HSCALE].value, valueSensitivity);
	gtk_widget_set_sensitive(_manipulators[VSCALE].value, valueSensitivity);
	gtk_widget_set_sensitive(_manipulators[ROTATION].value, valueSensitivity);

	// The fit widget sensitivity
	gtk_widget_set_sensitive(_fitTexture.hbox, fitSensitivity);
	gtk_widget_set_sensitive(_fitTexture.label, fitSensitivity);

	// The flip texture widget sensitivity
	gtk_widget_set_sensitive(_flipTexture.hbox, flipSensitivity);
	gtk_widget_set_sensitive(_flipTexture.label, flipSensitivity);

	// The natural widget sensitivity
	gtk_widget_set_sensitive(_applyTex.hbox, applySensitivity);
	gtk_widget_set_sensitive(_applyTex.label, applySensitivity);

	std::string selectedShader = selection::algorithm::getShaderFromSelection();
	gtk_entry_set_text(GTK_ENTRY(_shaderEntry), selectedShader.c_str());

	for (int i = 0; i < 32; ++i) {
		if (_contentFlags[i] != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(_contentFlags[i]), contentFlagSensitivity);
		if (_surfaceFlags[i] != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(_surfaceFlags[i]), surfaceFlagSensitivity);
	}

	if (valueSensitivity) {
		updateTexDef();
	}
	updateFlags();
	updateFlagButtons();
}

// Gets notified upon selection change
void SurfaceInspector::selectionChanged (scene::Instance& instance, bool isComponent)
{
	queueUpdate();
}

void SurfaceInspector::saveToRegistry ()
{
	// Disable the keyChanged() callback during the update process
	_callbackActive = true;

	// Pass the call to the RegistryConnector
	_connector.exportValues();

	// Re-enable the callbacks
	_callbackActive = false;
}

void SurfaceInspector::fitTexture ()
{
	float repeatX = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(_fitTexture.width));
	float repeatY = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(_fitTexture.height));

	if (repeatX > 0.0 && repeatY > 0.0) {
		selection::algorithm::fitTexture(repeatX, repeatY);
	} else {
		// Invalid repeatX && repeatY values
		gtkutil::errorDialog("Both fit values must be > 0.0.");
	}
}

GtkWidget* SurfaceInspector::getWidget () const
{
	return _dialogVBox;
}

const std::string SurfaceInspector::getTitle() const
{
	return _("_Surfaces");
}

void SurfaceInspector::switchPage (int pageIndex)
{
	if (pageIndex == _pageIndex) {
		// Register self to the SelSystem to get notified upon selection changes.
		GlobalSelectionSystem().addObserver(this);
		update();
	} else
		GlobalSelectionSystem().removeObserver(this);
}

void SurfaceInspector::shaderSelectionChanged (const std::string& shaderName)
{
	emitShader();
}

gboolean SurfaceInspector::onDefaultScaleChanged (GtkSpinButton* spinbutton, SurfaceInspector* self)
{
	// Tell the class instance to save its contents into the registry
	self->saveToRegistry();
	return false;
}

void SurfaceInspector::onStepChanged (GtkEditable* editable, SurfaceInspector* self)
{
	// Tell the class instance to save its contents into the registry
	self->saveToRegistry();
}

gboolean SurfaceInspector::onFit (GtkWidget* widget, SurfaceInspector* self)
{
	// Call the according member method
	self->fitTexture();
	self->update();

	return false;
}

gboolean SurfaceInspector::doUpdate (GtkWidget* widget, SurfaceInspector* self)
{
	// Update the widgets, everything else is done by the called Event
	self->update();
	return false;
}

// The GTK keypress callback for the shift/scale/rotation entry fields
gboolean SurfaceInspector::onValueKeyPress (GtkWidget* entry, GdkEventKey* event, SurfaceInspector* self)
{
	// Check for ENTER to emit the texture definition
	if (event->keyval == GDK_Return) {
		self->emitTexDef();
		// Don't propage the keypress if the Enter could be processed
		return true;
	}

	return false;
}

// The GTK keypress callback
gboolean SurfaceInspector::onKeyPress (GtkWidget* entry, GdkEventKey* event, SurfaceInspector* self)
{
	// Check for Enter Key to emit the shader
	if (event->keyval == GDK_Return) {
		self->emitShader();
		// Don't propagate the keypress if the Enter could be processed
		return true;
	}

	return false;
}

/**
 * @brief Updates check buttons based on actual content flags
 * @param[in] flags content flags to set button states for
 */
void SurfaceInspector::updateFlagButtons ()
{
	bool enableValueField = false;

	for (GtkCheckButton** p = _surfaceFlags; p != _surfaceFlags + 32; ++p) {
		if (*p == NULL)
			continue;
		GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
		const unsigned int state = _selectedFlags.getSurfaceFlags() & (1 << (p - _surfaceFlags));
		const unsigned int stateInconistent = _selectedFlags.getSurfaceFlagsDirty() & (1 << (p - _surfaceFlags));
		gtk_toggle_button_set_inconsistent(b, stateInconistent);
		toggle_button_set_active_no_signal(b, state);
		if (state && g_object_get_data(G_OBJECT(*p), "valueEnabler") != 0) {
			enableValueField = true;
		}
	}

	if (!GlobalSelectionSystem().areFacesSelected()) {
		for (GtkCheckButton** p = _contentFlags; p != _contentFlags + 32; ++p) {
			if (*p == NULL)
				continue;
			GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
			const unsigned int state = _selectedFlags.getContentFlags() & (1 << (p - _contentFlags));
			const unsigned int stateInconistent = _selectedFlags.getContentFlagsDirty() & (1 << (p - _contentFlags));
			gtk_toggle_button_set_inconsistent(b, stateInconistent);
			toggle_button_set_active_no_signal(b, state);
		}
	}

	if (!_selectedFlags.isValueDirty()) {
		_valueEntryWidget.setValue(string::toString(_selectedFlags.getValue()));
		_valueInconsistent = false;
	} else {
		_valueEntryWidget.setValue("");
		_valueInconsistent = true;
	}
	gtk_widget_set_sensitive(_valueEntryWidget.getWidget(), enableValueField);
}

gboolean SurfaceInspector::onValueEntryFocusOut (GtkWidget* widget, GdkEventKey* event, SurfaceInspector* self)
{
	self->_valueInconsistent = false;
	onApplyFlagsToggle(widget, self);

	return false;
}

gboolean SurfaceInspector::onValueEntryKeyPress (GtkWindow* window, GdkEventKey* event, SurfaceInspector* self)
{
	// Check for ESC to deselect all items
	if (event->keyval == GDK_Return) {
		self->_valueInconsistent = false;
		onApplyFlagsToggle(self->_valueEntryWidget.getWidget(), self);
		// Don't propagate the keypress if the Enter could be processed
		return true;
	}

	return false;
}

/**
 * @brief extended callback used to update value field sensitivity based on activation state of button
 * @param widget toggle button activated
 * @param self pointer to surface inspector instance
 */
void SurfaceInspector::onValueToggle (GtkWidget *widget, SurfaceInspector *self)
{
	onApplyFlagsToggle(widget, self);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		gtk_widget_set_sensitive(self->_valueEntryWidget.getWidget(), true);
	} else {
		/* we don't know whether there are other flags that enable value field, so check them all */
		self->updateFlagButtons();
	}
}

/**
 * @todo Set contentflags for whole brush when we are in face selection mode
 */
void SurfaceInspector::setFlagsForSelected (const ContentsFlagsValue& flags)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetFlags_Selected(GlobalSceneGraph(), flags);
	}
	Scene_BrushSetFlags_Component_Selected(GlobalSceneGraph(), flags);
}

/**
 * @brief Sets the flags for all selected faces/brushes
 * @param[in] activatedWidget currently activated widget
 * @param[in] inspector SurfaceInspector which holds the buttons
 */
void SurfaceInspector::onApplyFlagsToggle (GtkWidget *activatedWidget, SurfaceInspector *self)
{
	unsigned int surfaceflags = 0;
	unsigned int surfaceflagsDirty = 0;
	for (GtkCheckButton** p = self->_surfaceFlags; p != self->_surfaceFlags + 32; ++p) {
		if (*p == NULL)
			continue;
		GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
		unsigned int bitmask = 1 << (p - self->_surfaceFlags);
		if (activatedWidget == GTK_WIDGET(b))
			gtk_toggle_button_set_inconsistent(b, FALSE);
		if (gtk_toggle_button_get_inconsistent(b)) {
			surfaceflagsDirty |= bitmask;
		}
		if (gtk_toggle_button_get_active(b)) {
			surfaceflags |= bitmask;
		}
	}

	unsigned int contentflags = 0;
	unsigned int contentflagsDirty = 0;

	// if we have faces selected, the content flags are not active, that's why we are using the
	// original content flags here
	if (self->_selectionInfo.totalCount > 0 && GlobalSelectionSystem().areFacesSelected()) {
		contentflags = self->_selectedFlags.getContentFlags();
		contentflagsDirty = self->_selectedFlags.getContentFlagsDirty();
	} else {
		for (GtkCheckButton** p = self->_contentFlags; p != self->_contentFlags + 32; ++p) {
			if (*p == NULL)
				continue;
			GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
			unsigned int bitmask = 1 << (p - self->_contentFlags);
			if (activatedWidget == GTK_WIDGET(b))
				gtk_toggle_button_set_inconsistent(b, FALSE);
			if (gtk_toggle_button_get_inconsistent(b)) {
				contentflagsDirty |= bitmask;
			}
			if (gtk_toggle_button_get_active(b)) {
				contentflags |= bitmask;
			}
		}
	}
	int value = string::toInt(self->_valueEntryWidget.getValue());

	UndoableCommand undo("flagsSetSelected");
	/* set flags to the selection */
	self->setFlagsForSelected(ContentsFlagsValue(surfaceflags, contentflags, value, true, surfaceflagsDirty,
			contentflagsDirty, self->_valueInconsistent));
}

void SurfaceInspector::onShaderSelect (GtkWidget* button, SurfaceInspector* self)
{
	// Construct the modal dialog, self-destructs on close
	new ShaderChooser(self, GlobalRadiant().getMainWindow(), self->_shaderEntry);
}

} // namespace ui
