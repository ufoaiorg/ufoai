#ifndef CAMERA_H_
#define CAMERA_H_

enum camera_draw_mode
{
	cd_wire, cd_solid, cd_texture, cd_lighting
};

struct camera_t
{
		int width, height;

		bool timing;

		Vector3 origin;
		Vector3 angles;

		Vector3 color; // background

		Vector3 forward, right; // move matrix (TTimo: used to have up but it was not updated)
		Vector3 vup, vpn, vright; // view matrix (taken from the modelview matrix)

		Matrix4 projection;
		Matrix4 modelview;

		bool m_strafe; // true when in strafemode toggled by the ctrl-key
		bool m_strafe_forward; // true when in strafemode by ctrl-key and shift is pressed for forward strafing

		unsigned int movementflags; // movement flags
		Timer m_keycontrol_timer;
		guint m_keymove_handler;

		float fieldOfView;

		DeferredMotionDelta m_mouseMove;

		static void motionDelta (int x, int y, void* data)
		{
			Camera_mouseMove(*reinterpret_cast<camera_t*> (data), x, y);
		}

		View* m_view;
		Callback m_update;

		static camera_draw_mode draw_mode;

		camera_t (View* view, const Callback& update) :
			width(0), height(0), timing(false), origin(0, 0, 0), angles(0, 0, 0), color(0, 0, 0), projection(
					Matrix4::getIdentity()), modelview(Matrix4::getIdentity()), movementflags(0), m_keymove_handler(0),
					fieldOfView(90.0f), m_mouseMove(motionDelta, this), m_view(view), m_update(update)
		{
		}
};

#endif /* CAMERA_H_ */
