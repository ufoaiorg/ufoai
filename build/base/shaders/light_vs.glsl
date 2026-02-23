/**
 * @file
 * @brief Lighting vertex shader.
 */

vec3 point;
out_qualifier vec3 normal;

/**
 * @brief LightVertex.
 */
void LightVertex(void) {
	/* Pass the interpolated normal and position along for dynamic lights.*/
	normal = normalize(vec3(gl_NormalMatrix * Normal));
	point = vec3(gl_ModelViewMatrix * Vertex);
}
