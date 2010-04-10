uniform sampler2D SAMPLER0;
uniform float coefficients[3];
uniform vec2 offsets[3];

/* fragment shader that convolves a 5 element filter with
 * the specified texture.  
 * Orientation of the filter is controlled by "offsets".
 * The filter itself is specified by "coefficients".
 */
void main(void)
{
    vec2 inColor = gl_TexCoord[0].st;
    vec4 outColor = vec4(0, 0, 0, 1);

    outColor += coefficients[0] * texture2D(SAMPLER0, inColor + offsets[0]);
    outColor += coefficients[1] * texture2D(SAMPLER0, inColor + offsets[1]);
    outColor += coefficients[2] * texture2D(SAMPLER0, inColor + offsets[2]);

    gl_FragColor = outColor;
}
