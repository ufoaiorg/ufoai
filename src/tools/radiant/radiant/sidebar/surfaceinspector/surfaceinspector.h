/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(INCLUDED_SURFACEDIALOG_H)
#define INCLUDED_SURFACEDIALOG_H

#include "iregistry.h"
#include "preferencesystem.h"

#include <gtk/gtk.h>
#include "selectable.h"

#include "iscenegraph.h"
#include "iundo.h"
#include "iselection.h"
#include "ieventmanager.h"

#include "signal/isignal.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/entry.h"
#include "gtkutil/nonmodal.h"

#include "../../brush/FaceShader.h"
#include "../../brush/ContentsFlagsValue.h"
#include "../../brush/FaceInstance.h"
#include "../../brush/TexDef.h"
#include "../../brush/TextureProjection.h"

class Increment
{
	private:
		void spin_button_set_step (GtkSpinButton* spin, gfloat step)
		{
			GtkAdjustment* adjustment = gtk_spin_button_get_adjustment(spin);
			adjustment->step_increment = step;
			gtk_adjustment_changed(adjustment);
		}

		float& m_f;
	public:
		GtkSpinButton* m_spin;
		GtkEntry* m_entry;
		Increment (float& f) :
			m_f(f), m_spin(0), m_entry(0)
		{
		}
		void cancel (void)
		{
			entry_set_float(m_entry, m_f);
		}
		typedef MemberCaller<Increment, &Increment::cancel> CancelCaller;
		void apply (void)
		{
			m_f = static_cast<float> (entry_get_float(m_entry));
			spin_button_set_step(m_spin, m_f);
		}
		typedef MemberCaller<Increment, &Increment::apply> ApplyCaller;
};

typedef Callback3<std::string&, TextureProjection&, ContentsFlagsValue&> GetTextureCallback;
typedef Callback3<const std::string&, const TextureProjection&, const ContentsFlagsValue&> SetTextureCallback;

class SurfaceInspector: public RegistryKeyObserver, public PreferenceConstructor
{
	private:
		Vector2 _shift;
		Vector2 _scale;
		float _rotate;

		// TODO: remove me - use the radiant shutdown listener
		bool _shutdown;

		bool _snapTToGrid;

		std::string _selectedShader;
		TextureProjection _selectedTexdef;
		ContentsFlagsValue _selectedFlags;
		size_t _selectedShaderSize[2];

		NonModalEntry _textureEntry;
		NonModalSpinner _hshiftSpinner;
		NonModalEntry _hshiftEntry;
		NonModalSpinner _vshiftSpinner;
		NonModalEntry _vshiftEntry;
		NonModalSpinner _hscaleSpinner;
		NonModalEntry _hscaleEntry;
		NonModalSpinner _vscaleSpinner;
		NonModalEntry _vscaleEntry;
		NonModalSpinner _rotateSpinner;
		NonModalEntry _rotateEntry;

		IdleDraw _idleDraw;

		GtkCheckButton* _surfaceFlags[32];
		GtkFrame* _surfaceFlagsFrame;
		GtkCheckButton* _contentFlags[32];
		GtkFrame* _contentFlagsFrame;

		bool _textureSelectionDirty;

		NonModalEntry _valueEntry;
		GtkEntry* _valueEntryWidget;
		bool _valueInconsistent; // inconsistent marker for valueEntryWidget

		// Dialog Data
		float _fitHorizontal;
		float _fitVertical;

		Increment _hshiftIncrement;
		Increment _vshiftIncrement;
		Increment _hscaleIncrement;
		Increment _vscaleIncrement;
		Increment _rotateIncrement;
		// TODO: Use gtkutil::TextPanel
		GtkEntry* _texture;

	public:
		SurfaceInspector ();

		void shutdown (void);
		void update ();

		GtkWidget* buildNotebook ();

		void registerCommands (void);

		void fitTexture ();
		void flipTextureX ();
		void flipTextureY ();

		float getRotate () const;
		const Vector2& getScale () const;
		const Vector2& getShift () const;

		void constructPreferencePage (PreferenceGroup& group);
		void keyChanged (const std::string& changedKey, const std::string& newValue);

	private:

		void queueDraw (void);

		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::update> UpdateCaller;
		void applyShader ();
		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::applyShader> ApplyShaderCaller;
		void applyTexdef ();
		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::applyTexdef> ApplyTexdefCaller;
		void applyFlags ();
		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::applyFlags> ApplyFlagsCaller;

		void updateFlagButtons ();

		void gridChange ();

		void setSelectedShader (const std::string& shader);
		const std::string& getSelectedShader (void);

		void setSelectedTexdef (const TextureProjection& projection);
		const TextureProjection& getSelectedTexdef (void);

		void setSelectedFlags (const ContentsFlagsValue& flags);
		const ContentsFlagsValue& getSelectedFlags (void);

		// Fills the surface inspector with values of the current selected brush(es) or face(s)
		void setValuesFromSelected (void);

		void setFlagsForSelected (const ContentsFlagsValue& flags);

		const std::string& getContentFlagName (std::size_t bit) const;
		const std::string& getSurfaceFlagName (std::size_t bit) const;

		void doSnapTToGrid (float hscale, float vscale);

		guint togglebutton_connect_toggled (GtkToggleButton* button);

		void updateSelection (void);
		void selectionChanged (const Selectable& selectable);
		void toggleTexTool ();

		// Gtk callbacks
		static void onValueToggle (GtkWidget *widget, SurfaceInspector *inspector);
		static void onMatchGridClick (GtkWidget *widget, SurfaceInspector *inspector);
		static void onAxialClick (GtkWidget *widget, SurfaceInspector *inspector);
		static void onFaceFitClick (GtkWidget *widget, SurfaceInspector *inspector);
		static void onApplyFlagsToggle (GtkWidget *widget, SurfaceInspector *inspector);
};

inline SurfaceInspector& GlobalSurfaceInspector (void)
{
	static SurfaceInspector _surfaceInspector;
	return _surfaceInspector;
}

#endif
