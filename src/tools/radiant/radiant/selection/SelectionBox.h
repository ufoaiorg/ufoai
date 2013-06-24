#pragma once

#include "generic/callbackfwd.h"

struct SelectionRectangle {
  float min[2];
  float max[2];
};

typedef Callback1<SelectionRectangle> RectangleCallback;

// greebo: This returns the coordinates of a rectangle at the mouse position with edge length 2*epsilon
inline const SelectionRectangle SelectionBoxForPoint(const float device_point[2], const float device_epsilon[2]) {
  SelectionRectangle selection_box;
  selection_box.min[0] = device_point[0] - device_epsilon[0];
  selection_box.min[1] = device_point[1] - device_epsilon[1];
  selection_box.max[0] = device_point[0] + device_epsilon[0];
  selection_box.max[1] = device_point[1] + device_epsilon[1];
  return selection_box;
}

/* greebo: Returns the coordinates of the selected rectangle,
 * it is assured that the min values are smaller than the max values */
inline const SelectionRectangle SelectionBoxForArea(const float device_point[2], const float device_delta[2]) {
  SelectionRectangle selection_box;
  selection_box.min[0] = (device_delta[0] < 0) ? (device_point[0] + device_delta[0]) : (device_point[0]);
  selection_box.min[1] = (device_delta[1] < 0) ? (device_point[1] + device_delta[1]) : (device_point[1]);
  selection_box.max[0] = (device_delta[0] > 0) ? (device_point[0] + device_delta[0]) : (device_point[0]);
  selection_box.max[1] = (device_delta[1] > 0) ? (device_point[1] + device_delta[1]) : (device_point[1]);
  return selection_box;
}
