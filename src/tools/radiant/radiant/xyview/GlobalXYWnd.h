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

#if !defined(INCLUDED_XYWINDOW_H)
#define INCLUDED_XYWINDOW_H

#include "math/matrix.h"
#include "signal/signal.h"

#include "gtkutil/cursor.h"
#include "gtkutil/window.h"
#include "gtkutil/xorrectangle.h"
#include "gtkutil/glwidget.h"
#include "../camera/view.h"
#include "../map/map.h"
#include "texturelib.h"
#include <gtk/gtkmenuitem.h>

// Constants
const int XYWND_MINSIZE_X = 200;
const int XYWND_MINSIZE_Y = 200;

#include "iradiant.h"

class Shader;
class SelectionSystemWindowObserver;
namespace scene
{
	class Node;
}
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkMenu GtkMenu;

class XYWnd
{
		gtkutil::GLWidget _glWidget;
		GtkWidget* m_gl_widget;
		guint m_sizeHandler;
		guint m_exposeHandler;

		DeferredDraw m_deferredDraw;
		DeferredMotion m_deferred_motion;
	public:
		GtkWindow* m_parent;
		XYWnd ();
		~XYWnd ();

		void queueDraw ()
		{
			m_deferredDraw.draw();
		}
		GtkWidget* getWidget ()
		{
			return m_gl_widget;
		}

	public:
		SelectionSystemWindowObserver* m_window_observer;
		XORRectangle m_XORRectangle;

		static void captureStates ();
		static void releaseStates ();

		void positionView (const Vector3& position);
		const Vector3& getOrigin () const;
		void setOrigin (const Vector3& origin);
		void scroll (int x, int y);

		void draw ();
		void drawCameraIcon (const Vector3& origin, const Vector3& angles);
		void drawBlockGrid ();
		void drawAxis ();
		void drawGrid ();
		void drawBackground ();
		void loadBackgroundImage (const std::string& name);
		void disableBackground ();

		void XY_MouseUp (int x, int y, unsigned int buttons);
		void XY_MouseDown (int x, int y, unsigned int buttons);
		void XY_MouseMoved (int x, int y, unsigned int buttons);

		void NewBrushDrag_Begin (int x, int y);
		void NewBrushDrag (int x, int y);
		void NewBrushDrag_End (int x, int y);

		void convertXYToWorld (int x, int y, Vector3& point);
		void snapToGrid (Vector3& point);

		void beginMove ();
		void endMove ();
		bool _moveStarted;
		guint m_move_focusOut;

		void beginZoom ();
		void endZoom ();
		bool _zoomStarted;
		guint m_zoom_focusOut;

		void SetActive (bool b)
		{
			m_bActive = b;
		}

		bool Active ()
		{
			return m_bActive;
		}

		void Clipper_OnLButtonDown (int x, int y);
		void Clipper_OnLButtonUp (int x, int y);
		void Clipper_OnMouseMoved (int x, int y);
		void Clipper_Crosshair_OnMouseMoved (int x, int y);
		void DropClipPoint (int pointx, int pointy);

		void setViewType (EViewType n);
		bool m_bActive;

		int m_chasemouse_current_x, m_chasemouse_current_y;
		int m_chasemouse_delta_x, m_chasemouse_delta_y;

		guint _chaseMouseHandler;
		void chaseMouse ();
		bool chaseMouseMotion (int pointx, int pointy);

		void updateModelview ();
		void updateProjection ();
		Matrix4 m_projection;
		Matrix4 m_modelview;

		int m_nWidth;
		int m_nHeight;

		// background image stuff
		qtexture_t *m_tex;
		bool m_backgroundActivated;
		float m_alpha; // vertex alpha
		float m_xmin, m_ymin, m_xmax, m_ymax;
	private:
		float m_fScale;
		Vector3 m_vOrigin;

		View m_view;
		static Shader* m_state_selected;

		int m_ptCursorX, m_ptCursorY;

		unsigned int m_buttonstate;

		int m_nNewBrushPressx;
		int m_nNewBrushPressy;
		scene::Node* m_NewBrushDrag;
		bool m_bNewBrushDrag;

		Vector3 m_mousePosition;

		EViewType m_viewType;

		void OriginalButtonUp (guint32 nFlags, int point, int pointy);
		void OriginalButtonDown (guint32 nFlags, int point, int pointy);

		static void callbackMoveDelta (int x, int y, unsigned int state, void* data);
		static gboolean callbackMoveFocusOut (GtkWidget* widget, GdkEventFocus* event, XYWnd* xywnd);

		GtkMenuItem* m_mnitem_separator;
		GtkMenuItem* m_mnitem_connect;
		GtkMenuItem* m_mnitem_fitface;
		void onContextMenu ();
		void drawSizeInfo (int nDim1, int nDim2, Vector3& vMinBounds, Vector3& vMaxBounds);

		int m_entityCreate_x, m_entityCreate_y;
		bool m_entityCreate;

		void CameraMoved ();

		// GTK callbacks
		static gboolean callbackButtonPress (GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd);
		static gboolean callbackButtonRelease (GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd);
		static void callbackMouseMotion (gdouble x, gdouble y, guint state, void* data);
		static gboolean callbackMouseWheelScroll (GtkWidget* widget, GdkEventScroll* event, XYWnd* xywnd);
		static gboolean callbackSizeAllocate (GtkWidget* widget, GtkAllocation* allocation, XYWnd* xywnd);
		static gboolean callbackExpose (GtkWidget* widget, GdkEventExpose* event, XYWnd* xywnd);

	public:
		void ButtonState_onMouseDown (unsigned int buttons)
		{
			m_buttonstate |= buttons;
		}
		void ButtonState_onMouseUp (unsigned int buttons)
		{
			m_buttonstate &= ~buttons;
		}
		unsigned int getButtonState () const
		{
			return m_buttonstate;
		}
		void EntityCreate_MouseDown (int x, int y);
		void EntityCreate_MouseMove (int x, int y);
		void EntityCreate_MouseUp (int x, int y);

		EViewType getViewType ()
		{
			return m_viewType;
		}
		void setScale (float f);
		float getScale () const
		{
			return m_fScale;
		}
		int getWidth () const
		{
			return m_nWidth;
		}
		int getHeight () const
		{
			return m_nHeight;
		}

		Signal0 onDestroyed;
		Signal3<const WindowVector&, ButtonIdentifier, ModifierFlags> onMouseDown;
		void mouseDown (const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers);
		typedef Member3<XYWnd, const WindowVector&, ButtonIdentifier, ModifierFlags, void, &XYWnd::mouseDown>
				MouseDownCaller;
};

struct xywindow_globals_t
{
		Vector3 color_gridback;
		Vector3 color_gridminor;
		Vector3 color_gridmajor;
		Vector3 color_gridblock;
		Vector3 color_gridtext;
		Vector3 color_brushes;
		Vector3 color_selbrushes;
		Vector3 color_clipper;
		Vector3 color_viewname;
		Vector3 color_gridminor_alt;
		Vector3 color_gridmajor_alt;
		Vector3 AxisColorX;
		Vector3 AxisColorY;
		Vector3 AxisColorZ;

		bool m_bRightClick; //! activates the context menu
		bool m_bNoStipple;

		xywindow_globals_t () :
			color_gridback(0.77f, 0.77f, 0.77f), color_gridminor(0.83f, 0.83f, 0.83f), color_gridmajor(0.89f, 0.89f,
					0.89f), color_gridblock(1.0f, 1.0f, 1.0f), color_gridtext(0.0f, 0.0f, 0.0f), color_brushes(0.0f,
					0.0f, 0.0f), color_selbrushes(1.0f, 0.0f, 0.0f), color_clipper(0.0f, 0.0f, 1.0f), color_viewname(
					0.5f, 0.0f, 0.75f), color_gridminor_alt(0.f, 0.f, 0.f), color_gridmajor_alt(0.f, 0.f, 0.f),

			AxisColorX(1.f, 0.f, 0.f), AxisColorY(0.f, 1.f, 0.f), AxisColorZ(0.f, 0.f, 1.f), m_bRightClick(true),
					m_bNoStipple(false)
		{
		}

};

extern xywindow_globals_t g_xywindow_globals;

EViewType GlobalXYWnd_getCurrentViewType ();

typedef struct _GtkWindow GtkWindow;
void XY_Top_Shown_Construct (GtkWindow* parent);
void YZ_Side_Shown_Construct (GtkWindow* parent);
void XZ_Front_Shown_Construct (GtkWindow* parent);

void XYWindow_Construct ();
void XYWindow_Destroy ();
void WXY_BackgroundSelect ();
void XYShow_registerCommands ();
void XYWnd_registerShortcuts ();

#endif
