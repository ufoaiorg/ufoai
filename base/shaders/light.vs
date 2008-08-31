// lighting vertex shader

varying vec3 vertex;
varying vec3 normal;


/*
LightVertex
*/
void LightVertex(void){

	// output lerped normal and position for fragment shader
	normal = normalize(gl_NormalMatrix * gl_Normal);
	vertex = vec3(gl_ModelViewMatrix * gl_Vertex);
}
