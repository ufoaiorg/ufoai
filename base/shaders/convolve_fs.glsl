#version 110

uniform sampler2D SAMPLER0;
uniform float COEFFICIENTS[#replace r_convolve_filter_width ];
uniform vec2 OFFSETS[#replace r_convolve_filter_width ];

/* fragment shader that convolves a filter with
 * the specified texture.  
 * Orientation of the filter is controlled by "OFFSETS".
 * The filter itself is specified by "COEFFICIENTS". */
void main(void) {

	vec2 inColor = gl_TexCoord[0].st;
	//vec4 outColor = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 outColor = vec4(0.0);

#unroll r_convolve_filter_width
	outColor += COEFFICIENTS[$] * texture2D(SAMPLER0, inColor + OFFSETS[$]);
#endunroll

	gl_FragColor = outColor;
}
