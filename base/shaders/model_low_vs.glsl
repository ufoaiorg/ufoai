/**
 * @file model_low_vs.glsl
 * @brief Low quality battlescape model vertex shader.
 */

uniform float OFFSET;
uniform int ANIMATE;
uniform vec3 SUNDIRECTION;

/* from includes:
attribute vec4 NEXT_FRAME_VERTS;
attribute vec4 NEXT_FRAME_NORMALS;
attribute vec4 NEXT_FRAME_TANGENTS;
uniform float TIME;

vec4 Vertex;
vec3 Normal;
vec4 Tangent;

varying float fog;
*/

out_qualifier vec3 sunDir; /** < Direction towards the sun, in tangent space */

#include "lerp_vs.glsl"
#include "fog_vs.glsl"

/**
 * @brief Main.
 */
void main(void) {
	if (ANIMATE > 0) {
		lerpVertex();
	} else {
		Vertex = gl_Vertex;
		Normal = gl_Normal;
		Tangent = TANGENTS; /** @todo what if tangents are disabled? */
	}

	/* MVP transform into clip space.*/
	gl_Position = ftransform();
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(Vertex);


	/* Pass texture coordinate through.*/
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;

	/* construct tangent space */
	vec3 normal = normalize(gl_NormalMatrix * Normal);
	vec3 tangent = normalize(gl_NormalMatrix * Tangent.xyz);
	vec3 bitangent = normalize(cross(normal, tangent)) * Tangent.w;

	/* transform sun direction to tangent space */
	sunDir.x = dot(SUNDIRECTION, tangent);
	sunDir.y = dot(SUNDIRECTION, bitangent);
	sunDir.z = dot(SUNDIRECTION, normal);

#if r_fog
	FogVertex();
#endif
}
