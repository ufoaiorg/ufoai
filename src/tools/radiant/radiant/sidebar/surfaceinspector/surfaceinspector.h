#ifndef SURFACEINSPECTOR_H_
#define SURFACEINSPECTOR_H_

#include <map>
#include "iselection.h"
#include "iregistry.h"
#include "gtkutil/RegistryConnector.h"
#include "gtkutil/TextPanel.h"
#include "gtkutil/event/SingleIdleCallback.h"
#include "../../ui/common/ShaderChooser.h"
#include "../../brush/ContentsFlagsValue.h"
#include "../sidebar.h"

// Forward declarations to decrease compile times
typedef struct _GtkSpinButton GtkSpinButton;
typedef struct _GtkEditable GtkEditable;
typedef struct _GtkTable GtkTable;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkCheckButton GtkCheckButton;
namespace gtkutil {
class ControlButton;
}

namespace ui {

class SurfaceInspector: public RegistryKeyObserver,
		public SelectionSystem::Observer,
		public ShaderChooser::ChooserClient,
		public SidebarComponent,
		public gtkutil::SingleIdleCallback {
		// The actual dialog window
		GtkWidget* _dialogVBox;

		typedef gtkutil::ControlButton* ControlButtonPtr;

		struct ManipulatorRow
		{
				GtkWidget* hbox;
				GtkWidget* label;
				GtkWidget* value;
				gulong valueChangedHandler;
				ControlButtonPtr smaller;
				ControlButtonPtr larger;
				GtkWidget* step;
				GtkWidget* steplabel;
		};

		// This are the named manipulator rows (shift, scale, rotation, etc)
		typedef std::map<std::string, ManipulatorRow> ManipulatorMap;
		ManipulatorMap _manipulators;

		GtkCheckButton* _surfaceFlags[32];
		GtkCheckButton* _contentFlags[32];

		// The "shader" entry field
		GtkWidget* _shaderEntry;
		GtkWidget* _selectShaderButton;

		ContentsFlagsValue _selectedFlags;

		struct FitTextureWidgets
		{
				GtkWidget* hbox;
				GtkObject* widthAdj;
				GtkObject* heightAdj;
				GtkWidget* width;
				GtkWidget* height;
				GtkWidget* button;
				GtkWidget* label;
		} _fitTexture;

		struct FlipTextureWidgets
		{
				GtkWidget* hbox;
				GtkWidget* flipX;
				GtkWidget* flipY;
				GtkWidget* label;
		} _flipTexture;

		struct ApplyTextureWidgets
		{
				GtkWidget* hbox;
				GtkWidget* label;
				GtkWidget* natural;
		} _applyTex;

		GtkWidget* _defaultTexScale;
		GtkWidget* _texLockButton;

		gtkutil::TextPanel _valueEntryWidget;
		// if more than one of the selected objects has a value set, this is inconsistent and we can't know which we should take
		// So in this case we just don't change the old value until we explicitly edit the value entry field
		bool _valueInconsistent;

		// To avoid key changed loopbacks when the registry is updated
		bool _callbackActive;

		// This member takes care of importing/exporting Registry
		// key values from and to widgets
		gtkutil::RegistryConnector _connector;

		// A reference to the SelectionInfo structure (with the counters)
		const SelectionInfo& _selectionInfo;

	public:

		// Constructor
		SurfaceInspector ();

		~SurfaceInspector ();

		/** Connect and updates the widgets
		 */
		void init ();

		/** greebo: Some sort of "soft" destructor that de-registers
		 * this class from the SelectionSystem, etc.
		 */
		void shutdown ();

		/** greebo: Contains the static instance of this dialog.
		 * Constructs the instance and calls toggle() when invoked.
		 */
		static SurfaceInspector& Instance ();

		/** greebo: Gets called when the default texscale registry key changes
		 */
		void keyChanged (const std::string&, const std::string&);

		/** greebo: SelectionSystem::Observer implementation. Gets called by
		 * the SelectionSystem upon selection change to allow updating of the
		 * texture properties.
		 */
		void selectionChanged (scene::Instance& instance, bool isComponent);

		// Updates the widgets
		void queueUpdate ();

		/** greebo: Gets called upon shader selection change (during ShaderChooser display)
		 */
		void shaderSelectionChanged (const std::string& shaderName);

		GtkWidget* getWidget () const;

		const std::string getTitle() const;

		void switchPage (int pageIndex);

		// Executes the fit command for the selection
		void fitTexture ();

		// Idle callback, used for deferred updates
		void onGtkIdle();
	private:
		// Updates the widgets (this is private, use queueUpdate() instead)
		void update();

		/** greebo: Creates a row consisting of label, value entry,
		 * two arrow buttons and a step entry field.
		 *
		 * @table: the GtkTable the row should be packed into.
		 * @row: the target row number (first table row = 0).
		 *
		 * @returns: the structure containing the widget pointers.
		 */
		ManipulatorRow createManipulatorRow (const std::string& label, GtkTable* table, int row, bool vertical);

		const std::string& getContentFlagName (std::size_t bit) const;

		const std::string& getSurfaceFlagName (std::size_t bit) const;

		// Adds all the widgets to the window
		void populateWindow ();

		// Connect IEvents to the widgets
		void connectEvents ();

		// Updates the content- and surfaceflags
		void updateFlags ();

		// Updates the texture shift/scale/rotation fields
		void updateTexDef ();

		// The counter-part of updateTexDef() - emits the TexCoords to the selection
		void emitTexDef ();

		// Applies the entered shader to the current selection
		void emitShader ();

		void updateFlagButtons ();

		void setFlagsForSelected (const ContentsFlagsValue& flags);

		// Saves the connected widget content into the registry
		void saveToRegistry ();

		// The callback when the "select shader" button is pressed, opens the ShaderChooser dialog
		static void onShaderSelect (GtkWidget* button, SurfaceInspector* self);

		// The callback for the delete event (toggles the visibility)
		static gboolean onDelete (GtkWidget* widget, GdkEvent* event, SurfaceInspector* self);

		// Gets called when the step entry fields get changed
		static void onStepChanged (GtkEditable* editable, SurfaceInspector* self);

		// Gets called when the value entry field is changed (shift/scale/rotation) - emits the texcoords
		static gboolean onDefaultScaleChanged (GtkSpinButton* spinbutton, SurfaceInspector* self);

		// The callback for the Fit Texture button
		static gboolean onFit (GtkWidget* widget, SurfaceInspector* self);
		static gboolean doUpdate (GtkWidget* widget, SurfaceInspector* self);

		// the callback for the surface flag value
		static void onValueToggle (GtkWidget *widget, SurfaceInspector *self);

		// the callback for the flags toggle
		static void onApplyFlagsToggle (GtkWidget *activatedWidget, SurfaceInspector *self);

		// The keypress handler for catching the Enter key when in the shader entry field
		static gboolean onKeyPress (GtkWidget* entry, GdkEventKey* event, SurfaceInspector* self);

		// The keypress handler for catching the Enter key when in the value entry fields
		static gboolean onValueKeyPress (GtkWidget* entry, GdkEventKey* event, SurfaceInspector* self);

		// the callback for changing the surface property value like e.g. the light intensity for surface lights
		static gboolean onValueEntryKeyPress (GtkWindow* window, GdkEventKey* event, SurfaceInspector* self);

		// the callback that changes the value when we focus-out the entry field
		static gboolean onValueEntryFocusOut (GtkWidget* widget, GdkEventKey* event, SurfaceInspector* self);
}; // class SurfaceInspector

} // namespace ui

#endif /*SURFACEINSPECTOR_H_*/
