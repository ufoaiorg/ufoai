// lighting vertex shader

varying vec3 point;
varying vec3 normal;

/*
 * LightVertex
 */
void LightVertex(void){

	// pass the interpolated normal and position along for dynamic lights
	normal = normalize(gl_NormalMatrix * Normal);
	point = vec3(gl_ModelViewMatrix * Vertex);

	// pass the color through as well
	gl_FrontColor = gl_Color;
}
