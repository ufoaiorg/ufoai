/**
 * @file transform_lights_vs.glsl
 * @brief Transform lights to the tangent space. Requires light_vs.glsl and lerp_vs.glsl.
 */

uniform vec3 SUNDIRECTION;
out_qualifier vec3 sunDir; /** < Direction towards the sun, in tangent space */

out_qualifier vec3 eyedir;

#define R_DYNAMIC_LIGHTS #replace r_dynamic_lights
#if r_dynamic_lights
out_qualifier vec3 lightDirs[R_DYNAMIC_LIGHTS];
out_qualifier vec4 lightParams[R_DYNAMIC_LIGHTS];
#endif

/**
 * @brief TransformLights
 */
void TransformLights(void) {
	/* Load the tangent.*/
	vec3 tangent = normalize(gl_NormalMatrix * Tangent.xyz);
	/* Compute the bitangent.*/
	vec3 bitangent = normalize(cross(normal, tangent)) * Tangent.w;

	/* Transform the eye direction into tangent space.*/
	vec3 v;
	v.x = dot(-point, tangent);
	v.y = dot(-point, bitangent);
	v.z = dot(-point, normal);

	eyedir = normalize(v);

	/* transform sun direction to tangent space */
	sunDir.x = dot(SUNDIRECTION, tangent);
	sunDir.y = dot(SUNDIRECTION, bitangent);
	sunDir.z = dot(SUNDIRECTION, normal);

	/* Transform relative light positions into tangent space.*/
	vec3 lpos;
#unroll r_dynamic_lights
	lpos = gl_LightSource[$].position.xyz - point;

	lightDirs[$].x = dot(lpos, tangent);
	lightDirs[$].y = dot(lpos, bitangent);
	lightDirs[$].z = dot(lpos, normal);
	lightParams[$].rgb = gl_LightSource[$].diffuse.rgb;
	lightParams[$].a = gl_LightSource[$].quadraticAttenuation;
#endunroll
}
