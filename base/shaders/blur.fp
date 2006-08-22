!!ARBfp1.0
OPTION ARB_precision_hint_fastest;
PARAM  texCoord00 = program.local[0];
PARAM  texCoord01 = program.local[1];
PARAM  texCoord02 = program.local[2];
PARAM  texCoord10 = program.local[3];
PARAM  texCoord12 = program.local[5];
PARAM  texCoord20 = program.local[6];
PARAM  texCoord21 = program.local[7];
PARAM  texCoord22 = program.local[8];

PARAM  const00 = {0.0769, 0.0769, 0.0769, 0.0};
PARAM  const01 = {0.1538, 0.1538, 0.1538, 0.0};
PARAM  const02 = {0.0769, 0.0769, 0.0769, 0.0};
PARAM  const10 = {0.1538, 0.1538, 0.1538, 0.0};
PARAM  const11 = {0.0769, 0.0769, 0.0769, 0.0};
PARAM  const12 = {0.1538, 0.1538, 0.1538, 0.0};
PARAM  const20 = {0.0769, 0.0769, 0.0769, 0.0};
PARAM  const21 = {0.1538, 0.1538, 0.1538, 0.0};
PARAM  const22 = {0.0769, 0.0769, 0.0769, 0.0};

TEMP   finalPixel;
TEMP   coord00;
TEMP   coord01;
TEMP   coord02;
TEMP   coord10;
TEMP   coord11;
TEMP   coord12;
TEMP   coord20;
TEMP   coord21;
TEMP   coord22;

OUTPUT oColor = result.color;


ADD coord00, texCoord00, fragment.texcoord[0];
ADD coord01, texCoord01, fragment.texcoord[0];
ADD coord02, texCoord02, fragment.texcoord[0];
ADD coord10, texCoord10, fragment.texcoord[0];
ADD coord12, texCoord12, fragment.texcoord[0];
ADD coord20, texCoord20, fragment.texcoord[0];
ADD coord21, texCoord21, fragment.texcoord[0];
ADD coord22, texCoord22, fragment.texcoord[0];


TEX coord00, coord00, texture[0], 2D;
TEX coord01, coord01, texture[0], 2D;
TEX coord02, coord02, texture[0], 2D;
TEX coord10, coord10, texture[0], 2D;
TEX coord11, fragment.texcoord[0], texture[0], 2D;
TEX coord12, coord12, texture[0], 2D;
TEX coord20, coord20, texture[0], 2D;
TEX coord21, coord21, texture[0], 2D;
TEX coord22, coord22, texture[0], 2D;


MUL finalPixel, coord00, const00;
MAD finalPixel, coord10, const10, finalPixel;
MAD finalPixel, coord20, const20, finalPixel;
MAD finalPixel, coord01, const01, finalPixel;
MAD finalPixel, coord11, const11, finalPixel;
MAD finalPixel, coord21, const21, finalPixel;
MAD finalPixel, coord02, const02, finalPixel;
MAD finalPixel, coord12, const12, finalPixel;
MAD oColor, coord22, const22, finalPixel;
END
