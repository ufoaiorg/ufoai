/**
 * @file light_vs.glsl
 * @brief Lighting vertex shader.
 */

out_qualifier vec3 point;
out_qualifier vec3 normal;
out_qualifier vec3 ambientLight;

/**
 * @brief LightVertex.
 */
void LightVertex(void) {
	/* Pass the interpolated normal and position along for dynamic lights.*/
	normal = normalize(gl_NormalMatrix * Normal);
	point = vec3(gl_ModelViewMatrix * Vertex);

	/* Pass the color through as well.*/
	gl_FrontColor = gl_Color;

	/* To avoid GL light settings being read in fragment shader */
	ambientLight = gl_LightSource[0].ambient.rgb; /* All ambient lighting is passed through light #0, which is the only global light (sun, etc) */
}
