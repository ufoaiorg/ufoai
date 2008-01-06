!!ARBfp1.0
PARAM scale = { 3.5, 3.5, 3.5, 1.0};
PARAM base = program.local[0];
TEMP diffuse, temp;
TEX diffuse, fragment.texcoord[0], texture[0], 2D;
MUL temp, scale, fragment.color;
ADD temp, base, temp;
MUL result.color, diffuse, temp;
END
