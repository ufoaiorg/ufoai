/**
 * @file cl_cinematic.c
 * @brief Single player employee stuff.
 * @note Employee related functions prefix: E_
 * @note This code based on the OverDose amd KMQuakeII source code
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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


#include "client.h"
#include "snd_loc.h"

#include <assert.h>

#define ROQ_IDENT					0x1084

#define ROQ_QUAD_INFO				0x1001
#define ROQ_QUAD_CODEBOOK			0x1002
#define ROQ_QUAD_VQ					0x1011
#define ROQ_SOUND_MONO				0x1020
#define ROQ_SOUND_STEREO			0x1021

#define ROQ_CHUNK_HEADER_SIZE		8			/* Size of a RoQ chunk header */

#define ROQ_MAX_CHUNK_SIZE			65536		/* Max size of a RoQ chunk */

typedef struct {
	unsigned short	id;
	unsigned int	size;
	unsigned short	flags;
} roqChunk_t;

typedef struct {
	unsigned int			pixel[4];
} roqQuadVector_t;

typedef struct {
	byte			index[4];
} roqQuadCell_t;

typedef struct {
	int				vr[256];
	int				ug[256];
	int				vg[256];
	int				ub[256];
} yuvTable_t;

typedef struct {
	char			name[MAX_OSPATH];

	qFILE			file;
	int				size;
	int				offset;

	int				frameWidth;
	int				frameHeight;
	int				frameRate;
	byte *			frameBuffer[2];

	int				frameTime;
	int				currentFrame;

	byte			data[ROQ_MAX_CHUNK_SIZE + ROQ_CHUNK_HEADER_SIZE];
	byte *			header;

	roqChunk_t		chunk;
	roqQuadVector_t	quadVectors[256];
	roqQuadCell_t	quadCells[256];
} cinematic_t;

static yuvTable_t	cin_yuvTable;
static short		cin_sqrTable[256];

static int			cin_quadOffsets2[2][4];
static int			cin_quadOffsets4[2][4];

static cinematic_t	cin;

/**
 * @brief Clamps integer value into byte
 */
static byte CIN_ClampByte (int value)
{
	if (value < 0)
		return 0;

	if (value > 255)
		return 255;

	return value;
}

/**
 * @brief
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyVector2x2 (int x, int y, const byte *indices)
{
	unsigned int	*src, *dst;
	int		xp, yp;
	int		i;

	for (i = 0; i < 4; i++){
		xp = x + cin_quadOffsets2[0][i];
		yp = y + cin_quadOffsets2[1][i];

		src = (unsigned int *)cin.quadVectors + (indices[i] * 4);
		dst = (unsigned int *)cin.frameBuffer[0] + (yp * cin.frameWidth + xp);

		dst[0] = src[0];
		dst[1] = src[1];

		dst += cin.frameWidth;

		dst[0] = src[2];
		dst[1] = src[3];
	}
}

/**
 * @brief
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyVector4x4 (int x, int y, const byte *indices)
{
	unsigned int	*src, *dst;
	int		xp, yp;
	int		i;

	for (i = 0; i < 4; i++) {
		xp = x + cin_quadOffsets4[0][i];
		yp = y + cin_quadOffsets4[1][i];

		src = (unsigned int *)cin.quadVectors + (indices[i] * 4);
		dst = (unsigned int *)cin.frameBuffer[0] + (yp * cin.frameWidth + xp);

		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[1];
		dst[3] = src[1];

		dst += cin.frameWidth;

		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[1];
		dst[3] = src[1];

		dst += cin.frameWidth;

		dst[0] = src[2];
		dst[1] = src[2];
		dst[2] = src[3];
		dst[3] = src[3];

		dst += cin.frameWidth;

		dst[0] = src[2];
		dst[1] = src[2];
		dst[2] = src[3];
		dst[3] = src[3];
	}
}

/**
 * @brief
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyMotion4x4 (int x, int y, int mx, int my, int mv)
{
	unsigned int	*src, *dst;
	int		xp, yp;
	int		i;

	xp = x + 8 - (mv >> 4) - mx;
	yp = y + 8 - (mv & 15) - my;

	src = (unsigned int *)cin.frameBuffer[1] + (yp * cin.frameWidth + xp);
	dst = (unsigned int *)cin.frameBuffer[0] + (y * cin.frameWidth + x);

	for (i = 0; i < 4; i++, src += cin.frameWidth, dst += cin.frameWidth) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
	}
}

/**
 * @brief
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyMotion8x8 (int x, int y, int mx, int my, int mv)
{
	unsigned int	*src, *dst;
	int		xp, yp;
	int		i;

	xp = x + 8 - (mv >> 4) - mx;
	yp = y + 8 - (mv & 15) - my;

	src = (unsigned int *)cin.frameBuffer[1] + (yp * cin.frameWidth + xp);
	dst = (unsigned int *)cin.frameBuffer[0] + (y * cin.frameWidth + x);

	for (i = 0; i < 8; i++, src += cin.frameWidth, dst += cin.frameWidth) {
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
 * @brief
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeInfo (const byte *data)
{
	if (cin.frameBuffer[0] && cin.frameBuffer[1])
		return;		/* Already allocated */

	/* Allocate the frame buffer */
	cin.frameWidth = data[0] | (data[1] << 8);
	cin.frameHeight = data[2] | (data[3] << 8);

	cin.frameWidth = LittleShort(cin.frameWidth);
	cin.frameHeight = LittleShort(cin.frameHeight);

	if (!Q_IsPowerOfTwo(cin.frameWidth) || !Q_IsPowerOfTwo(cin.frameHeight))
		Com_Error(ERR_DROP, "CIN_DecodeInfo: size is not a power of two (%i x %i)", cin.frameWidth, cin.frameHeight);

	cin.frameBuffer[0] = (byte *)Mem_PoolAlloc(cin.frameWidth * cin.frameHeight * 4, cl_genericPool, CL_TAG_NONE);
	cin.frameBuffer[1] = (byte *)Mem_PoolAlloc(cin.frameWidth * cin.frameHeight * 4, cl_genericPool, CL_TAG_NONE);
}

/**
 * @brief
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeCodeBook (const byte *data)
{
	int		numQuadVectors, numQuadCells;
	int		r, g, b;
	int		i;

	if (cin.chunk.flags) {
		numQuadVectors = (cin.chunk.flags >> 8) & 0xFF;
		numQuadCells = (cin.chunk.flags >> 0) & 0xFF;

		if (!numQuadVectors)
			numQuadVectors = 256;
	} else {
		numQuadVectors = 256;
		numQuadCells = 256;
	}

	/* Decode YUV quad vectors to RGB */
	for (i = 0; i < numQuadVectors; i++) {
		r = cin_yuvTable.vr[data[5]];
		g = cin_yuvTable.ug[data[4]] + cin_yuvTable.vg[data[5]];
		b = cin_yuvTable.ub[data[4]];

		((byte *)&cin.quadVectors[i].pixel[0])[0] = CIN_ClampByte(data[0] + r);
		((byte *)&cin.quadVectors[i].pixel[0])[1] = CIN_ClampByte(data[0] - g);
		((byte *)&cin.quadVectors[i].pixel[0])[2] = CIN_ClampByte(data[0] + b);
		((byte *)&cin.quadVectors[i].pixel[0])[3] = 255;

		((byte *)&cin.quadVectors[i].pixel[1])[0] = CIN_ClampByte(data[1] + r);
		((byte *)&cin.quadVectors[i].pixel[1])[1] = CIN_ClampByte(data[1] - g);
		((byte *)&cin.quadVectors[i].pixel[1])[2] = CIN_ClampByte(data[1] + b);
		((byte *)&cin.quadVectors[i].pixel[1])[3] = 255;

		((byte *)&cin.quadVectors[i].pixel[2])[0] = CIN_ClampByte(data[2] + r);
		((byte *)&cin.quadVectors[i].pixel[2])[1] = CIN_ClampByte(data[2] - g);
		((byte *)&cin.quadVectors[i].pixel[2])[2] = CIN_ClampByte(data[2] + b);
		((byte *)&cin.quadVectors[i].pixel[2])[3] = 255;

		((byte *)&cin.quadVectors[i].pixel[3])[0] = CIN_ClampByte(data[3] + r);
		((byte *)&cin.quadVectors[i].pixel[3])[1] = CIN_ClampByte(data[3] - g);
		((byte *)&cin.quadVectors[i].pixel[3])[2] = CIN_ClampByte(data[3] + b);
		((byte *)&cin.quadVectors[i].pixel[3])[3] = 255;

		data += 6;
	}

	/* Copy quad cells */
	for (i = 0; i < numQuadCells; i++) {
		cin.quadCells[i].index[0] = data[0];
		cin.quadCells[i].index[1] = data[1];
		cin.quadCells[i].index[2] = data[2];
		cin.quadCells[i].index[3] = data[3];

		data += 4;
	}
}

/**
 * @brief
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeVideo (const byte *data)
{
	byte	*buffer;
	int		vqFlag, vqFlagPos, vqCode;
	int		xPos, yPos, xMot, yMot;
	int		x, y, xp, yp;
	int		index;
	int		i;

	if (!cin.frameBuffer[0] || !cin.frameBuffer[1])
		return;		/* No frame buffer */

	vqFlag = 0;
	vqFlagPos = 0;

	xPos = 0;
	yPos = 0;

	xMot = (char)((cin.chunk.flags >> 8) & 0xFF);
	yMot = (char)((cin.chunk.flags >> 0) & 0xFF);

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

				vqCode = vqFlag & 0xC000;
				vqFlag <<= 2;

				switch (vqCode) {
				case 0x4000:
					CIN_ApplyMotion8x8(x, y, xMot, yMot, data[index]);
					index += 1;
					break;
				case 0x8000:
					CIN_ApplyVector4x4(x, y, cin.quadCells[data[index]].index);
					index += 1;
					break;
				case 0xC000:
					for (i = 0; i < 4; i++) {
						xp = x + cin_quadOffsets4[0][i];
						yp = y + cin_quadOffsets4[1][i];

						if (!vqFlagPos) {
							vqFlagPos = 7;
							vqFlag = data[index+0] | (data[index+1] << 8);
							vqFlag = LittleShort(vqFlag);

							index += 2;
						} else
							vqFlagPos--;

						vqCode = vqFlag & 0xC000;
						vqFlag <<= 2;

						switch (vqCode) {
						case 0x4000:
							CIN_ApplyMotion4x4(xp, yp, xMot, yMot, data[index]);
							index += 1;
							break;
						case 0x8000:
							CIN_ApplyVector2x2(xp, yp, cin.quadCells[data[index]].index);
							index += 1;
							break;
						case 0xC000:
							CIN_ApplyVector2x2(xp, yp, &data[index]);
							index += 4;
							break;
						}
					}
					break;
				}
			}
		}

		xPos += 16;
		if (xPos >= cin.frameWidth) {
			xPos -= cin.frameWidth;

			yPos += 16;
			if (yPos >= cin.frameHeight)
				break;
		}
	}

	/* Copy or swap the buffers */
	if (!cin.currentFrame)
		memcpy(cin.frameBuffer[1], cin.frameBuffer[0], cin.frameWidth * cin.frameHeight * 4);
	else {
		buffer = cin.frameBuffer[0];
		cin.frameBuffer[0] = cin.frameBuffer[1];
		cin.frameBuffer[1] = buffer;
	}

	cin.currentFrame++;
}

/**
 * @brief
 * @sa CIN_DecodeSoundStereo
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeSoundMono (const byte *data)
{
	short	samples[ROQ_MAX_CHUNK_SIZE];
	int		prev;
	int		i;

	prev = cin.chunk.flags;

	for (i = 0; i < cin.chunk.size; i++) {
		prev = (short)(prev + cin_sqrTable[data[i]]);
		samples[i] = (short)prev;
	}

	/* Send samples to mixer */
	S_RawSamples(cin.chunk.size, 22050, 2, 1, (byte*)samples, 1.0f);
}

/**
 * @brief
 * @sa CIN_DecodeSoundMono
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeSoundStereo (const byte *data)
{
	short	samples[ROQ_MAX_CHUNK_SIZE];
	int		prevL, prevR;
	int		i;

	prevL = (cin.chunk.flags & 0xFF00) << 0;
	prevR = (cin.chunk.flags & 0x00FF) << 8;

	for (i = 0; i < cin.chunk.size; i += 2) {
		prevL = (short)(prevL + cin_sqrTable[data[i + 0]]);
		prevR = (short)(prevR + cin_sqrTable[data[i + 1]]);

		samples[i + 0] = (short)prevL;
		samples[i + 1] = (short)prevR;
	}

	/* Send samples to mixer */
	S_RawSamples(cin.chunk.size / 2, 22050, 2, 2, (byte*)samples, 1.0f);
}

/**
 * @brief
 */
static qboolean CIN_DecodeChunk (void)
{
	if (cin.offset >= cin.size)
		return qfalse;	/* Finished */

	/* Parse the chunk header */
	cin.chunk.id = cin.header[0] | (cin.header[1] << 8);
	cin.chunk.size = cin.header[2] | (cin.header[3] << 8) | (cin.header[4] << 16) | (cin.header[5] << 24);
	cin.chunk.flags = cin.header[6] | (cin.header[7] << 8);

	cin.chunk.id = LittleShort(cin.chunk.id);
	cin.chunk.size = LittleLong(cin.chunk.size);
	cin.chunk.flags = LittleShort(cin.chunk.flags);

	if (cin.chunk.id == ROQ_IDENT || cin.chunk.size > ROQ_MAX_CHUNK_SIZE) {
		Com_Printf("Invalid chunk\n");
		return qfalse;	/* Invalid chunk */
	}

	/* Read the chunk data and the next chunk header */
	FS_Read(cin.data, cin.chunk.size + ROQ_CHUNK_HEADER_SIZE, &cin.file);
	cin.offset += cin.chunk.size + ROQ_CHUNK_HEADER_SIZE;

	cin.header = cin.data + cin.chunk.size;

	/* Decode the chunk data */
	switch (cin.chunk.id) {
	case ROQ_QUAD_INFO:
		CIN_DecodeInfo(cin.data);
		break;
	case ROQ_QUAD_CODEBOOK:
		CIN_DecodeCodeBook(cin.data);
		break;
	case ROQ_QUAD_VQ:
		CIN_DecodeVideo(cin.data);
		break;
	case ROQ_SOUND_MONO:
		CIN_DecodeSoundMono(cin.data);
		break;
	case ROQ_SOUND_STEREO:
		CIN_DecodeSoundStereo(cin.data);
		break;
	}

	return qtrue;
}

/**
 * @brief
 * @sa CL_Frame
 */
void CIN_RunCinematic (void)
{
	int frame;

	if (!cls.playingCinematic)
		return;			/* Not playing */

	/* Check if a new frame is needed */
	frame = (cls.realtime - cin.frameTime) * cin.frameRate / 1000;
	if (frame <= cin.currentFrame)
		return;

	/* Never drop too many frames in a row */
	if (frame > cin.currentFrame + 1)
		cin.frameTime = cls.realtime - (cin.currentFrame * 1000 / cin.frameRate);

	/* Decode chunks until the desired frame is reached */
	if (CIN_DecodeChunk())
		return;

	/* If we get here, the cinematic has either finished or failed */
	CIN_StopCinematic();
}

/**
 * @brief
 * @sa GL_DrawPixelData
 */
void CIN_DrawCinematic (void)
{
	assert(cls.playingCinematic);

	if (!cin.frameBuffer[1])
		return;
	re.DrawImagePixelData("***cinematic***", cin.frameBuffer[1], cin.frameWidth, cin.frameHeight);
}

/**
 * @brief
 * @sa CIN_StopCinematic
 */
void CIN_PlayCinematic (const char *name)
{
	roqChunk_t chunk;
	int size;
	byte header[ROQ_CHUNK_HEADER_SIZE];

	/* Make sure sounds aren't playing */
	S_StopAllSounds();
	/* also stop the background music */
	OGG_Stop();

	/* If already playing a cinematic, stop it */
	CIN_StopCinematic();

	/* Open the file */
	size = FS_FOpenFile(name, &cin.file);
	if (!cin.file.f && !cin.file.z){
		Com_Printf("Cinematic %s not found\n", name);
		return;
	}

	/* Parse the header */
	FS_Read(header, sizeof(header), &cin.file);

	chunk.id = header[0] | (header[1] << 8);
	chunk.size = header[2] | (header[3] << 8) | (header[4] << 16) | (header[5] << 24);
	chunk.flags = header[6] | (header[7] << 8);

	if (chunk.id != ROQ_IDENT){
		FS_FCloseFile(&cin.file);
		Com_Error(ERR_DROP, "CIN_PlayCinematic: invalid RoQ header");
	}

	/* Play the cinematic */
	cls.playingCinematic = qtrue;

	/* Fill it in */
	Q_strncpyz(cin.name, name, sizeof(cin.name));

	cin.size = size;
	cin.offset = sizeof(header);

	cin.frameWidth = 0;
	cin.frameHeight = 0;
	cin.frameTime = cls.realtime;
	cin.frameRate = (chunk.flags != 0) ? chunk.flags : 30;
	if (cin.frameBuffer[0]) {
		Mem_Free(cin.frameBuffer[0]);
		cin.frameBuffer[0] = NULL;
	}
	if (cin.frameBuffer[1]) {
		Mem_Free(cin.frameBuffer[1]);
		cin.frameBuffer[1] = NULL;
	}

	cin.currentFrame = 0;

	/* Read the first chunk header */
	FS_Read(cin.data, ROQ_CHUNK_HEADER_SIZE, &cin.file);
	cin.offset += ROQ_CHUNK_HEADER_SIZE;

	cin.header = cin.data;

	/* Force console off */
	Con_Close();

	/* Run the first frame */
	CIN_RunCinematic();
}

/**
 * @brief
 * @sa CIN_PlayCinematic
 */
void CIN_StopCinematic (void)
{
	if (!cls.playingCinematic)
		return;			/* Not playing */

	cls.playingCinematic = qfalse;

	if (cin.file.f || cin.file.z)
		FS_FCloseFile(&cin.file);
	else
		Com_Printf("CIN_StopCinematic: Warning no opened file\n");

	memset(&cin, 0, sizeof(cinematic_t));
}

/**
 * @brief
 */
static void CIN_Cinematic_f (void)
{
	char name[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: cinematic <videoName>\n");
		return;
	}

	Com_sprintf(name, sizeof(name), "videos/%s.roq", Cmd_Argv(1));

	/* If running a local server, kill it */
	SV_Shutdown("Server quit", qfalse);

	/* If connected to a server, disconnect */
	CL_Disconnect();

	/* Play the cinematic */
	CIN_PlayCinematic(name);
}

/**
 * @brief Stop the current active cinematic
 */
static void CIN_CinematicStop_f (void)
{
	if (!cls.playingCinematic) {
		Com_Printf("No cinematic active\n");
		return;			/* Not playing */
	}
	CIN_StopCinematic();
}

/**
 * @brief
 */
void CIN_Init (void)
{
	float f;
	short s;
	int i;

	memset(&cin, 0, sizeof(cinematic_t));

	/* Register our commands */
	Cmd_AddCommand("cinematic", CIN_Cinematic_f, "Plays a cinematic");
	Cmd_AddCommand("cinematic_stop", CIN_CinematicStop_f, "Stops a cinematic");

	/* Build YUV table */
	for (i = 0; i < 256; i++){
		f = (float)(i - 128);
		cin_yuvTable.vr[i] = Q_ftol(f * 1.40200f);
		cin_yuvTable.ug[i] = Q_ftol(f * 0.34414f);
		cin_yuvTable.vg[i] = Q_ftol(f * 0.71414f);
		cin_yuvTable.ub[i] = Q_ftol(f * 1.77200f);
	}

	/* Build square table */
	for (i = 0; i < 128; i++){
		s = (short)(i * i);
		cin_sqrTable[i] = s;
		cin_sqrTable[i + 128] = -s;
	}

	/* Set up quad offsets */
	for (i = 0; i < 4; i++) {
		cin_quadOffsets2[0][i] = 2 * (i & 1);
		cin_quadOffsets2[1][i] = 2 * (i >> 1);
		cin_quadOffsets4[0][i] = 4 * (i & 1);
		cin_quadOffsets4[1][i] = 4 * (i >> 1);
	}
}

/**
 * @brief
 */
void CIN_Shutdown (void)
{
	/* Unregister our commands */
	Cmd_RemoveCommand("cinematic");
	Cmd_RemoveCommand("cinematic_stop");

	/* If playing a cinematic, stop it */
	CIN_StopCinematic();
}
