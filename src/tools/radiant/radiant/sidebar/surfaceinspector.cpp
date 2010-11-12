/**
 * @file surfaceinspector.cpp
 * @brief Surface Dialog
 * @author Leonardo Zide (leo@lokigames.com)
 */

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

#include "surfaceinspector.h"
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

namespace {
const std::string RKEY_SNAP_TO_GRID = "user/ui/surfaceinspector/snapToGrid";

const char* surfaceflagNamesDefault[32] = { "surf1", "surf2", "surf3", "surf4", "surf5", "surf6", "surf7", "surf8",
		"surf9", "surf10", "surf11", "surf12", "surf13", "surf14", "surf15", "surf16", "surf17", "surf18", "surf19",
		"surf20", "surf21", "surf22", "surf23", "surf24", "surf25", "surf26", "surf27", "surf28", "surf29", "surf30",
		"surf31", "surf32" };

const char* contentflagNamesDefault[32] = { "cont1", "cont2", "cont3", "cont4", "cont5", "cont6", "cont7", "cont8",
		"cont9", "cont10", "cont11", "cont12", "cont13", "cont14", "cont15", "cont16", "cont17", "cont18", "cont19",
		"cont20", "cont21", "cont22", "cont23", "cont24", "cont25", "cont26", "cont27", "cont28", "cont29", "cont30",
		"cont31", "cont32" };
}

/**
 * @sa SurfaceInspector::GetSelectedShader
 */
void SurfaceInspector::SetSelectedShader (const std::string& shader)
{
	g_selectedShader = shader;
	queueDraw();
}

/**
 * @sa SurfaceInspector::GetSelectedTexdef
 */
void SurfaceInspector::SetSelectedTexdef (const TextureProjection& projection)
{
	g_selectedTexdef = projection;
	queueDraw();
}

/**
 * @sa SurfaceInspector::GetSelectedFlags
 */
void SurfaceInspector::SetSelectedFlags (const ContentsFlagsValue& flags)
{
	g_selectedFlags = flags;
	queueDraw();
}

void SurfaceInspector::updateSelection (void)
{
	s_texture_selection_dirty = true;
	queueDraw();
}

void SurfaceInspector::SelectionChanged (const Selectable& selectable)
{
	SurfaceInspector::updateSelection();
}

/**
 * @sa Scene_BrushGetFlags_Component_Selected
 * @brief Fills the surface inspector with values of the current selected brush(es) or face(s)
 * @sa Scene_BrushGetFlags_Component_Selected
 * @sa Scene_BrushGetFlags_Selected
 */
void SurfaceInspector::SetCurrent_FromSelected (void)
{
	if (s_texture_selection_dirty == true) {
		s_texture_selection_dirty = false;
		if (GlobalSelectionSystem().areFacesSelected()) {
			TextureProjection projection;
			Scene_BrushGetTexdef_Component_Selected(GlobalSceneGraph(), projection);

			SetSelectedTexdef(projection);

			Scene_BrushGetShaderSize_Component_Selected(GlobalSceneGraph(), g_selectedShaderSize[0],
					g_selectedShaderSize[1]);

			std::string name;
			Scene_BrushGetShader_Component_Selected(GlobalSceneGraph(), name);
			if (!name.empty())
				SetSelectedShader(name);

			ContentsFlagsValue flags(0, 0, 0, false);
			Scene_BrushGetFlags_Component_Selected(GlobalSceneGraph(), flags);
			SetSelectedFlags(flags);
		} else {
			TextureProjection projection;
			Scene_BrushGetTexdef_Selected(GlobalSceneGraph(), projection);
			SetSelectedTexdef(projection);

			std::string name;
			Scene_BrushGetShader_Selected(GlobalSceneGraph(), name);
			if (!name.empty())
				SetSelectedShader(name);

			ContentsFlagsValue flags(0, 0, 0, false);
			Scene_BrushGetFlags_Selected(GlobalSceneGraph(), flags);
			SurfaceInspector::SetSelectedFlags(flags);
		}
	}
}

/**
 * @sa SurfaceInspector::Update
 * @sa SurfaceInspector::SetCurrent_FromSelected
 */
const std::string& SurfaceInspector::GetSelectedShader (void)
{
	SurfaceInspector::SetCurrent_FromSelected();
	return g_selectedShader;
}

/**
 * @sa SurfaceInspector::Update
 * @sa SurfaceInspector::SetCurrent_FromSelected
 */
const TextureProjection& SurfaceInspector::GetSelectedTexdef (void)
{
	SurfaceInspector::SetCurrent_FromSelected();
	return g_selectedTexdef;
}

/**
 * @sa SurfaceInspector::Update
 * @sa SurfaceInspector::SetCurrent_FromSelected
 */
const ContentsFlagsValue& SurfaceInspector::GetSelectedFlags (void)
{
	SurfaceInspector::SetCurrent_FromSelected();
	return g_selectedFlags;
}

/*
 ===================================================
 SURFACE INSPECTOR
 ===================================================
 */

/**
 * @brief make the shift increments match the grid settings
 * the objective being that the shift+arrows shortcuts move the texture by the corresponding grid size
 * this depends on a scale value if you have selected a particular texture on which you want it to work:
 * we move the textures in pixels, not world units. (i.e. increment values are in pixel)
 * depending on the texture scale it doesn't take the same amount of pixels to move of GetGridSize()
 * @code increment * scale = gridsize @endcode
 * hscale and vscale are optional parameters, if they are zero they will be set to the default scale
 * @note the default scale is 0.5f (128 pixels cover 64 world units)
 */
void SurfaceInspector::DoSnapTToGrid (float hscale, float vscale)
{
	const float gridSize = GlobalGrid().getGridSize();
	_shift[0] = static_cast<float> (float_to_integer(gridSize / hscale));
	_shift[1] = static_cast<float> (float_to_integer(gridSize / vscale));
	queueDraw();
}

/**
 * @brief make the shift increments match the grid settings
 * the objective being that the shift+arrows shortcuts move the texture by the corresponding grid size
 * this depends on the current texture scale used?
 * we move the textures in pixels, not world units. (i.e. increment values are in pixel)
 * depending on the texture scale it doesn't take the same amount of pixels to move of GetGridSize()
 * @code increment * scale = gridsize @endcode
 */
void SurfaceInspector::OnBtnMatchGrid (GtkWidget *widget, SurfaceInspector *inspector)
{
	float hscale, vscale;
	hscale = static_cast<float> (gtk_spin_button_get_value_as_float(inspector->m_hscaleIncrement.m_spin));
	vscale = static_cast<float> (gtk_spin_button_get_value_as_float(inspector->m_vscaleIncrement.m_spin));

	if (hscale == 0.0f || vscale == 0.0f) {
		globalErrorStream() << "ERROR: unexpected scale == 0.0f\n";
		return;
	}

	inspector->DoSnapTToGrid(hscale, vscale);
}

void SurfaceInspector::FitTexture (void)
{
	UndoableCommand undo("textureAutoFit");
	Select_FitTexture(m_fitHorizontal, m_fitVertical);
}

void SurfaceInspector::Select_SetTexdef (const TextureProjection& projection)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetTexdef_Selected(GlobalSceneGraph(), projection);
	}
	Scene_BrushSetTexdef_Component_Selected(GlobalSceneGraph(), projection);
}

void SurfaceInspector::OnBtnAxial (GtkWidget *widget, SurfaceInspector *inspector)
{
	UndoableCommand undo("textureDefault");
	TextureProjection projection;
	projection.constructDefault();

	inspector->Select_SetTexdef(projection);
}

void SurfaceInspector::OnBtnFaceFit (GtkWidget *widget, SurfaceInspector *inspector)
{
	inspector->FitTexture();
}

const std::string& SurfaceInspector::getSurfaceFlagName (std::size_t bit) const
{
	const std::string& value = g_pGameDescription->getKeyValue(surfaceflagNamesDefault[bit]);
	return value;
}

const std::string& SurfaceInspector::getContentFlagName (std::size_t bit) const
{
	const std::string& value = g_pGameDescription->getKeyValue(contentflagNamesDefault[bit]);
	return value;
}

guint SurfaceInspector::togglebutton_connect_toggled (GtkToggleButton* button)
{
	return g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(SurfaceInspector::ApplyFlagsWithIndicator), this);
}

SurfaceInspector::SurfaceInspector () :
	m_textureEntry(ApplyShaderCaller(*this), UpdateCaller(*this)), m_hshiftSpinner(ApplyTexdefCaller(*this),
			UpdateCaller(*this)), m_hshiftEntry(Increment::ApplyCaller(m_hshiftIncrement), Increment::CancelCaller(
			m_hshiftIncrement)), m_vshiftSpinner(ApplyTexdefCaller(*this), UpdateCaller(*this)), m_vshiftEntry(
			Increment::ApplyCaller(m_vshiftIncrement), Increment::CancelCaller(m_vshiftIncrement)), m_hscaleSpinner(
			ApplyTexdefCaller(*this), UpdateCaller(*this)), m_hscaleEntry(Increment::ApplyCaller(m_hscaleIncrement),
			Increment::CancelCaller(m_hscaleIncrement)),
			m_vscaleSpinner(ApplyTexdefCaller(*this), UpdateCaller(*this)), m_vscaleEntry(Increment::ApplyCaller(
					m_vscaleIncrement), Increment::CancelCaller(m_vscaleIncrement)), m_rotateSpinner(ApplyTexdefCaller(
					*this), UpdateCaller(*this)), m_rotateEntry(Increment::ApplyCaller(m_rotateIncrement),
					Increment::CancelCaller(m_rotateIncrement)), m_idleDraw(UpdateCaller(*this)), m_valueEntry(
					ApplyFlagsCaller(*this), UpdateCaller(*this)), m_hshiftIncrement(_shift[0]), m_vshiftIncrement(
					_shift[1]), m_hscaleIncrement(_scale[0]), m_vscaleIncrement(_scale[1]), m_rotateIncrement(_rotate)
{
	m_fitVertical = 1;
	m_fitHorizontal = 1;
	m_valueInconsistent = false;
	s_texture_selection_dirty = false;

	GlobalGrid().addGridChangeCallback(MemberCaller<SurfaceInspector, &SurfaceInspector::gridChange> (*this));
	GlobalSelectionSystem().addSelectionChangeCallback(MemberCaller1<SurfaceInspector, const Selectable&,
			&SurfaceInspector::SelectionChanged> (*this));
	GlobalSceneGraph().addSceneChangedCallback(MemberCaller<SurfaceInspector, &SurfaceInspector::updateSelection> (
			*this));
	Brush_addTextureChangedCallback(MemberCaller<SurfaceInspector, &SurfaceInspector::updateSelection> (*this));

	_shift[0] = 8.0f;
	_shift[1] = 8.0f;
	_scale[0] = 0.5f;
	_scale[1] = 0.5f;
	_rotate = 45.0f;

	GlobalRegistry().addKeyObserver(this, RKEY_SNAP_TO_GRID);

	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);
}

float SurfaceInspector::getRotate () const
{
	return _rotate;
}

const Vector2& SurfaceInspector::getScale () const
{
	return _scale;
}
const Vector2& SurfaceInspector::getShift () const
{
	return _shift;
}

void SurfaceInspector::queueDraw (void)
{
	m_idleDraw.queueDraw();
}

void SurfaceInspector::gridChange (void)
{
	if (m_bSnapTToGrid) {
		float defaultScale = GlobalRegistry().getFloat("user/ui/textures/defaultTextureScale");
		DoSnapTToGrid(defaultScale, defaultScale);
	}
}

GtkWidget* SurfaceInspector::BuildNotebook (void)
{
	GtkWidget* pageframe = gtk_frame_new(_("Surface Inspector"));

	// replaced by only the vbox:
	GtkWidget *surfaceDialogBox = gtk_vbox_new(FALSE, 5);
	gtk_widget_show(surfaceDialogBox);
	gtk_container_add(GTK_CONTAINER(pageframe), GTK_WIDGET(surfaceDialogBox));
	gtk_container_set_border_width(GTK_CONTAINER(surfaceDialogBox), 5);

	{
		GtkWidget* hbox2 = gtk_hbox_new(FALSE, 5);
		gtk_widget_show(hbox2);
		gtk_box_pack_start(GTK_BOX(surfaceDialogBox), GTK_WIDGET(hbox2), FALSE, FALSE, 0);

		{
			GtkWidget* label = gtk_label_new(_("Texture"));
			gtk_widget_show(label);
			gtk_box_pack_start(GTK_BOX (hbox2), label, FALSE, TRUE, 0);
		}
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			gtk_box_pack_start(GTK_BOX(hbox2), GTK_WIDGET(entry), TRUE, TRUE, 0);
			m_texture = entry;
			m_textureEntry.connect(entry);
			GlobalTextureEntryCompletion::instance().connect(entry);
		}
	}

	{
		GtkWidget* table = gtk_table_new(6, 4, FALSE);
		gtk_widget_show(table);
		gtk_box_pack_start(GTK_BOX(surfaceDialogBox), GTK_WIDGET(table), FALSE, FALSE, 0);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);
		{
			GtkWidget* label = gtk_label_new(_("Horizontal shift"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkSpinButton
					* spin =
							GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 0)), 0, 2));
			m_hshiftIncrement.m_spin = spin;
			m_hshiftSpinner.connect(spin);
			gtk_widget_show(GTK_WIDGET(spin));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(spin), 60, 0);
		}
		{
			GtkWidget* label = gtk_label_new(C_("Step width", "Step"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(entry), 50, 0);
			m_hshiftIncrement.m_entry = entry;
			m_hshiftEntry.connect(entry);
		}
		{
			GtkWidget* label = gtk_label_new(_("Vertical shift"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkSpinButton
					* spin =
							GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 0)), 0, 2));
			m_vshiftIncrement.m_spin = spin;
			m_vshiftSpinner.connect(spin);
			gtk_widget_show(GTK_WIDGET(spin));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 1, 2, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(spin), 60, 0);
		}
		{
			GtkWidget* label = gtk_label_new(C_("Step width", "Step"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 1, 2, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(entry), 50, 0);
			m_vshiftIncrement.m_entry = entry;
			m_vshiftEntry.connect(entry);
		}
		{
			GtkWidget* label = gtk_label_new(_("Horizontal stretch"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkSpinButton
					* spin =
							GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 0)), 0, 5));
			m_hscaleIncrement.m_spin = spin;
			m_hscaleSpinner.connect(spin);
			gtk_widget_show(GTK_WIDGET(spin));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 2, 3, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(spin), 60, 0);
		}
		{
			GtkWidget* label = gtk_label_new(C_("Step width", "Step"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 2, 3, 2, 3, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 2, 3);
		}
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 2, 3, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 2, 3);
			widget_set_size(GTK_WIDGET(entry), 50, 0);
			m_hscaleIncrement.m_entry = entry;
			m_hscaleEntry.connect(entry);
		}
		{
			GtkWidget* label = gtk_label_new(_("Vertical stretch"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkSpinButton
					* spin =
							GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 0)), 0, 5));
			m_vscaleIncrement.m_spin = spin;
			m_vscaleSpinner.connect(spin);
			gtk_widget_show(GTK_WIDGET(spin));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 3, 4, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(spin), 60, 0);
		}
		{
			GtkWidget* label = gtk_label_new(C_("Step width", "Step"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 2, 3, 3, 4, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 3, 4, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(entry), 50, 0);
			m_vscaleIncrement.m_entry = entry;
			m_vscaleEntry.connect(entry);
		}
		{
			GtkWidget* label = gtk_label_new(_("Rotate"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkSpinButton
					* spin =
							GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 0)), 0, 2));
			m_rotateIncrement.m_spin = spin;
			m_rotateSpinner.connect(spin);
			gtk_widget_show(GTK_WIDGET(spin));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 4, 5, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(spin), 60, 0);
			gtk_spin_button_set_wrap(spin, TRUE);
		}
		{
			GtkWidget* label = gtk_label_new(C_("Step width", "Step"));
			gtk_widget_show(label);
			gtk_misc_set_alignment(GTK_MISC (label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label, 2, 3, 4, 5, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		}
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 4, 5, (GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			widget_set_size(GTK_WIDGET(entry), 50, 0);
			m_rotateIncrement.m_entry = entry;
			m_rotateEntry.connect(entry);
		}
		{
			// match grid button
			GtkWidget* button = gtk_button_new_with_label(_("Match Grid"));
			gtk_widget_show(button);
			gtk_table_attach(GTK_TABLE(table), button, 2, 4, 5, 6, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
			g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(OnBtnMatchGrid), this);
		}
	}

	{
		GtkWidget* frame = gtk_frame_new(_("Texturing"));
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(surfaceDialogBox), GTK_WIDGET(frame), FALSE, FALSE, 0);
		{
			GtkWidget* table = gtk_table_new(1, 2, FALSE);
			gtk_widget_show(table);
			gtk_container_add(GTK_CONTAINER(frame), table);
			gtk_table_set_row_spacings(GTK_TABLE(table), 5);
			gtk_table_set_col_spacings(GTK_TABLE(table), 5);
			gtk_container_set_border_width(GTK_CONTAINER (table), 5);
			{
				GtkWidget* button = gtk_button_new_with_label(_("Axial"));
				gtk_widget_show(button);
				gtk_table_attach(GTK_TABLE(table), button, 0, 1, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (0), 0, 0);
				g_signal_connect(G_OBJECT(button), "clicked",
						G_CALLBACK(OnBtnAxial), this);
				widget_set_size(button, 60, 0);
			}
			{
				GtkWidget* button = gtk_button_new_with_label(_("Fit"));
				gtk_widget_show(button);
				gtk_table_attach(GTK_TABLE(table), button, 1, 2, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (0), 0, 0);
				g_signal_connect(G_OBJECT(button), "clicked",
						G_CALLBACK(OnBtnFaceFit), this);
				widget_set_size(button, 60, 0);
			}
		}
	}
	{
		const std::string& valueEnablingFields = g_pGameDescription->getKeyValue("SurfaceInspector::enable_value");
		{
			m_surfaceFlagsFrame = GTK_FRAME(gtk_frame_new(_("Surface Flags")));
			gtk_widget_show(GTK_WIDGET (m_surfaceFlagsFrame));
			gtk_box_pack_start(GTK_BOX(surfaceDialogBox), GTK_WIDGET(m_surfaceFlagsFrame), TRUE, TRUE, 0);
			{
				GtkVBox* vbox3 = GTK_VBOX(gtk_vbox_new(FALSE, 4));
				//gtk_container_set_border_width(GTK_CONTAINER(vbox3), 4);
				gtk_widget_show(GTK_WIDGET (vbox3));
				gtk_container_add(GTK_CONTAINER(m_surfaceFlagsFrame), GTK_WIDGET(vbox3));
				{
					GtkTable* table = GTK_TABLE(gtk_table_new(8, 4, FALSE));
					gtk_widget_show(GTK_WIDGET (table));
					gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(table), TRUE, TRUE, 0);
					gtk_table_set_row_spacings(table, 0);
					gtk_table_set_col_spacings(table, 0);
					GtkCheckButton** p = m_surfaceFlags;
					for (int c = 0; c != 4; ++c) {
						for (int r = 0; r != 8; ++r) {
							const std::size_t id = c * 8 + r;
							const std::string& name = getSurfaceFlagName(id);
							GtkCheckButton* check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(name.c_str()));
							gtk_widget_show(GTK_WIDGET (check));
							gtk_table_attach(table, GTK_WIDGET(check), c, c + 1, r, r + 1,
									(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
							*p++ = check;
							guint handler_id = 0;
							if (name.empty()) {
								gtk_widget_set_sensitive(GTK_WIDGET(check), FALSE);
								handler_id = togglebutton_connect_toggled(GTK_TOGGLE_BUTTON(check));
							} else if (valueEnablingFields.find(name) != std::string::npos) {
								g_object_set_data(G_OBJECT(check), "valueEnabler", gint_to_pointer(true));
								handler_id = g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(
												SurfaceInspector::UpdateValueStatus), this);
							} else {
								handler_id = togglebutton_connect_toggled(GTK_TOGGLE_BUTTON(check));
							}
							g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(handler_id));
						}
					}
				}
			}
		}
		{
			m_contentFlagsFrame = GTK_FRAME(gtk_frame_new(_("Content Flags")));
			gtk_widget_show(GTK_WIDGET (m_contentFlagsFrame));
			gtk_box_pack_start(GTK_BOX(surfaceDialogBox), GTK_WIDGET(m_contentFlagsFrame), TRUE, TRUE, 0);
			{
				GtkVBox* vbox3 = GTK_VBOX(gtk_vbox_new(FALSE, 4));
				//gtk_container_set_border_width(GTK_CONTAINER(vbox3), 4);
				gtk_widget_show(GTK_WIDGET (vbox3));
				gtk_container_add(GTK_CONTAINER(m_contentFlagsFrame), GTK_WIDGET(vbox3));
				{
					GtkTable* table = GTK_TABLE(gtk_table_new(8, 4, FALSE));
					gtk_widget_show(GTK_WIDGET (table));
					gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(table), TRUE, TRUE, 0);
					gtk_table_set_row_spacings(table, 0);
					gtk_table_set_col_spacings(table, 0);
					GtkCheckButton** p = m_contentFlags;
					for (int c = 0; c != 4; ++c) {
						for (int r = 0; r != 8; ++r) {
							const std::size_t id = c * 8 + r;
							const std::string& name = getContentFlagName(id);
							GtkCheckButton* check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(name.c_str()));
							gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(check), FALSE);
							gtk_widget_show(GTK_WIDGET (check));
							gtk_table_attach(table, GTK_WIDGET(check), c, c + 1, r, r + 1,
									(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
							*p++ = check;
							guint handler_id = 0;
							if (name.empty()) {
								gtk_widget_set_sensitive(GTK_WIDGET(check), FALSE);
								handler_id = togglebutton_connect_toggled(GTK_TOGGLE_BUTTON(check));
							} else if (valueEnablingFields.find(name) != std::string::npos) {
								g_object_set_data(G_OBJECT(check), "valueEnabler", gint_to_pointer(true));
								handler_id = g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(
												SurfaceInspector::UpdateValueStatus), this);
							} else {
								handler_id = togglebutton_connect_toggled(GTK_TOGGLE_BUTTON(check));
							}
							g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(handler_id));
						}
					}
				}
			}
		}
	}
	{
		GtkFrame* frame = GTK_FRAME(gtk_frame_new(_("Value")));
		gtk_widget_show(GTK_WIDGET (frame));
		gtk_box_pack_start(GTK_BOX(surfaceDialogBox), GTK_WIDGET(frame), TRUE, TRUE, 0);
		{
			GtkVBox* vbox3 = GTK_VBOX(gtk_vbox_new(FALSE, 4));
			gtk_container_set_border_width(GTK_CONTAINER(vbox3), 4);
			gtk_widget_show(GTK_WIDGET (vbox3));
			gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(vbox3));

			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET (entry));
				gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_valueEntryWidget = entry;
				m_valueEntry.connect(entry);
			}
		}
	}

	return pageframe;
}

inline void spin_button_set_value_no_signal (GtkSpinButton* spin, gdouble value)
{
	guint handler_id = gpointer_to_int(g_object_get_data(G_OBJECT(spin), "handler"));
	g_signal_handler_block(G_OBJECT(gtk_spin_button_get_adjustment(spin)), handler_id);
	gtk_spin_button_set_value(spin, value);
	g_signal_handler_unblock(G_OBJECT(gtk_spin_button_get_adjustment(spin)), handler_id);
}

inline void spin_button_set_step_increment (GtkSpinButton* spin, gdouble value)
{
	GtkAdjustment* adjust = gtk_spin_button_get_adjustment(spin);
	adjust->step_increment = value;
}

/**
 * @brief Set the fields to the current texdef (i.e. map/texdef -> dialog widgets)
 * if faces selected (instead of brushes) -> will read this face texdef, else current texdef
 * @sa SurfaceInspector::GetSelectedFlags
 */
void SurfaceInspector::Update (void)
{
	const std::string& name = SurfaceInspector::GetSelectedShader();

	if (shader_is_texture(name)) {
		gtk_entry_set_text(m_texture, shader_get_textureName(name).c_str());
	} else {
		gtk_entry_set_text(m_texture, "");
	}

	TexDef shiftScaleRotate;

	shiftScaleRotate = SurfaceInspector::GetSelectedTexdef().m_texdef;

	// normalize again to hide the ridiculously high scale values that get created when using texlock
	shiftScaleRotate._shift[0] = float_mod(shiftScaleRotate._shift[0], (float) g_selectedShaderSize[0]);
	shiftScaleRotate._shift[1] = float_mod(shiftScaleRotate._shift[1], (float) g_selectedShaderSize[1]);

	{
		spin_button_set_value_no_signal(m_hshiftIncrement.m_spin, shiftScaleRotate._shift[0]);
		spin_button_set_step_increment(m_hshiftIncrement.m_spin, _shift[0]);
		entry_set_float(m_hshiftIncrement.m_entry, _shift[0]);
	}

	{
		spin_button_set_value_no_signal(m_vshiftIncrement.m_spin, shiftScaleRotate._shift[1]);
		spin_button_set_step_increment(m_vshiftIncrement.m_spin, _shift[1]);
		entry_set_float(m_vshiftIncrement.m_entry, _shift[1]);
	}

	{
		spin_button_set_value_no_signal(m_hscaleIncrement.m_spin, shiftScaleRotate._scale[0]);
		spin_button_set_step_increment(m_hscaleIncrement.m_spin, _scale[0]);
		entry_set_float(m_hscaleIncrement.m_entry, _scale[0]);
	}

	{
		spin_button_set_value_no_signal(m_vscaleIncrement.m_spin, shiftScaleRotate._scale[1]);
		spin_button_set_step_increment(m_vscaleIncrement.m_spin, _scale[1]);
		entry_set_float(m_vscaleIncrement.m_entry, _scale[1]);
	}

	{
		spin_button_set_value_no_signal(m_rotateIncrement.m_spin, shiftScaleRotate._rotate);
		spin_button_set_step_increment(m_rotateIncrement.m_spin, _rotate);
		entry_set_float(m_rotateIncrement.m_entry, _rotate);
	}

	UpdateFlagButtons();

	// Update the TexTool instance as well
	ui::TexTool::Instance().draw();
}

/**
 * @brief Updates check buttons based on actual content flags
 * @param[in] flags content flags to set button states for
 * @sa SurfaceInspector::Update
 */
void SurfaceInspector::UpdateFlagButtons ()
{
	ContentsFlagsValue flags = SurfaceInspector::GetSelectedFlags();
	bool enableValueField = false;

	for (GtkCheckButton** p = m_surfaceFlags; p != m_surfaceFlags + 32; ++p) {
		GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
		const unsigned int state = flags.m_surfaceFlags & (1 << (p - m_surfaceFlags));
		const unsigned int stateInconistent = flags.m_surfaceFlagsDirty & (1 << (p - m_surfaceFlags));
		gtk_toggle_button_set_inconsistent(b, stateInconistent);
		toggle_button_set_active_no_signal(b, state);
		if (state && g_object_get_data(G_OBJECT(*p), "valueEnabler") != 0) {
			enableValueField = true;
		}
	}

	if (GlobalSelectionSystem().areFacesSelected()) {
		gtk_widget_hide_all(GTK_WIDGET(m_contentFlagsFrame));
	} else {
		gtk_widget_show_all(GTK_WIDGET(m_contentFlagsFrame));

		for (GtkCheckButton** p = m_contentFlags; p != m_contentFlags + 32; ++p) {
			GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
			const unsigned int state = flags.m_contentFlags & (1 << (p - m_contentFlags));
			const unsigned int stateInconistent = flags.m_contentFlagsDirty & (1 << (p - m_contentFlags));
			gtk_toggle_button_set_inconsistent(b, stateInconistent);
			toggle_button_set_active_no_signal(b, state);
			if (state && g_object_get_data(G_OBJECT(*p), "valueEnabler") != 0) {
				enableValueField = true;
			}
		}
	}

	if (!flags.m_valueDirty) {
		entry_set_int(m_valueEntryWidget, flags.m_value);
		m_valueInconsistent = false;
	} else {
		entry_set_string(m_valueEntryWidget, "");
		m_valueInconsistent = true;
	}
	gtk_widget_set_sensitive(GTK_WIDGET(m_valueEntryWidget), enableValueField);

}

/**
 * @brief Reads the fields to get the current texdef (i.e. widgets -> MAP)
 */
void SurfaceInspector::ApplyShader (void)
{
	std::string name = GlobalTexturePrefix_get() + gtk_entry_get_text(m_texture);

	// TTimo: detect and refuse invalid texture names (at least the ones with spaces)
	if (!texdef_name_valid(name)) {
		globalErrorStream() << "invalid texture name '" << name << "'\n";
		SurfaceInspector::queueDraw();
		return;
	}

	UndoableCommand undo("textureNameSetSelected");
	Select_SetShader(name);

	// Update the TexTool instance as well
	ui::TexTool::Instance().draw();
}

void SurfaceInspector::ApplyTexdef (void)
{
	TexDef shiftScaleRotate;

	shiftScaleRotate._shift[0] = static_cast<float> (gtk_spin_button_get_value_as_float(m_hshiftIncrement.m_spin));
	shiftScaleRotate._shift[1] = static_cast<float> (gtk_spin_button_get_value_as_float(m_vshiftIncrement.m_spin));
	shiftScaleRotate._scale[0] = static_cast<float> (gtk_spin_button_get_value_as_float(m_hscaleIncrement.m_spin));
	shiftScaleRotate._scale[1] = static_cast<float> (gtk_spin_button_get_value_as_float(m_vscaleIncrement.m_spin));
	shiftScaleRotate._rotate = static_cast<float> (gtk_spin_button_get_value_as_float(m_rotateIncrement.m_spin));

	TextureProjection projection(shiftScaleRotate);

	UndoableCommand undo("textureProjectionSetSelected");
	Select_SetTexdef(projection);

	// Update the TexTool instance as well
	ui::TexTool::Instance().draw();
}

void SurfaceInspector::ApplyFlags (void)
{
	m_valueInconsistent = false;
	ApplyFlagsWithIndicator(GTK_WIDGET(m_valueEntryWidget), this);
}

/**
 * @brief extended callback used to update value field sensitivity based on activation state of button
 * @param widget toggle button activated
 * @param inspector reference to surface inspector
 */
void SurfaceInspector::UpdateValueStatus (GtkWidget *widget, SurfaceInspector *inspector)
{
	ApplyFlagsWithIndicator(widget, inspector);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		gtk_widget_set_sensitive(GTK_WIDGET(inspector->m_valueEntryWidget), true);
	} else {
		/* we don't know whether there are other flags that enable value field, so check them all */
		inspector->UpdateFlagButtons();
	}
}

/**
 * @todo Set contentflags for whole brush when we are in face selection mode
 */
void SurfaceInspector::Select_SetFlags (const ContentsFlagsValue& flags)
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
 * @todo Change this to only update those, that were changed
 * @todo Make sure to set those flags that are not used (and not clickable anymore)
 * are set back to 0
 */
void SurfaceInspector::ApplyFlagsWithIndicator (GtkWidget *activatedWidget, SurfaceInspector *inspector)
{
	unsigned int surfaceflags = 0;
	unsigned int surfaceflagsDirty = 0;
	for (GtkCheckButton** p = inspector->m_surfaceFlags; p != inspector->m_surfaceFlags + 32; ++p) {
		GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
		unsigned int bitmask = 1 << (p - inspector->m_surfaceFlags);
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
	for (GtkCheckButton** p = inspector->m_contentFlags; p != inspector->m_contentFlags + 32; ++p) {
		GtkToggleButton *b = GTK_TOGGLE_BUTTON(*p);
		unsigned int bitmask = 1 << (p - inspector->m_contentFlags);
		if (activatedWidget == GTK_WIDGET(b))
			gtk_toggle_button_set_inconsistent(b, FALSE);
		if (gtk_toggle_button_get_inconsistent(b)) {
			contentflagsDirty |= bitmask;
		}
		if (gtk_toggle_button_get_active(b)) {
			contentflags |= bitmask;
		}
	}

	int value = entry_get_int(inspector->m_valueEntryWidget);

	UndoableCommand undo("flagsSetSelected");
	globalOutputStream() << "dirty: " << surfaceflagsDirty << "\n";
	/* set flags to the selection */
	inspector->Select_SetFlags(ContentsFlagsValue(surfaceflags, contentflags, value, true, surfaceflagsDirty,
			contentflagsDirty, inspector->m_valueInconsistent));
}

Texturable SurfaceInspector::Scene_getClosestTexturable (scene::Graph& graph, SelectionTest& test)
{
	Texturable texturable;
	graph.traverse(BrushGetClosestFaceVisibleWalker(test, texturable));
	return texturable;
}

bool SurfaceInspector::Scene_getClosestTexture (scene::Graph& graph, SelectionTest& test, std::string& shader,
		TextureProjection& projection, ContentsFlagsValue& flags)
{
	Texturable texturable = Scene_getClosestTexturable(graph, test);
	if (texturable.getTexture != GetTextureCallback()) {
		texturable.getTexture(shader, projection, flags);
		return true;
	}
	return false;
}

void SurfaceInspector::Scene_setClosestTexture (scene::Graph& graph, SelectionTest& test, const std::string& shader,
		const TextureProjection& projection, const ContentsFlagsValue& flags)
{
	Texturable texturable = Scene_getClosestTexturable(graph, test);
	if (texturable.setTexture != SetTextureCallback()) {
		texturable.setTexture(shader, projection, flags);
	}
}

void SurfaceInspector::TextureClipboard_textureSelected ()
{
	g_faceTextureClipboard.m_flags = ContentsFlagsValue(0, 0, 0, false);
	g_faceTextureClipboard.m_projection.constructDefault();
}

void SurfaceInspector::Scene_copyClosestTexture (SelectionTest& test)
{
	std::string shader;
	if (Scene_getClosestTexture(GlobalSceneGraph(), test, shader, g_faceTextureClipboard.m_projection,
			g_faceTextureClipboard.m_flags)) {
		GlobalTextureBrowser().setSelectedShader(shader);
	}
}

void SurfaceInspector::Scene_applyClosestTexture (SelectionTest& test)
{
	UndoableCommand command("facePaintTexture");

	Scene_setClosestTexture(GlobalSceneGraph(), test, GlobalTextureBrowser().getSelectedShader(),
			g_faceTextureClipboard.m_projection, g_faceTextureClipboard.m_flags);

	SceneChangeNotify();
}

/**
 * @todo Don't change the levelflags here (content flags)
 */
void SurfaceInspector::SelectedFaces_copyTexture (void)
{
	if (GlobalSelectionSystem().areFacesSelected()) {
		Face& face = g_SelectedFaceInstances.last().getFace();
		face.GetTexdef(g_faceTextureClipboard.m_projection);
		g_faceTextureClipboard.m_flags = face.getShader().m_flags;

		GlobalTextureBrowser().setSelectedShader(face.getShader().getShader());
	}
}

void SurfaceInspector::FaceInstance_pasteTexture (FaceInstance& faceInstance)
{
	faceInstance.getFace().SetTexdef(GlobalSurfaceInspector().g_faceTextureClipboard.m_projection);
	faceInstance.getFace().SetShader(GlobalTextureBrowser().getSelectedShader());
	faceInstance.getFace().SetFlags(GlobalSurfaceInspector().g_faceTextureClipboard.m_flags);
	SceneChangeNotify();
}

void SurfaceInspector::SelectedFaces_pasteTexture (void)
{
	UndoableCommand command("facePasteTexture");
	g_SelectedFaceInstances.foreach(FaceInstance_pasteTexture);
}

void SurfaceInspector::FlipTextureX ()
{
	Select_FlipTexture(0);
}

void SurfaceInspector::FlipTextureY ()
{
	Select_FlipTexture(1);
}

void SurfaceInspector::ToggleTexTool ()
{
	// Call the toggle() method of the static instance
	ui::TexTool::Instance().toggle();
}

void SurfaceInspector::registerCommands (void)
{
	GlobalEventManager().addCommand("FitTexture", MemberCaller<SurfaceInspector, &SurfaceInspector::FitTexture> (*this));
	GlobalEventManager().addCommand("TextureTool", MemberCaller<SurfaceInspector, &SurfaceInspector::ToggleTexTool> (
			*this));

	GlobalEventManager().addCommand("FaceCopyTexture", MemberCaller<SurfaceInspector,
			&SurfaceInspector::SelectedFaces_copyTexture> (*this));
	GlobalEventManager().addCommand("FacePasteTexture", MemberCaller<SurfaceInspector,
			&SurfaceInspector::SelectedFaces_pasteTexture> (*this));

	GlobalEventManager().addCommand("FlipTextureX", MemberCaller<SurfaceInspector, &SurfaceInspector::FlipTextureX> (
			*this));
	GlobalEventManager().addCommand("FlipTextureY", MemberCaller<SurfaceInspector, &SurfaceInspector::FlipTextureY> (
			*this));
}

void SurfaceInspector::keyChanged (const std::string& changedKey, const std::string& newValue)
{
	m_bSnapTToGrid = GlobalRegistry().getBool(RKEY_SNAP_TO_GRID);
}

void SurfaceInspector::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage* page(group.createPage(_("Surface Inspector"), _("Surface Inspector Preferences")));

	page->appendCheckBox("", _("Surface Inspector Increments Match Grid"), RKEY_SNAP_TO_GRID);
}

void SurfaceInspector_Construct (void)
{
	GlobalSurfaceInspector().registerCommands();

	GlobalSurfaceInspector().TextureClipboard_textureSelected();
}

void SurfaceInspector_Destroy (void)
{
}
