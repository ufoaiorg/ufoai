#include "CameraSettings.h"

#include "camwindow.h"
#include "CamWnd.h"
#include "GlobalCamera.h"
#include <iostream>

CameraSettings::CameraSettings() :
	_callbackActive(false),
	_movementSpeed(GlobalRegistry().getInt("user/ui/camera/movementSpeed")),
	_invertMouseVerticalAxis(GlobalRegistry().get("user/ui/camera/invertMouseVerticalAxis") == "1")
{
	initDiscreteMovement();
}

void CameraSettings::keyChanged() {
	std::cout << "Observer called\n";

	// Check for iterative loops
	if (_callbackActive) {
		return;
	}
	else {
		_callbackActive = true;

		// Call the setFarClip method to update the current far clip state
		Camera_SetFarClip( Camera_GetFarClip() );

		// Load the values from the registry
		_movementSpeed = GlobalRegistry().getInt("user/ui/camera/movementSpeed");
		_invertMouseVerticalAxis = (GlobalRegistry().get("user/ui/camera/invertMouseVerticalAxis") == "1");

		if (_invertMouseVerticalAxis) {
			std::cout << "invertAxis = true\n";
		}
		else {
			std::cout << "invertAxis = false\n";
		}

		// Check if a global camwindow is set
		if (GlobalCamera().getCamWnd() != 0) {
			// Disable discrete movement if it was active before
			if (_discreteMovement == true) {
				GlobalCamera().getCamWnd()->moveDiscreteDisable();
			}
			else {
				GlobalCamera().getCamWnd()->moveDisable();
			}

			// Check the value and take the according actions
			initDiscreteMovement();
		}

	}
	_callbackActive = false;
}

void CameraSettings::initDiscreteMovement() {
	_discreteMovement = (GlobalRegistry().get("user/ui/camera/discreteMovement") == "1");

	// If it is activated now, take the appropriate action
	if (_discreteMovement) {
		GlobalCamera().getCamWnd()->moveDiscreteEnable();
	}
	else {
		GlobalCamera().getCamWnd()->moveEnable();
	}
}

CameraSettings* getCameraSettings() {
	static CameraSettings _cameraSettings;
	return &_cameraSettings;
}
