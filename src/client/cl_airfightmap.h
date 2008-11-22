/*
 * cl_airfightmap.h
 *
 *  Created on: Jul 30, 2008
 *      Author: stevenjackson
 */

#ifndef CL_AIRFIGHTMAP_H_
#define CL_AIRFIGHTMAP_H_

void AFM_Init_f(void);
void AFM_Exit_f(void);
void AFM_DrawMap(const menuNode_t* node);
qboolean AFM_MapToScreen(const menuNode_t* node, const vec2_t pos, int *x, int *y);
qboolean AFM_Draw3DMarkerIfVisible(const menuNode_t* node, const vec2_t pos, float theta, const char *model, int skin);
void AFM_StopSmoothMovement(void);


#endif /* CL_AIRFIGHTMAP_H_ */
