!!ARBfp1.0
OPTION ARB_fog_linear;
PARAM scale = { 3.5, 3.5, 3.5, 1.0};
TEMP diffuse, lightmap, temp;
TEX diffuse, fragment.texcoord[0], texture[0], 2D;
TEX lightmap, fragment.texcoord[1], texture[1], 2D;
MUL temp, scale, fragment.color;
ADD lightmap, lightmap, temp;
MUL result.color, diffuse, lightmap;
END
