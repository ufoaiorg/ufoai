uniform sampler2D SAMPLER0;
uniform float COEFFICIENTS[3];
uniform vec2 OFFSETS[3];

/* fragment shader that convolves a 5 element filter with
 * the specified texture.
 * Orientation of the filter is controlled by "OFFSETS".
 * The filter itself is specified by "COEFFICIENTS". */
void main(void){

	vec2 inColor = gl_TexCoord[0].st;
	vec4 outColor = vec4(0, 0, 0, 1);

	outColor += COEFFICIENTS[0] * texture2D(SAMPLER0, inColor + OFFSETS[0]);
	outColor += COEFFICIENTS[1] * texture2D(SAMPLER0, inColor + OFFSETS[1]);
	outColor += COEFFICIENTS[2] * texture2D(SAMPLER0, inColor + OFFSETS[2]);

	gl_FragColor = outColor;
}
