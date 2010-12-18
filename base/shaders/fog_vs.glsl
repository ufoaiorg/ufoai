/**
 * @file fog_vs.glsl
 * @brief Fog vertex shader.
 */

varying float fog;

/**
 * @brief FogVertex.
 */
void FogVertex(void){

	/* Calculate interpolated fog depth.*/
	fog = (gl_Position.z - gl_Fog.start) / (gl_Fog.end - gl_Fog.start);
	fog = clamp(fog, 0.0, 1.0) * gl_Fog.density;
}
