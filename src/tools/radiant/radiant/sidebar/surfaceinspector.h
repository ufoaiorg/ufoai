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

void SurfaceInspector_Construct ();
void SurfaceInspector_Destroy ();

#include "gtkutil/nonmodal.h"
#include "gtkutil/idledraw.h"
#include "gtkutil/entry.h"

#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "iscenegraph.h"
#include "../brush/TexDef.h"
#include "iundo.h"
#include "iselection.h"
#include "ieventmanager.h"
#include "igrid.h"

#include "signal/isignal.h"
#include "generic/object.h"
#include "texturelib.h"
#include "shaderlib.h"
#include "stringio.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/entry.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/pointer.h"
#include "gtkutil/button.h"
#include "../map/map.h"
#include "../select.h"
#include "../brush/brushmanip.h"
#include "../brush/BrushModule.h"
#include "../settings/preferences.h"
#include "../brush/TexDef.h"
#include "../brush/TextureProjection.h"
#include "../mainframe.h"
#include "../dialog.h"
#include "../brush/brush.h"
#include "stream/stringstream.h"
#include "../textureentry.h"
#include "../textool/TexTool.h"

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

class FaceTexture
{
	public:
		TextureProjection m_projection;
		ContentsFlagsValue m_flags;
};

typedef Callback3<std::string&, TextureProjection&, ContentsFlagsValue&> GetTextureCallback;
typedef Callback3<const std::string&, const TextureProjection&, const ContentsFlagsValue&> SetTextureCallback;

struct Texturable
{
		GetTextureCallback getTexture;
		SetTextureCallback setTexture;
};

class OccludeSelector: public Selector
{
		SelectionIntersection& m_bestIntersection;
		bool& m_occluded;
	public:
		OccludeSelector (SelectionIntersection& bestIntersection, bool& occluded) :
			m_bestIntersection(bestIntersection), m_occluded(occluded)
		{
			m_occluded = false;
		}
		void pushSelectable (Selectable& selectable)
		{
		}
		void popSelectable (void)
		{
		}
		void addIntersection (const SelectionIntersection& intersection)
		{
			if (SelectionIntersection_closer(intersection, m_bestIntersection)) {
				m_bestIntersection = intersection;
				m_occluded = true;
			}
		}
};

class BrushGetClosestFaceVisibleWalker: public scene::Graph::Walker
{
		SelectionTest& m_test;
		Texturable& m_texturable;
		mutable SelectionIntersection m_bestIntersection;

		static void Face_getTexture (Face& face, std::string& shader, TextureProjection& projection,
				ContentsFlagsValue& flags)
		{
			shader = face.GetShader();
			face.GetTexdef(projection);
			flags = face.getShader().m_flags;
		}
		typedef Function4<Face&, std::string&, TextureProjection&, ContentsFlagsValue&, void, Face_getTexture>
				FaceGetTexture;

		static void Face_setTexture (Face& face, const std::string& shader, const TextureProjection& projection,
				const ContentsFlagsValue& flags)
		{
			face.SetShader(shader);
			face.SetTexdef(projection);
			face.SetFlags(flags);
		}
		typedef Function4<Face&, const std::string&, const TextureProjection&, const ContentsFlagsValue&, void,
				Face_setTexture> FaceSetTexture;

		static void Face_getClosest (Face& face, SelectionTest& test, SelectionIntersection& bestIntersection,
				Texturable& texturable)
		{
			SelectionIntersection intersection;
			face.testSelect(test, intersection);
			if (intersection.valid() && SelectionIntersection_closer(intersection, bestIntersection)) {
				bestIntersection = intersection;
				texturable.setTexture = makeCallback3(FaceSetTexture(), face);
				texturable.getTexture = makeCallback3(FaceGetTexture(), face);
			}
		}

	public:
		BrushGetClosestFaceVisibleWalker (SelectionTest& test, Texturable& texturable) :
			m_test(test), m_texturable(texturable)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				BrushInstance* brush = Instance_getBrush(instance);
				if (brush != 0) {
					m_test.BeginMesh(brush->localToWorld());

					for (Brush::const_iterator i = brush->getBrush().begin(); i != brush->getBrush().end(); ++i) {
						Face_getClosest(*(*i), m_test, m_bestIntersection, m_texturable);
					}
				} else {
					SelectionTestable* selectionTestable = Instance_getSelectionTestable(instance);
					if (selectionTestable) {
						bool occluded;
						OccludeSelector selector(m_bestIntersection, occluded);
						selectionTestable->testSelect(selector, m_test);
						if (occluded)
							m_texturable = Texturable();
					}
				}
			}
			return true;
		}
};

class SurfaceInspector: public RegistryKeyObserver, public PreferenceConstructor
{
	private:
		Vector2 _shift;
		Vector2 _scale;
		float _rotate;

		bool _shutdown;

		bool _snapTToGrid;

		FaceTexture _faceTextureClipboard;

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

	private:

		void queueDraw (void);

		void update ();
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
		void setTexdefForSelected (const TextureProjection& projection);

		const std::string& getContentFlagName (std::size_t bit) const;
		const std::string& getSurfaceFlagName (std::size_t bit) const;

		void doSnapTToGrid (float hscale, float vscale);

		Texturable getClosestTexturable (scene::Graph& graph, SelectionTest& test);
		bool getClosestTexture (scene::Graph& graph, SelectionTest& test, std::string& shader,
				TextureProjection& projection, ContentsFlagsValue& flags);
		void setClosestTexture (scene::Graph& graph, SelectionTest& test, const std::string& shader,
				const TextureProjection& projection, const ContentsFlagsValue& flags);

		static void applyClipboardTexture (FaceInstance& faceInstance);

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
	public:
		SurfaceInspector ();

		void shutdown (void);

		GtkWidget* buildNotebook ();

		void registerCommands (void);

		void copyTextureFromSelectedFaces (void);
		void pasteTextureFromSelectedFaces (void);
		void applyClosestTexture (SelectionTest& test);
		void copyClosestTexture (SelectionTest& test);

		void resetTextureClipboard ();

		void fitTexture ();
		void flipTextureX ();
		void flipTextureY ();

		float getRotate () const;
		const Vector2& getScale () const;
		const Vector2& getShift () const;

		void constructPreferencePage (PreferenceGroup& group);
		void keyChanged (const std::string& changedKey, const std::string& newValue);
};

inline SurfaceInspector& GlobalSurfaceInspector (void)
{
	static SurfaceInspector _surfaceInspector;
	return _surfaceInspector;
}

#endif
