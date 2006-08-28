!!ARBfp1.0
OPTION ARB_precision_hint_fastest;
PARAM c[1] = { { 2.75 } };
TEMP R0;
TEX R0.zw, fragment.texcoord[1], texture[1], 2D;
MUL R0.xy, -R0.zwzw, fragment.color.primary.x;
ADD R0.xy, R0, fragment.texcoord[0];
TEX R0, R0, texture[0], 2D;
MUL R0, R0, fragment.color.primary;
MUL result.color, R0, c[0].x;
END