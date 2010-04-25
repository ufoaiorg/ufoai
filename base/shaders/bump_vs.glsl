// bumpmap vertex shader, requires light_vs.glsl

attribute vec4 TANGENT;
uniform int DYNAMICLIGHTS;

varying vec3 eyedir;
varying vec3 lightDirs[8];


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

	vec3 lpos;
	if(DYNAMICLIGHTS > 0) {
#unroll r_dynamic_lights
		lpos = gl_LightSource[$].position.rgb - point;
		lightDirs[$].x = dot(lpos.rgb, t);
		lightDirs[$].y = dot(lpos.rgb, b);
		lightDirs[$].z = dot(lpos.rgb, n);
#endunroll
	}
}
