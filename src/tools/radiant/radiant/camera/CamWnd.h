#ifndef CAMWND_H_
#define CAMWND_H_

#include "gtkutil/glwidget.h"

class CamWnd
{
	private:
		View m_view;
		camera_t m_Camera;
		CameraView m_cameraview;

		guint m_freemove_handle_focusout;

		static Shader* m_state_select1;
		static Shader* m_state_select2;

		FreezePointer m_freezePointer;

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

		bool m_drawing;
		void queue_draw ()
		{
			if (m_drawing) {
				return;
			}
			m_deferredDraw.draw();
		}
		void draw ();

		static void captureStates ()
		{
			m_state_select1 = GlobalShaderCache().capture("$CAM_HIGHLIGHT");
			m_state_select2 = GlobalShaderCache().capture("$CAM_OVERLAY");
		}
		static void releaseStates ()
		{
			GlobalShaderCache().release("$CAM_HIGHLIGHT");
			GlobalShaderCache().release("$CAM_OVERLAY");
		}

		camera_t& getCamera ()
		{
			return m_Camera;
		}

		void BenchMark ();
		void Cam_ChangeFloor (bool up);

		void DisableFreeMove ();
		void EnableFreeMove ();
		bool m_bFreeMove;

		CameraView& getCameraView ()
		{
			return m_cameraview;
		}

	private:
		void Cam_Draw ();
};

typedef MemberCaller<CamWnd, &CamWnd::queue_draw> CamWndQueueDraw;

#endif /* CAMWND_H_ */
