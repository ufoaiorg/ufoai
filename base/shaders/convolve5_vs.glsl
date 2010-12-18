/**
 * @file convolve5_vs.glsl
 * @brief Very simple vertex shader to pass along coordinates to a fragment shader.
 */

void main(void){

	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = ftransform();
}
