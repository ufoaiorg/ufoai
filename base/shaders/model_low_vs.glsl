/**
 * @file
 * @brief Low quality battlescape model vertex shader.
 */

uniform float OFFSET;
uniform int ANIMATE;

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

#include "lerp_vs.glsl"
#include "light_vs.glsl"
#include "transform_lights_vs.glsl"
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
		Tangent = TANGENTS;
	}

	/* MVP transform into clip space.*/
	gl_Position = ftransform();
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(Vertex);


	/* Pass texture coordinate through.*/
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;

	LightVertex();

	TransformLights();


#if r_fog
	FogVertex();
#endif
}
