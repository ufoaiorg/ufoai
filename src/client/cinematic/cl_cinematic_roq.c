/**
 * @file cl_cinematic_roq.c
 * @note This code based on the OverDose and KMQuakeII source code
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


#include "cl_cinematic_roq.h"
#include "cl_cinematic.h"
#include "../client.h"	/* needed by r_draw.h */
#include "../renderer/r_draw.h"
#include "../sound/s_main.h"
#include "../sound/s_music.h"

typedef struct {
	int vr[256];
	int	ug[256];
	int	vg[256];
	int	ub[256];
} yuvTable_t;

#define ROQ_IDENT					0x1084

#define ROQ_QUAD_INFO				0x1001
#define ROQ_QUAD_CODEBOOK			0x1002
#define ROQ_QUAD_VQ					0x1011
#define ROQ_SOUND_MONO				0x1020
#define ROQ_SOUND_STEREO			0x1021

#define ROQ_CHUNK_HEADER_SIZE		8			/* Size of a RoQ chunk header */

#define ROQ_MAX_CHUNK_SIZE			65536		/* Max size of a RoQ chunk */

#define ROQ_SOUND_RATE				22050

#define ROQ_ID_FCC 0x4000
#define ROQ_ID_SLD 0x8000
#define ROQ_ID_CCC 0xC000

typedef struct {
	uint16_t id;
	uint32_t size;
	uint16_t flags;
} roqChunk_t;

typedef struct {
	uint32_t pixel[4];
} roqQuadVector_t;

typedef struct {
	byte			index[4];
} roqQuadCell_t;

typedef struct {
	qFILE			file;
	int				size;
	int				offset;

	int				frameWidth;
	int				frameHeight;
	int				frameRate;
	byte *			frameBuffer[2];

	int				startTime;	/**< cinematic start timestamp */
	int				currentFrame;

	int				soundChannel;	/**< the mixer channel the sound is played on */

	byte			data[ROQ_MAX_CHUNK_SIZE + ROQ_CHUNK_HEADER_SIZE];
	byte *			header;

	roqChunk_t		chunk;
	roqQuadVector_t	quadVectors[256];
	roqQuadCell_t	quadCells[256];

	musicStream_t	musicStream;
} roqCinematic_t;

static short		roqCin_sqrTable[256];
static yuvTable_t	roqCin_yuvTable;

static int			roqCin_quadOffsets2[2][4];
static int			roqCin_quadOffsets4[2][4];

#define ROQCIN (*((roqCinematic_t*)cin->codecData))

/**
 * @brief Clamps integer value into byte
 */
static inline byte CIN_ROQ_ClampByte (int value)
{
	if (value < 0)
		return 0;

	if (value > 255)
		return 255;

	return value;
}

/**
 * @sa CIN_ROQ_DecodeVideo
 */
static void CIN_ROQ_ApplyVector2x2 (cinematic_t *cin, int x, int y, const byte *indices)
{
	int i;

	for (i = 0; i < 4; i++) {
		const int xp = x + roqCin_quadOffsets2[0][i];
		const int yp = y + roqCin_quadOffsets2[1][i];
		const unsigned int *src = (const unsigned int *)ROQCIN.quadVectors + (indices[i] * 4);
		unsigned int *dst = (unsigned int *)ROQCIN.frameBuffer[0] + (yp * ROQCIN.frameWidth + xp);

		dst[0] = src[0];
		dst[1] = src[1];

		dst += ROQCIN.frameWidth;

		dst[0] = src[2];
		dst[1] = src[3];
	}
}

/**
 * @sa CIN_ROQ_DecodeVideo
 */
static void CIN_ROQ_ApplyVector4x4 (cinematic_t *cin, int x, int y, const byte *indices)
{
	int i;

	for (i = 0; i < 4; i++) {
		const int xp = x + roqCin_quadOffsets4[0][i];
		const int yp = y + roqCin_quadOffsets4[1][i];
		const unsigned int *src = (const unsigned int *)ROQCIN.quadVectors + (indices[i] * 4);
		unsigned int *dst = (unsigned int *)ROQCIN.frameBuffer[0] + (yp * ROQCIN.frameWidth + xp);

		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[1];
		dst[3] = src[1];

		dst += ROQCIN.frameWidth;

		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[1];
		dst[3] = src[1];

		dst += ROQCIN.frameWidth;

		dst[0] = src[2];
		dst[1] = src[2];
		dst[2] = src[3];
		dst[3] = src[3];

		dst += ROQCIN.frameWidth;

		dst[0] = src[2];
		dst[1] = src[2];
		dst[2] = src[3];
		dst[3] = src[3];
	}
}

/**
 * @sa CIN_ROQ_DecodeVideo
 */
static void CIN_ROQ_ApplyMotion4x4 (cinematic_t *cin, int x, int y, int mx, int my, int mv)
{
	int i;
	const int xp = x + 8 - (mv >> 4) - mx;
	const int yp = y + 8 - (mv & 15) - my;
	const unsigned int *src = (const unsigned int *)ROQCIN.frameBuffer[1] + (yp * ROQCIN.frameWidth + xp);
	unsigned int *dst = (unsigned int *)ROQCIN.frameBuffer[0] + (y * ROQCIN.frameWidth + x);

	for (i = 0; i < 4; i++, src += ROQCIN.frameWidth, dst += ROQCIN.frameWidth) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
	}
}

/**
 * @sa CIN_ROQ_DecodeVideo
 */
static void CIN_ROQ_ApplyMotion8x8 (cinematic_t *cin, int x, int y, int mx, int my, int mv)
{
	int i;
	const int xp = x + 8 - (mv >> 4) - mx;
	const int yp = y + 8 - (mv & 15) - my;
	const unsigned int *src = (const unsigned int *)ROQCIN.frameBuffer[1] + (yp * ROQCIN.frameWidth + xp);
	unsigned int *dst = (unsigned int *)ROQCIN.frameBuffer[0] + (y * ROQCIN.frameWidth + x);

	for (i = 0; i < 8; i++, src += ROQCIN.frameWidth, dst += ROQCIN.frameWidth) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		dst[4] = src[4];
		dst[5] = src[5];
		dst[6] = src[6];
		dst[7] = src[7];
	}
}

/**
 * @sa CIN_ROQ_DecodeChunk
 */
static void CIN_ROQ_DecodeInfo (cinematic_t *cin, const byte *data)
{
	if (ROQCIN.frameBuffer[0] && ROQCIN.frameBuffer[1])
		return;		/* Already allocated */

	/* Allocate the frame buffer */
	ROQCIN.frameWidth = data[0] | (data[1] << 8);
	ROQCIN.frameHeight = data[2] | (data[3] << 8);

	ROQCIN.frameWidth = LittleShort(ROQCIN.frameWidth);
	ROQCIN.frameHeight = LittleShort(ROQCIN.frameHeight);

	if (!Q_IsPowerOfTwo(ROQCIN.frameWidth) || !Q_IsPowerOfTwo(ROQCIN.frameHeight))
		Com_Error(ERR_DROP, "CIN_DecodeInfo: size is not a power of two (%i x %i)", ROQCIN.frameWidth, ROQCIN.frameHeight);

	ROQCIN.frameBuffer[0] = (byte *)Mem_PoolAlloc(ROQCIN.frameWidth * ROQCIN.frameHeight * 4, cl_genericPool, 0);
	ROQCIN.frameBuffer[1] = (byte *)Mem_PoolAlloc(ROQCIN.frameWidth * ROQCIN.frameHeight * 4, cl_genericPool, 0);
}

/**
 * @sa CIN_ROQ_DecodeChunk
 */
static void CIN_ROQ_DecodeCodeBook (cinematic_t *cin, const byte *data)
{
	int		numQuadVectors, numQuadCells;
	int		i;

	if (ROQCIN.chunk.flags) {
		numQuadVectors = (ROQCIN.chunk.flags >> 8) & 0xFF;
		numQuadCells = (ROQCIN.chunk.flags >> 0) & 0xFF;

		if (!numQuadVectors)
			numQuadVectors = 256;
	} else {
		numQuadVectors = 256;
		numQuadCells = 256;
	}

	/* Decode YUV quad vectors to RGB */
	for (i = 0; i < numQuadVectors; i++) {
		const int r = roqCin_yuvTable.vr[data[5]];
		const int g = roqCin_yuvTable.ug[data[4]] + roqCin_yuvTable.vg[data[5]];
		const int b = roqCin_yuvTable.ub[data[4]];

		((byte *)&ROQCIN.quadVectors[i].pixel[0])[0] = CIN_ROQ_ClampByte(data[0] + r);
		((byte *)&ROQCIN.quadVectors[i].pixel[0])[1] = CIN_ROQ_ClampByte(data[0] - g);
		((byte *)&ROQCIN.quadVectors[i].pixel[0])[2] = CIN_ROQ_ClampByte(data[0] + b);
		((byte *)&ROQCIN.quadVectors[i].pixel[0])[3] = 255;

		((byte *)&ROQCIN.quadVectors[i].pixel[1])[0] = CIN_ROQ_ClampByte(data[1] + r);
		((byte *)&ROQCIN.quadVectors[i].pixel[1])[1] = CIN_ROQ_ClampByte(data[1] - g);
		((byte *)&ROQCIN.quadVectors[i].pixel[1])[2] = CIN_ROQ_ClampByte(data[1] + b);
		((byte *)&ROQCIN.quadVectors[i].pixel[1])[3] = 255;

		((byte *)&ROQCIN.quadVectors[i].pixel[2])[0] = CIN_ROQ_ClampByte(data[2] + r);
		((byte *)&ROQCIN.quadVectors[i].pixel[2])[1] = CIN_ROQ_ClampByte(data[2] - g);
		((byte *)&ROQCIN.quadVectors[i].pixel[2])[2] = CIN_ROQ_ClampByte(data[2] + b);
		((byte *)&ROQCIN.quadVectors[i].pixel[2])[3] = 255;

		((byte *)&ROQCIN.quadVectors[i].pixel[3])[0] = CIN_ROQ_ClampByte(data[3] + r);
		((byte *)&ROQCIN.quadVectors[i].pixel[3])[1] = CIN_ROQ_ClampByte(data[3] - g);
		((byte *)&ROQCIN.quadVectors[i].pixel[3])[2] = CIN_ROQ_ClampByte(data[3] + b);
		((byte *)&ROQCIN.quadVectors[i].pixel[3])[3] = 255;

		data += 6;
	}

	/* Copy quad cells */
	for (i = 0; i < numQuadCells; i++) {
		ROQCIN.quadCells[i].index[0] = data[0];
		ROQCIN.quadCells[i].index[1] = data[1];
		ROQCIN.quadCells[i].index[2] = data[2];
		ROQCIN.quadCells[i].index[3] = data[3];

		data += 4;
	}
}

/**
 * @sa CIN_ROQ_DecodeChunk
 */
static void CIN_ROQ_DecodeVideo (cinematic_t *cin, const byte *data)
{
	byte	*buffer;
	int		vqFlag, vqFlagPos, vqCode;
	int		xPos, yPos, xMot, yMot;
	int		x, y;
	int		index;
	int		i;

	if (!ROQCIN.frameBuffer[0] || !ROQCIN.frameBuffer[1])
		return;		/* No frame buffer */

	vqFlag = 0;
	vqFlagPos = 0;

	xPos = 0;
	yPos = 0;

	xMot = (char)((ROQCIN.chunk.flags >> 8) & 0xFF);
	yMot = (char)((ROQCIN.chunk.flags >> 0) & 0xFF);

	index = 0;

	while (1) {
		for (y = yPos; y < yPos + 16; y += 8) {
			for (x = xPos; x < xPos + 16; x += 8) {
				if (!vqFlagPos) {
					vqFlagPos = 7;
					vqFlag = data[index + 0] | (data[index + 1] << 8);
					vqFlag = LittleShort(vqFlag);
					index += 2;
				} else
					vqFlagPos--;

				vqCode = vqFlag & ROQ_ID_CCC;
				vqFlag <<= 2;

				switch (vqCode) {
				case ROQ_ID_FCC:
					CIN_ROQ_ApplyMotion8x8(cin, x, y, xMot, yMot, data[index]);
					index += 1;
					break;
				case ROQ_ID_SLD:
					CIN_ROQ_ApplyVector4x4(cin, x, y, ROQCIN.quadCells[data[index]].index);
					index += 1;
					break;
				case ROQ_ID_CCC:
					for (i = 0; i < 4; i++) {
						const int xp = x + roqCin_quadOffsets4[0][i];
						const int yp = y + roqCin_quadOffsets4[1][i];

						if (!vqFlagPos) {
							vqFlagPos = 7;
							vqFlag = data[index + 0] | (data[index + 1] << 8);
							vqFlag = LittleShort(vqFlag);

							index += 2;
						} else
							vqFlagPos--;

						vqCode = vqFlag & ROQ_ID_CCC;
						vqFlag <<= 2;

						switch (vqCode) {
						case ROQ_ID_FCC:
							CIN_ROQ_ApplyMotion4x4(cin, xp, yp, xMot, yMot, data[index]);
							index += 1;
							break;
						case ROQ_ID_SLD:
							CIN_ROQ_ApplyVector2x2(cin, xp, yp, ROQCIN.quadCells[data[index]].index);
							index += 1;
							break;
						case ROQ_ID_CCC:
							CIN_ROQ_ApplyVector2x2(cin, xp, yp, &data[index]);
							index += 4;
							break;
						}
					}
					break;
				}
			}
		}

		xPos += 16;
		if (xPos >= ROQCIN.frameWidth) {
			xPos -= ROQCIN.frameWidth;

			yPos += 16;
			if (yPos >= ROQCIN.frameHeight)
				break;
		}
	}

	/* Copy or swap the buffers */
	if (!ROQCIN.currentFrame)
		memcpy(ROQCIN.frameBuffer[1], ROQCIN.frameBuffer[0], ROQCIN.frameWidth * ROQCIN.frameHeight * 4);
	else {
		buffer = ROQCIN.frameBuffer[0];
		ROQCIN.frameBuffer[0] = ROQCIN.frameBuffer[1];
		ROQCIN.frameBuffer[1] = buffer;
	}

	ROQCIN.currentFrame++;
}

/**
 * @sa CIN_ROQ_DecodeSoundStereo
 * @sa CIN_ROQ_DecodeChunk
 */
static void CIN_ROQ_DecodeSoundMono (cinematic_t *cin, const byte *data)
{
	short samples[ROQ_MAX_CHUNK_SIZE * 2];
	int prev = 0;
	int i, j;

	for (i = 0, j = 0; i < ROQCIN.chunk.size; i++, j += 2) {
		prev = (short)(prev + roqCin_sqrTable[data[i]]);
		samples[j] = (short)prev;
		samples[j + 1] = (short)prev;
	}

	M_AddToSampleBuffer(&ROQCIN.musicStream, ROQ_SOUND_RATE, i, (const byte *)samples);
}

/**
 * @sa CIN_ROQ_DecodeSoundMono
 * @sa CIN_ROQ_DecodeChunk
 */
static void CIN_ROQ_DecodeSoundStereo (cinematic_t *cin, const byte *data)
{
	short samples[ROQ_MAX_CHUNK_SIZE];
	int i;
	short prevL = (ROQCIN.chunk.flags & 0xFF00) << 0;
	short prevR = (ROQCIN.chunk.flags & 0x00FF) << 8;

	for (i = 0; i < ROQCIN.chunk.size; i += 2) {
		prevL = prevL + roqCin_sqrTable[data[i + 0]];
		prevR = prevR + roqCin_sqrTable[data[i + 1]];

		samples[i + 0] = prevL;
		samples[i + 1] = prevR;
	}

	M_AddToSampleBuffer(&ROQCIN.musicStream, ROQ_SOUND_RATE, i / 2, (const byte *)samples);
}

/**
 * @sa CIN_ROQ_RunCinematic
 * @return true if the cinematic is still running, false otherwise
 */
static qboolean CIN_ROQ_DecodeChunk (cinematic_t *cin)
{
	int frame;

	if (ROQCIN.startTime + ((1000 / ROQCIN.frameRate) * ROQCIN.currentFrame) > CL_Milliseconds())
		return qtrue;

	frame = ROQCIN.currentFrame;

	do {
		if (ROQCIN.offset >= ROQCIN.size)
			return qfalse;	/* Finished */

		/* Parse the chunk header */
		ROQCIN.chunk.id = LittleShort(*(short *)&ROQCIN.header[0]);
		ROQCIN.chunk.size = LittleLong(*(int *)&ROQCIN.header[2]);
		ROQCIN.chunk.flags = LittleShort(*(short *)&ROQCIN.header[6]);

		if (ROQCIN.chunk.id == ROQ_IDENT || ROQCIN.chunk.size > ROQ_MAX_CHUNK_SIZE) {
			Com_Printf("Invalid chunk id during decode: %i\n", ROQCIN.chunk.id);
			cin->replay = qfalse;
			return qfalse;	/* Invalid chunk */
		}

		/* Read the chunk data and the next chunk header */
		FS_Read(ROQCIN.data, ROQCIN.chunk.size + ROQ_CHUNK_HEADER_SIZE, &ROQCIN.file);
		ROQCIN.offset += ROQCIN.chunk.size + ROQ_CHUNK_HEADER_SIZE;

		ROQCIN.header = ROQCIN.data + ROQCIN.chunk.size;

		/* Decode the chunk data */
		switch (ROQCIN.chunk.id) {
		case ROQ_QUAD_INFO:
			CIN_ROQ_DecodeInfo(cin, ROQCIN.data);
			break;
		case ROQ_QUAD_CODEBOOK:
			CIN_ROQ_DecodeCodeBook(cin, ROQCIN.data);
			break;
		case ROQ_QUAD_VQ:
			CIN_ROQ_DecodeVideo(cin, ROQCIN.data);
			break;
		case ROQ_SOUND_MONO:
			if (!cin->noSound)
				CIN_ROQ_DecodeSoundMono(cin, ROQCIN.data);
			break;
		case ROQ_SOUND_STEREO:
			if (!cin->noSound)
				CIN_ROQ_DecodeSoundStereo(cin, ROQCIN.data);
			break;
		default:
			Com_Printf("Invalid chunk id: %i\n", ROQCIN.chunk.id);
			break;
		}
	/* loop until we finally got a new frame */
	} while (frame == ROQCIN.currentFrame && cin->status);

	return qtrue;
}

/**
 * @sa R_UploadData
 */
static void CIN_ROQ_DrawCinematic (cinematic_t *cin)
{
	int texnum;

	assert(cin->status != CIN_STATUS_NONE);

	if (!ROQCIN.frameBuffer[1])
		return;
	texnum = R_UploadData("***cinematic***", ROQCIN.frameBuffer[1], ROQCIN.frameWidth, ROQCIN.frameHeight);
	R_DrawTexture(texnum, cin->x, cin->y, cin->w, cin->h);
}

/**
 * @return true if the cinematic is still running, false otherwise
 */
qboolean CIN_ROQ_RunCinematic (cinematic_t *cin)
{
	qboolean runState = CIN_ROQ_DecodeChunk(cin);
	if (runState)
		CIN_ROQ_DrawCinematic(cin);
	return runState;
}

void CIN_ROQ_CloseCinematic (cinematic_t *cin)
{
	if (ROQCIN.file.f || ROQCIN.file.z)
		FS_CloseFile(&ROQCIN.file);
	else
		Com_Printf("CIN_ROQ_StopCinematic: Warning no opened file\n");

	if (ROQCIN.frameBuffer[0]) {
		Mem_Free(ROQCIN.frameBuffer[0]);
		Mem_Free(ROQCIN.frameBuffer[1]);
	}

	M_StopMusicStream(&ROQCIN.musicStream);

	Mem_Free(cin->codecData);
	cin->codecData = NULL;
}

int CIN_ROQ_OpenCinematic (cinematic_t *cin, const char *fileName)
{
	roqChunk_t chunk;
	int size;
	byte header[ROQ_CHUNK_HEADER_SIZE];

	if (cin->codecData && (ROQCIN.file.f || ROQCIN.file.z)) {
		Com_Printf("WARNING: it seams there was already a roq running, it will be killed to start %s\n", fileName);
		CIN_ROQ_CloseCinematic(cin);
	}

	assert(cin->codecData == NULL);
	cin->codecData = Mem_PoolAlloc(sizeof(ROQCIN), vid_genericPool, 0);
	OBJZERO(ROQCIN);

	/* Open the file */
	size = FS_OpenFile(fileName, &ROQCIN.file, FILE_READ);
	if (!ROQCIN.file.f && !ROQCIN.file.z) {
		Com_Printf("Cinematic %s not found\n", fileName);
		return 1;
	}

	/* Parse the header */
	FS_Read(header, sizeof(header), &ROQCIN.file);

	/* first 8 bytes are the header */
	chunk.id = LittleShort(*(short *)&header[0]);
	chunk.size = LittleLong(*(int *)&header[2]);
	chunk.flags = LittleShort(*(short *)&header[6]);

	if (chunk.id != ROQ_IDENT) {
		FS_CloseFile(&ROQCIN.file);
		Com_Printf("CIN_PlayCinematic: invalid RoQ header\n");
		return 1;
	}

	cin->cinematicType = CINEMATIC_TYPE_ROQ;

	/* Fill it in */
	Q_strncpyz(cin->name, fileName, sizeof(cin->name));

	M_PlayMusicStream(&ROQCIN.musicStream);

	ROQCIN.size = size;
	ROQCIN.offset = sizeof(header);

	ROQCIN.frameWidth = 0;
	ROQCIN.frameHeight = 0;
	ROQCIN.startTime = CL_Milliseconds();
	ROQCIN.frameRate = (chunk.flags != 0) ? chunk.flags : 30;
	if (ROQCIN.frameBuffer[0]) {
		Mem_Free(ROQCIN.frameBuffer[0]);
		ROQCIN.frameBuffer[0] = NULL;
	}
	if (ROQCIN.frameBuffer[1]) {
		Mem_Free(ROQCIN.frameBuffer[1]);
		ROQCIN.frameBuffer[1] = NULL;
	}

	ROQCIN.currentFrame = 0;

	/* Read the first chunk header */
	FS_Read(ROQCIN.data, ROQ_CHUNK_HEADER_SIZE, &ROQCIN.file);
	ROQCIN.offset += ROQ_CHUNK_HEADER_SIZE;

	ROQCIN.header = ROQCIN.data;

	return 0;
}

void CIN_ROQ_Init (void)
{
	int i;

	/* Build square table */
	for (i = 0; i < 128; i++) {
		const short s = (short)(i * i);
		roqCin_sqrTable[i] = s;
		roqCin_sqrTable[i + 128] = -s;
	}

	/* Set up quad offsets */
	for (i = 0; i < 4; i++) {
		roqCin_quadOffsets2[0][i] = 2 * (i & 1);
		roqCin_quadOffsets2[1][i] = 2 * (i >> 1);
		roqCin_quadOffsets4[0][i] = 4 * (i & 1);
		roqCin_quadOffsets4[1][i] = 4 * (i >> 1);
	}

	/* Build YUV table */
	for (i = 0; i < 256; i++) {
		const float f = (float)(i - 128);
		roqCin_yuvTable.vr[i] = Q_ftol(f * 1.40200f);
		roqCin_yuvTable.ug[i] = Q_ftol(f * 0.34414f);
		roqCin_yuvTable.vg[i] = Q_ftol(f * 0.71414f);
		roqCin_yuvTable.ub[i] = Q_ftol(f * 1.77200f);
	}
}
