/**
 * @file convolve3_vs.glsl
 * @brief Very simple vertex shader to pass along coordinates to a fragment shader.
 */

void main(void) {
	gl_TexCoord[0] = gl_MultiTexCoord0;
#ifdef glsl110
	gl_Position = ftransform();
#endif
}
