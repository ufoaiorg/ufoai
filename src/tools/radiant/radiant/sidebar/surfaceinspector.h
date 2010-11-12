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

#include <gtk/gtk.h>
#include "selectable.h"

void SurfaceInspector_Construct ();
void SurfaceInspector_Destroy ();
void SurfaceInspector_FitTexture (void);

void FlipTextureX ();
void FlipTextureY ();

void SelectedFaces_copyTexture ();
void SelectedFaces_pasteTexture ();

void Scene_applyClosestTexture (SelectionTest& test);
void Scene_copyClosestTexture (SelectionTest& test);

#include "gtkutil/nonmodal.h"
#include "gtkutil/idledraw.h"
#include "gtkutil/entry.h"

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

class SurfaceInspector
{
		NonModalEntry m_textureEntry;
		NonModalSpinner m_hshiftSpinner;
		NonModalEntry m_hshiftEntry;
		NonModalSpinner m_vshiftSpinner;
		NonModalEntry m_vshiftEntry;
		NonModalSpinner m_hscaleSpinner;
		NonModalEntry m_hscaleEntry;
		NonModalSpinner m_vscaleSpinner;
		NonModalEntry m_vscaleEntry;
		NonModalSpinner m_rotateSpinner;
		NonModalEntry m_rotateEntry;

		IdleDraw m_idleDraw;

		GtkCheckButton* m_surfaceFlags[32];
		GtkFrame* m_surfaceFlagsFrame;
		GtkCheckButton* m_contentFlags[32];
		GtkFrame* m_contentFlagsFrame;

		NonModalEntry m_valueEntry;
		GtkEntry* m_valueEntryWidget;
		bool m_valueInconsistent; // inconsistent marker for valueEntryWidget

		void UpdateFlagButtons ();
		static void UpdateValueStatus (GtkWidget *widget, SurfaceInspector *inspector);

		void gridChange ();

	public:
		GtkWidget* BuildNotebook ();

		// Dialog Data
		float m_fitHorizontal;
		float m_fitVertical;

		Increment m_hshiftIncrement;
		Increment m_vshiftIncrement;
		Increment m_hscaleIncrement;
		Increment m_vscaleIncrement;
		Increment m_rotateIncrement;
		// TODO: Use gtkutil::TextPanel
		GtkEntry* m_texture;

		SurfaceInspector ();
		void queueDraw (void);

		void Update ();
		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::Update> UpdateCaller;
		void ApplyShader ();
		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::ApplyShader> ApplyShaderCaller;
		void ApplyTexdef ();
		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::ApplyTexdef> ApplyTexdefCaller;
		void ApplyFlags ();
		typedef MemberCaller<SurfaceInspector, &SurfaceInspector::ApplyFlags> ApplyFlagsCaller;

		static void ApplyFlagsWithIndicator (GtkWidget *widget, SurfaceInspector *inspector);
};

// the increment we are using for the surface inspector (this is saved in the prefs)
struct si_globals_t
{
		float shift[2];
		float scale[2];
		float rotate;

		bool m_bSnapTToGrid;

		si_globals_t () :
			m_bSnapTToGrid(false)
		{
			shift[0] = 8.0f;
			shift[1] = 8.0f;
			scale[0] = 0.5f;
			scale[1] = 0.5f;
			rotate = 45.0f;
		}
};
extern si_globals_t g_si_globals;

GtkWidget *SurfaceInspector_constructNotebookTab (void);

#endif
