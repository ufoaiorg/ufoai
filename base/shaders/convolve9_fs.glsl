/**
 * @file convolve9_fs.glsl
 * @brief convolve9 fragment shader.
 */

uniform sampler2D SAMPLER0;
uniform float COEFFICIENTS[9];
uniform vec2 OFFSETS[9];

/**
 * @brief Fragment shader that convolves a 9 element filter with the specified texture.
 *
 * Orientation of the filter is controlled by "OFFSETS".
 * The filter itself is specified by "COEFFICIENTS".
 */
void main(void){

	vec2 inColor = gl_TexCoord[0].st;
	vec4 outColor = vec4(0, 0, 0, 0);

	outColor += COEFFICIENTS[0] * texture2D(SAMPLER0, inColor + OFFSETS[0]);
	outColor += COEFFICIENTS[1] * texture2D(SAMPLER0, inColor + OFFSETS[1]);
	outColor += COEFFICIENTS[2] * texture2D(SAMPLER0, inColor + OFFSETS[2]);
	outColor += COEFFICIENTS[3] * texture2D(SAMPLER0, inColor + OFFSETS[3]);
	outColor += COEFFICIENTS[4] * texture2D(SAMPLER0, inColor + OFFSETS[4]);
	outColor += COEFFICIENTS[5] * texture2D(SAMPLER0, inColor + OFFSETS[5]);
	outColor += COEFFICIENTS[6] * texture2D(SAMPLER0, inColor + OFFSETS[6]);
	outColor += COEFFICIENTS[7] * texture2D(SAMPLER0, inColor + OFFSETS[7]);
	outColor += COEFFICIENTS[8] * texture2D(SAMPLER0, inColor + OFFSETS[8]);

	gl_FragColor = outColor;
}
