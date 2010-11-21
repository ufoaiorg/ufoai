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

#if !defined(INCLUDED_ICAMERA_H)
#define INCLUDED_ICAMERA_H

#include "math/Vector3.h"
#include "scenelib.h"
enum
{
	CAMERA_PITCH = 0, // up / down
	CAMERA_YAW = 1, // left / right
	CAMERA_ROLL = 2
// fall over
};

/**
 * The "global" interface of DarkRadiant's camera module.
 */
class ICamera
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "camera");

		virtual ~ICamera ()
		{
		}

		/**
		 * greebo: Sets the camera origin to the given <point> using the given <angles>.
		 */
		virtual void focusCamera (const Vector3& point, const Vector3& angles) = 0;
};

class Matrix4;

class CameraView
{
	public:
		virtual ~CameraView ()
		{
		}
		virtual void setModelview (const Matrix4& modelview) = 0;
		virtual void setFieldOfView (float fieldOfView) = 0;
};

class CameraModel
{
	public:
		STRING_CONSTANT(Name, "CameraModel");
		virtual ~CameraModel ()
		{
		}
		virtual void setCameraView (CameraView* view, const Callback& disconnect) = 0;
};

inline CameraModel* Instance_getCameraModel (scene::Instance& instance)
{
	return dynamic_cast<CameraModel*>(&instance);
}

// Module definitions

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<ICamera> GlobalCameraModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<ICamera> GlobalCameraModuleRef;

// This is the accessor for the registry
inline ICamera& GlobalCameraView ()
{
	return GlobalCameraModule::getTable();
}

#endif
