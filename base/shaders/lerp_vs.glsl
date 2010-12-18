/**
 * @file lerp_vs.glsl
 * @brief Lerp vertex shader.
 */

attribute vec3 NEXT_FRAME_VERTS;
attribute vec3 NEXT_FRAME_NORMALS;
attribute vec4 TANGENTS;
attribute vec4 NEXT_FRAME_TANGENTS;
uniform float TIME;

varying vec4 Vertex;
varying vec3 Normal;
varying vec4 Tangent;

void lerpVertex(void) {
	Vertex = mix(vec4(NEXT_FRAME_VERTS, 1.0), gl_Vertex, TIME);
	Normal = mix(NEXT_FRAME_NORMALS, gl_Normal, TIME);
	Tangent = mix(NEXT_FRAME_TANGENTS, TANGENTS, TIME);
}
