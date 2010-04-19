uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;

void main(void)
{
    gl_FragData[0] = texture2D(SAMPLER0, gl_TexCoord[0].st);
    gl_FragData[1] = texture2D(SAMPLER1, gl_TexCoord[0].st);
}
