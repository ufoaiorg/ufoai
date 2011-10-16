#ifndef XYWND_H_
#define XYWND_H_

#include "../clipper/Clipper.h"

#include <gdk/gdkevents.h>
#include <gtk/gtkwidget.h>

#include "math/Vector3.h"
#include "math/matrix.h"
#include "gtkutil/cursor.h"
#include "gtkutil/window.h"
#include "gtkutil/xorrectangle.h"
#include "../timer.h"
#include "iradiant.h"
#include "math/Vector4.h"

#include "../map/DeferredDraw.h"
#include "../camera/CameraObserver.h"
#include "../camera/CamWnd.h"
#include "../selection/RadiantWindowObserver.h"

namespace {
const int XYWND_MINSIZE_X = 200;
const int XYWND_MINSIZE_Y = 200;
}

class XYWnd: public CameraObserver, public scene::Graph::Observer
{
		gtkutil::GLWidget _glWidget;
		GtkWidget* m_gl_widget;
		guint m_sizeHandler;
		guint m_exposeHandler;

		DeferredDraw m_deferredDraw;
		DeferredMotion m_deferred_motion;

		// The timer used for chase mouse xyview movements
		Timer _chaseMouseTimer;
		FreezePointer _freezePointer;

		GtkWindow* _parent;

		// The maximum/minimum values of a coordinate
		float _minWorldCoord;
		float _maxWorldCoord;

	public:
		// Constructor, this allocates the GL widget
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

		void setParent(GtkWindow* parent);
		GtkWindow* getParent() const;

		SelectionSystemWindowObserver* m_window_observer;
		XORRectangle m_XORRectangle;

		static void captureStates ();
		static void releaseStates ();

		void positionView (const Vector3& position);
		const Vector3& getOrigin () const;
		void setOrigin (const Vector3& origin);
		void scroll (int x, int y);

		void positionCamera(int x, int y, CamWnd& camwnd);
		void orientCamera(int x, int y, CamWnd& camwnd);

		void draw ();
		void drawCameraIcon (const Vector3& origin, const Vector3& angles);
		void drawBlockGrid ();
		void drawAxis ();
		void drawGrid ();

		void NewBrushDrag_Begin (int x, int y);
		void NewBrushDrag (int x, int y);
		void NewBrushDrag_End (int x, int y);

		void convertXYToWorld (int x, int y, Vector3& point);
		void snapToGrid (Vector3& point);

		void mouseToPoint(int x, int y, Vector3& point);

		void updateXORRectangle (Rectangle area);

		void beginMove ();
		void endMove ();
		bool _moveStarted;
		guint m_move_focusOut;

		void beginZoom ();
		void endZoom ();
		bool _zoomStarted;
		guint m_zoom_focusOut;
		int _dragZoom;

		void setActive (bool b)
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
		bool chaseMouseMotion (int pointx, int pointy, const unsigned int& state);

		void updateModelview ();
		void updateProjection ();
		Matrix4 m_projection;
		Matrix4 m_modelview;

		int m_nWidth;
		int m_nHeight;

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

		void onContextMenu ();
		void drawSizeInfo (int nDim1, int nDim2, Vector3& vMinBounds, Vector3& vMaxBounds);

		int m_entityCreate_x, m_entityCreate_y;
		bool m_entityCreate;
		void onSceneGraphChange ();

		// Save the current event state
		GdkEventButton* _event;

		void CameraMoved ();

		// GTK callbacks
		static gboolean callbackButtonPress (GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd);
		static gboolean callbackButtonRelease (GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd);
		static void callbackMouseMotion (gdouble x, gdouble y, guint state, void* data);
		static gboolean callbackMouseWheelScroll (GtkWidget* widget, GdkEventScroll* event, XYWnd* xywnd);
		static gboolean callbackSizeAllocate (GtkWidget* widget, GtkAllocation* allocation, XYWnd* xywnd);
		static gboolean callbackExpose (GtkWidget* widget, GdkEventExpose* event, XYWnd* xywnd);
		static gboolean onFocusOut (GtkWidget* widget, GdkEventFocus* event, XYWnd* xywnd);

		static gboolean xywnd_chasemouse (gpointer data);
		static void zoomDelta (int x, int y, unsigned int state, void* data);
		Vector4 getWindowCoordinates();
	public:
		void EntityCreate_MouseDown (int x, int y);
		void EntityCreate_MouseMove (int x, int y);
		void EntityCreate_MouseUp (int x, int y);

		EViewType getViewType ();
		static const std::string getViewTypeTitle(EViewType viewtype);
		void setScale (float f);
		float getScale () const;
		int getWidth () const;
		int getHeight () const;

		void zoomIn ();
		void zoomOut ();

		// Save the current GDK event state
		void setEvent(GdkEventButton* event);

		// The method handling the different mouseUp situations according to <event>
		void mouseUp(int x, int y, GdkEventButton* event);

		// The method responsible for mouseMove situations according to <event>
		void mouseMoved(int x, int y, const unsigned int& state);

		// The method handling the different mouseDown situations
		void mouseDown(int x, int y, GdkEventButton* event);
		typedef Member3<XYWnd, int, int, GdkEventButton*, void, &XYWnd::mouseDown> MouseDownCaller;

		// greebo: CameraObserver implementation; gets called when the camera is moved
		void cameraMoved();
}; // class XYWnd

#endif /*XYWND_H_*/
