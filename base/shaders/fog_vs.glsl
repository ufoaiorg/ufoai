/**
 * @file
 * @brief Fog vertex shader.
 */

#if r_fog
uniform float FOGDENSITY;
uniform vec2 FOGRANGE; /* start, end */

out_qualifier float fog;

/**
 * @brief FogVertex.
 */
void FogVertex(void) {
	/* Calculate interpolated fog depth.*/
	float fogT = (gl_Position.z - FOGRANGE[0]) / (FOGRANGE[1] - FOGRANGE[0]);
	fog = clamp(fogT, 0.0, 1.0) * FOGDENSITY;
}
#endif
