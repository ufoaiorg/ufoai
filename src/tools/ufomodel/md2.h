#pragma once

#include "../../shared/ufotypes.h"
#include "../../common/qfiles.h"

void MD2SkinEdit (const byte *buf, const char *fileName, int bufSize, void *userData);
void MD2Info (const byte *buf, const char *fileName, int bufSize, void *userData);
void MD2SkinNum (const byte *buf, const char *fileName, int bufSize, void *userData);
void MD2GLCmdsRemove (const byte *buf, const char *fileName, int bufSize, void *userData);
void MD2HeaderCheck (const dMD2Model_t *md2, const char *fileName, int bufSize);
