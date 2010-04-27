// bumpmap vertex shader, requires light_vs.glsl

attribute vec4 TANGENT;
uniform int DYNAMICLIGHTS;

varying vec3 eyedir;
varying vec3 lightDirs[8];
varying vec3 staticLightDir;
varying vec3 tangent;

/*
 * BumpVertex
 */
void BumpVertex(void){
	// load the tangent
	tangent = normalize(gl_NormalMatrix * TANGENT.xyz);
	// compute the bitangent
	vec3 bitangent = normalize(cross(normal, tangent) * TANGENT.w);

	// transform the eye direction into tangent space
	vec3 v;
	v.x = dot(-point, tangent);
	v.y = dot(-point, bitangent);
	v.z = dot(-point, normal);

	eyedir = normalize(v);

	// transform relative light positions into tangent space
	vec3 lpos = normalize(lightpos - point);
	staticLightDir.x = dot(lpos, tangent);
	staticLightDir.y = dot(lpos, bitangent);
	staticLightDir.z = dot(lpos, normal);

	if(DYNAMICLIGHTS > 0) {
#unroll r_dynamic_lights
		lpos = gl_LightSource[$].position.rgb - point;
		lightDirs[$].x = dot(lpos, tangent);
		lightDirs[$].y = dot(lpos, bitangent);
		lightDirs[$].z = dot(lpos, normal);
#endunroll
	}
}
