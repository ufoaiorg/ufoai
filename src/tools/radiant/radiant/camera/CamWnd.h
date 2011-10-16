#ifndef CAMWND_H_
#define CAMWND_H_

#include "RadiantCameraView.h"
#include "irender.h"
#include "gtkutil/xorrectangle.h"
#include "../selection/RadiantWindowObserver.h"
#include "gtkutil/glwidget.h"

#include "../map/DeferredDraw.h"

const int CAMWND_MINSIZE_X = 240;
const int CAMWND_MINSIZE_Y = 200;

class SelectionTest;

class CamWnd : public scene::Graph::Observer
{
		View m_view;
		// The contained camera
		Camera m_Camera;
		RadiantCameraView m_cameraview;

		guint m_freemove_handle_focusout;
		static Shader* m_state_select1;
		static Shader* m_state_select2;
		FreezePointer m_freezePointer;

		bool m_drawing;

	public:
		gtkutil::GLWidget _glWidget;
		GtkWidget* m_gl_widget;
		GtkWindow* m_parent;

		SelectionSystemWindowObserver* m_window_observer;

		XORRectangle m_XORRectangle;

		DeferredDraw m_deferredDraw;
		DeferredMotion m_deferred_motion;

		guint m_selection_button_press_handler;
		guint m_selection_button_release_handler;
		guint m_selection_motion_handler;
		guint m_freelook_button_press_handler;
		guint m_sizeHandler;
		guint m_exposeHandler;

		CamWnd ();
		~CamWnd ();

		const Vector3& getCameraOrigin () const;
		void setCameraOrigin (const Vector3& origin);

		const Vector3& getCameraAngles () const;
		void setCameraAngles (const Vector3& origin);

		// Increases/decreases the far clip plane distance
		void cubicScaleIn ();
		void cubicScaleOut ();

		void queueDraw ();
		void draw ();
		void update ();

		static void captureStates ();
		static void releaseStates ();

		Camera& getCamera ();

		void changeFloor (bool up);

		GtkWidget* getWidget ();
		void setParent(GtkWindow* parent);
		GtkWindow* getParent ();

		void enableFreeMove ();
		void disableFreeMove ();

		bool m_bFreeMove;

		void jumpToObject(SelectionTest& selectionTest);

		CameraView* getCameraView ();

		// Enables/disables the (ordinary) camera movement (non-freelook)
		void addHandlersMove ();
		void removeHandlersMove ();

		void enableDiscreteMoveEvents();
		void enableFreeMoveEvents();

		void disableDiscreteMoveEvents();
		void disableFreeMoveEvents();

	private:
		void Cam_Draw ();

		static gboolean selection_button_press_freemove(GtkWidget* widget, GdkEventButton* event, WindowObserver* observer);
		static gboolean selection_button_release_freemove(GtkWidget* widget, GdkEventButton* event, WindowObserver* observer);
		static gboolean selection_motion_freemove(GtkWidget *widget, GdkEventMotion *event, WindowObserver* observer);
		static gboolean wheelmove_scroll(GtkWidget* widget, GdkEventScroll* event, CamWnd* camwnd);
		static gboolean selection_button_press(GtkWidget* widget, GdkEventButton* event, WindowObserver* observer);
		static gboolean selection_button_release(GtkWidget* widget, GdkEventButton* event, WindowObserver* observer);
		static gboolean enable_freelook_button_press(GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd);
		static gboolean disable_freelook_button_press(GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd);

		static gboolean camera_size_allocate(GtkWidget* widget, GtkAllocation* allocation, CamWnd* camwnd);
		static gboolean camera_expose(GtkWidget* widget, GdkEventExpose* event, gpointer data);

		void onSceneGraphChange ();
};

typedef MemberCaller<CamWnd, &CamWnd::queueDraw> CamWndQueueDraw;
typedef MemberCaller<CamWnd, &CamWnd::update> CamWndUpdate;

#endif /*CAMWND_H_*/
