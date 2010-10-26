/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#include "brushmodule.h"
#include "radiant_i18n.h"

#include "iradiant.h"

#include "brushnode.h"
#include "brushmanip.h"

#include "preferencesystem.h"
#include "stringio.h"

#include "../map/map.h"
#include "../qe3.h"
#include "../dialog.h"
#include "../mainframe.h"
#include "../settings/preferences.h"

LatchedBool g_useAlternativeTextureProjection(false, "Use alternative texture-projection");
bool g_showAlternativeTextureProjectionOption = false;
bool g_brush_always_nodraw = false;

void Face_importSnapPlanes (bool value)
{
	Face::m_quantise = value ? quantiseInteger : quantiseFloating;
}
typedef FreeCaller1<bool, Face_importSnapPlanes> FaceImportSnapPlanesCaller;

void Face_exportSnapPlanes (const BoolImportCallback& importer)
{
	importer(Face::m_quantise == quantiseInteger);
}
typedef FreeCaller1<const BoolImportCallback&, Face_exportSnapPlanes> FaceExportSnapPlanesCaller;

void Brush_constructPreferences (PreferencesPage& page)
{
	page.appendCheckBox("", _("Snap planes to integer grid"), FaceImportSnapPlanesCaller(), FaceExportSnapPlanesCaller());
	page.appendEntry(_("Default texture scale"), g_texdef_default_scale);
	if (g_showAlternativeTextureProjectionOption) {
		page.appendCheckBox("", _("Use alternative texture-projection"), LatchedBoolImportCaller(
				g_useAlternativeTextureProjection), BoolExportCaller(g_useAlternativeTextureProjection.m_latched));
	}
	page.appendCheckBox("", _("Always use nodraw for new brushes"), g_brush_always_nodraw);
}
void Brush_constructPage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage(_("Brush"), _("Brush Settings")));
	Brush_constructPreferences(page);
}
void Brush_registerPreferencesPage ()
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Brush_constructPage> ());
}

void Brush_Construct ()
{
	Brush_registerCommands();
	Brush_registerPreferencesPage();

	BrushFilters_construct();

	BrushClipPlane::constructStatic();
	BrushInstance::constructStatic();
	Brush::constructStatic();

	Brush::m_maxWorldCoord = g_MaxWorldCoord;
	BrushInstance::m_counter = &g_brushCount;

	g_texdef_default_scale = 0.25f;

	GlobalPreferenceSystem().registerPreference("TextureLock", BoolImportStringCaller(g_brush_texturelock_enabled),
			BoolExportStringCaller(g_brush_texturelock_enabled));
	GlobalPreferenceSystem().registerPreference("BrushSnapPlanes", makeBoolStringImportCallback(
			FaceImportSnapPlanesCaller()), makeBoolStringExportCallback(FaceExportSnapPlanesCaller()));
	GlobalPreferenceSystem().registerPreference("TexdefDefaultScale", FloatImportStringCaller(g_texdef_default_scale),
			FloatExportStringCaller(g_texdef_default_scale));
}

void Brush_Destroy ()
{
	Brush::m_maxWorldCoord = 0;
	BrushInstance::m_counter = 0;

	Brush::destroyStatic();
	BrushInstance::destroyStatic();
	BrushClipPlane::destroyStatic();
}

void Brush_clipperColourChanged ()
{
	BrushClipPlane::destroyStatic();
	BrushClipPlane::constructStatic();
}

void BrushFaceData_fromFace (const BrushFaceDataCallback& callback, Face& face)
{
	_QERFaceData faceData;
	faceData.m_p0 = face.getPlane().planePoints()[0];
	faceData.m_p1 = face.getPlane().planePoints()[1];
	faceData.m_p2 = face.getPlane().planePoints()[2];
	faceData.m_shader = face.GetShader();
	faceData.m_texdef = face.getTexdef().m_projection.m_texdef;
	faceData.contents = face.getShader().m_flags.m_contentFlags;
	faceData.flags = face.getShader().m_flags.m_surfaceFlags;
	faceData.value = face.getShader().m_flags.m_value;
	callback(faceData);
}
typedef ConstReferenceCaller1<BrushFaceDataCallback, Face&, BrushFaceData_fromFace> BrushFaceDataFromFaceCaller;
typedef Callback1<Face&> FaceCallback;

class UFOBrushCreator: public BrushCreator
{
	public:
		scene::Node& createBrush ()
		{
			return (new BrushNode)->node();
		}
		bool useAlternativeTextureProjection () const
		{
			return g_useAlternativeTextureProjection.m_value;
		}
		void Brush_forEachFace (scene::Node& brush, const BrushFaceDataCallback& callback)
		{
			::Brush_forEachFace(*Node_getBrush(brush), FaceCallback(BrushFaceDataFromFaceCaller(callback)));
		}
		bool Brush_addFace (scene::Node& brush, const _QERFaceData& faceData)
		{
			Node_getBrush(brush)->undoSave();
			return Node_getBrush(brush)->addPlane(faceData.m_p0, faceData.m_p1, faceData.m_p2, faceData.m_shader,
					TextureProjection(faceData.m_texdef, BrushPrimitTexDef(), Vector3(0, 0, 0), Vector3(0, 0, 0)))
					!= 0;
		}
};

UFOBrushCreator g_UFOBrushCreator;

BrushCreator& GetBrushCreator ()
{
	return g_UFOBrushCreator;
}

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class BrushDependencies: public GlobalRadiantModuleRef,
		public GlobalSceneGraphModuleRef,
		public GlobalShaderCacheModuleRef,
		public GlobalSelectionModuleRef,
		public GlobalOpenGLModuleRef,
		public GlobalUndoModuleRef,
		public GlobalFilterModuleRef
{
};

class BrushUFOAPI: public TypeSystemRef
{
		BrushCreator* m_brushufo;
	public:
		typedef BrushCreator Type;
		STRING_CONSTANT(Name, "ufo")
		;

		BrushUFOAPI ()
		{
			Brush_Construct();

			m_brushufo = &GetBrushCreator();
		}
		~BrushUFOAPI ()
		{
			Brush_Destroy();
		}
		BrushCreator* getTable ()
		{
			return m_brushufo;
		}
};

typedef SingletonModule<BrushUFOAPI, BrushDependencies> BrushUFOModule;
typedef Static<BrushUFOModule> StaticBrushUFOModule;
StaticRegisterModule staticRegisterBrushUFO (StaticBrushUFOModule::instance ());
