// lighting vertex shader

varying vec3 point;
varying vec3 normal;


/*
LightVertex
*/
void LightVertex(void){

	// pass the interpolated normal and position along
	normal = normalize(gl_NormalMatrix * gl_Normal);
	point = vec3(gl_ModelViewMatrix * gl_Vertex);
}
