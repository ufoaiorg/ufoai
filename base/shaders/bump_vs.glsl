// bumpmap vertex shader, requires light_vs.glsl

attribute vec4 TANGENT;

varying vec3 eyedir;


/*
 * BumpVertex
 */
void BumpVertex(void){

	vec3 n = normalize(gl_NormalMatrix * gl_Normal);
	vec3 t = normalize(gl_NormalMatrix * TANGENT.xyz);

	// compute the bitangent
	vec3 b = cross(n, t) * TANGENT.w;

	// transform the eye direction into tangent space
	vec3 v;
	v.x = dot(point, t);
	v.y = dot(point, b);
	v.z = dot(point, n);

	eyedir = -normalize(v);
}
