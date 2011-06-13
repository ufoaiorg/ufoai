/**
 * @file warp_vs.glsl
 * @brief Warp vertex shader.
 */

#include "fog_vs.glsl"


/**
 * @brief Main.
 */
void main(void) {
#ifdef glsl110
	/* MVP transform into clip space.*/
	gl_Position = ftransform();
#endif

	/* Pass texcoords through, */
	gl_TexCoord[0] = gl_MultiTexCoord0;

	/*  and primary color.*/
	gl_FrontColor = gl_Color;

#if r_fog
	FogVertex();
#endif
}
