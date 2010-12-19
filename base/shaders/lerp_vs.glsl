/**
 * @file lerp_vs.glsl
 * @brief Lerp vertex shader.
 */

#ifndef glsl110
        /** Linkage into a shader from a previous stage, variable is copied in.*/
        #define in_qualifier in
        /** Linkage out of a shader to a subsequent stage, variable is copied out.*/
        #define out_qualifier out
#else
        /** Deprecated after glsl110; linkage between a vertex shader and OpenGL for per-vertex data.*/
        #define in_qualifier attribute
        /** Deprecated after glsl110; linkage between a vertex shader and a fragment shader for interpolated data.*/
        #define out_qualifier varying
#endif

in_qualifier vec3 NEXT_FRAME_VERTS;
in_qualifier vec3 NEXT_FRAME_NORMALS;
in_qualifier vec4 TANGENTS;
in_qualifier vec4 NEXT_FRAME_TANGENTS;
uniform float TIME;

out_qualifier vec4 Vertex;
out_qualifier vec3 Normal;
out_qualifier vec4 Tangent;

void lerpVertex(void) {
	Vertex = mix(vec4(NEXT_FRAME_VERTS, 1.0), gl_Vertex, TIME);
	Normal = mix(NEXT_FRAME_NORMALS, gl_Normal, TIME);
	Tangent = mix(NEXT_FRAME_TANGENTS, TANGENTS, TIME);
}
