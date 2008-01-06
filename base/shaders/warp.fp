!!ARBfp1.0
PARAM offset = program.local[0];
TEMP warp, coord;
ADD warp.x, fragment.texcoord[0], offset.x;
ADD warp.y, fragment.texcoord[0], offset.y;
TEX warp, warp, texture[1], 2D;
ADD coord.x, fragment.texcoord[0].x, warp.z;
ADD coord.y, fragment.texcoord[0].y, warp.w;
TEX warp, coord, texture[0], 2D;
MUL result.color, warp, fragment.color;
END
