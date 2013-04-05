#pragma once

#include <list>

class CameraObserver
{
	public:
		virtual ~CameraObserver() {}

		// This gets called as soon as the camera is moved
		virtual void cameraMoved () = 0;

}; // class CameraObserver

typedef std::list<CameraObserver*> CameraObserverList;
