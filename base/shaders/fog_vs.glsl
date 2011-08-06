/**
 * @file fog_vs.glsl
 * @brief Fog vertex shader.
 */

uniform float FOGDENSITY;
uniform vec2 FOGRANGE; /* start, end */

out_qualifier float fog;

/**
 * @brief FogVertex.
 */
void FogVertex(void) {
	/* Calculate interpolated fog depth.*/
	fog = (gl_Position.z - FOGRANGE[0]) / (FOGRANGE[1] - FOGRANGE[0]);
	fog = clamp(fog, 0.0, 1.0) * FOGDENSITY;
}
