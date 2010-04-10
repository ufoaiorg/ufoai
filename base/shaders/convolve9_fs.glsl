uniform sampler2D SAMPLER0;
uniform float coefficients[9];
uniform vec2 offsets[9];

/* fragment shader that convolves a 9 element filter with the specified texture.  
 * Orientation of the filter is controlled by "offsets".
 * The filter itself is specified by "coefficients".
 */
void main(void)
{
    vec2 inColor = gl_TexCoord[0].st;
    vec4 outColor = vec4(0, 0, 0, 0);

    outColor += coefficients[0] * texture2D(SAMPLER0, inColor + offsets[0]);
    outColor += coefficients[1] * texture2D(SAMPLER0, inColor + offsets[1]);
    outColor += coefficients[2] * texture2D(SAMPLER0, inColor + offsets[2]);
    outColor += coefficients[3] * texture2D(SAMPLER0, inColor + offsets[3]);
    outColor += coefficients[4] * texture2D(SAMPLER0, inColor + offsets[4]);
    outColor += coefficients[5] * texture2D(SAMPLER0, inColor + offsets[5]);
    outColor += coefficients[6] * texture2D(SAMPLER0, inColor + offsets[6]);
    outColor += coefficients[7] * texture2D(SAMPLER0, inColor + offsets[7]);
    outColor += coefficients[8] * texture2D(SAMPLER0, inColor + offsets[8]);

    gl_FragColor = outColor;
}
