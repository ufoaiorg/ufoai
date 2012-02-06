/**
 * @file world_vs.glsl
 * @brief Default battlescape vertex shader.
 */

uniform float OFFSET;
uniform int BUMPMAP;

in_qualifier vec4 TANGENTS;

/* from includes:
out_qualifier vec3 point;
out_qualifier vec3 normal;

out_qualifier vec3 eyedir;
out_qualifier vec3 lightDirs[];
out_qualifier vec4 lightParams[R_DYNAMIC_LIGHTS];

varying float fog;
*/

vec4 Vertex;
vec3 Normal;
vec4 Tangent;

#include "light_vs.glsl"
#include "transform_lights_vs.glsl"
#include "fog_vs.glsl"

/**
 * @brief Main.
 */
void main(void) {
	Vertex = gl_Vertex;
	Normal = gl_Normal;
	Tangent = TANGENTS; /** @todo what if tangents are disabled? */

	/* MVP transform into clip space.*/
	gl_Position = ftransform();
	/*gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(Vertex);*/


	/* Pass texcoords through.*/
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;
	gl_TexCoord[1] = gl_MultiTexCoord1 + OFFSET;

	LightVertex();

	TransformLights();

#if r_fog
	FogVertex();
#endif
}
