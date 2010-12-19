/**
 * @file light_vs.glsl
 * @brief Lighting vertex shader.
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

out_qualifier vec3 point;
out_qualifier vec3 normal;

/**
 * @brief LightVertex.
 */
void LightVertex(void){

	/* Pass the interpolated normal and position along for dynamic lights.*/
	normal = normalize(gl_NormalMatrix * Normal);
	point = vec3(gl_ModelViewMatrix * Vertex);

	/* Pass the color through as well.*/
	gl_FrontColor = gl_Color;
}
