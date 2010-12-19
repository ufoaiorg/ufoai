/**
 * @file fog_vs.glsl
 * @brief Fog vertex shader.
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

out_qualifier float fog;

/**
 * @brief FogVertex.
 */
void FogVertex(void){

	/* Calculate interpolated fog depth.*/
	fog = (gl_Position.z - gl_Fog.start) / (gl_Fog.end - gl_Fog.start);
	fog = clamp(fog, 0.0, 1.0) * gl_Fog.density;
}
