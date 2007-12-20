!!ARBfp1.0
TEMP offset, coord, dist;
TEX offset, fragment.texcoord[1], texture[1], 2D;
ADD coord.x, fragment.texcoord[0].x, offset.z;
ADD coord.y, fragment.texcoord[0].y, offset.w;
TEX dist, coord, texture[0], 2D;
MUL result.color, dist, fragment.color;
END
