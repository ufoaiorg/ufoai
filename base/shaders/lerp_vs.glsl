/**
 * @file lerp_vs.glsl
 * @brief Lerp vertex shader.
 */

in_qualifier vec3 NEXT_FRAME_VERTS;
in_qualifier vec3 NEXT_FRAME_NORMALS;
in_qualifier vec4 TANGENTS;
in_qualifier vec4 NEXT_FRAME_TANGENTS;
uniform float TIME;

vec4 Vertex;
vec3 Normal;
vec4 Tangent;

void lerpVertex(void) {
	Vertex = mix(vec4(NEXT_FRAME_VERTS, 1.0), gl_Vertex, TIME);
	Normal = mix(NEXT_FRAME_NORMALS, gl_Normal, TIME);
	Tangent = mix(NEXT_FRAME_TANGENTS, TANGENTS, TIME);
}
