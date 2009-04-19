/**
 * @file cl_cinematic.c
 * @brief Single player employee stuff.
 * @note Employee related functions prefix: E_
 * @note This code based on the OverDose and KMQuakeII source code
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
#include "cl_cinematic.h"
#include "cl_console.h"
#include "cl_sound.h"
#include "cl_music.h"
#include "renderer/r_draw.h"

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
	char			name[MAX_QPATH];	/**< virtuell filesystem path with file suffix */

	qboolean		replay;	/**< autmatically replay in endless loop */

	qboolean		inMenu;	/**< is this cinematic shown in a menu node? */
	int				x, y, w, h; /**< for drawing in the menu maybe */

	qFILE			file;
	int				size;
	int				offset;

	int				frameWidth;
	int				frameHeight;
	int				frameRate;
	byte *			frameBuffer[2];

	int				startTime;	/**< cinematic start timestamp */
	int				currentFrame;

	int				soundChannels;
	short *			soundBuffer;
	size_t			soundBufferPos;
	size_t			soundBufferSize;
	int				soundChannel;	/**< the mixer channel the sound is played on */

	byte			data[ROQ_MAX_CHUNK_SIZE + ROQ_CHUNK_HEADER_SIZE];
	byte *			header;

	qboolean		noSound;	/**< no sound while playing the cinematic */

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
static inline byte CIN_ClampByte (int value)
{
	if (value < 0)
		return 0;

	if (value > 255)
		return 255;

	return value;
}

/**
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyVector2x2 (int x, int y, const byte *indices)
{
	int i;

	for (i = 0; i < 4; i++) {
		const int xp = x + cin_quadOffsets2[0][i];
		const int yp = y + cin_quadOffsets2[1][i];
		const unsigned int *src = (const unsigned int *)cin.quadVectors + (indices[i] * 4);
		unsigned int *dst = (unsigned int *)cin.frameBuffer[0] + (yp * cin.frameWidth + xp);

		dst[0] = src[0];
		dst[1] = src[1];

		dst += cin.frameWidth;

		dst[0] = src[2];
		dst[1] = src[3];
	}
}

/**
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyVector4x4 (int x, int y, const byte *indices)
{
	int i;

	for (i = 0; i < 4; i++) {
		const int xp = x + cin_quadOffsets4[0][i];
		const int yp = y + cin_quadOffsets4[1][i];
		const unsigned int *src = (const unsigned int *)cin.quadVectors + (indices[i] * 4);
		unsigned int *dst = (unsigned int *)cin.frameBuffer[0] + (yp * cin.frameWidth + xp);

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
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyMotion4x4 (int x, int y, int mx, int my, int mv)
{
	int i;
	const int xp = x + 8 - (mv >> 4) - mx;
	const int yp = y + 8 - (mv & 15) - my;
	const unsigned int *src = (const unsigned int *)cin.frameBuffer[1] + (yp * cin.frameWidth + xp);
	unsigned int *dst = (unsigned int *)cin.frameBuffer[0] + (y * cin.frameWidth + x);

	for (i = 0; i < 4; i++, src += cin.frameWidth, dst += cin.frameWidth) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
	}
}

/**
 * @sa CIN_DecodeVideo
 */
static void CIN_ApplyMotion8x8 (int x, int y, int mx, int my, int mv)
{
	int i;
	const int xp = x + 8 - (mv >> 4) - mx;
	const int yp = y + 8 - (mv & 15) - my;
	const unsigned int *src = (const unsigned int *)cin.frameBuffer[1] + (yp * cin.frameWidth + xp);
	unsigned int *dst = (unsigned int *)cin.frameBuffer[0] + (y * cin.frameWidth + x);

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

	cin.frameBuffer[0] = (byte *)Mem_PoolAlloc(cin.frameWidth * cin.frameHeight * 4, cl_genericPool, 0);
	cin.frameBuffer[1] = (byte *)Mem_PoolAlloc(cin.frameWidth * cin.frameHeight * 4, cl_genericPool, 0);
}

/**
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeCodeBook (const byte *data)
{
	int		numQuadVectors, numQuadCells;
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
		const int r = cin_yuvTable.vr[data[5]];
		const int g = cin_yuvTable.ug[data[4]] + cin_yuvTable.vg[data[5]];
		const int b = cin_yuvTable.ub[data[4]];

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
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeVideo (const byte *data)
{
	byte	*buffer;
	int		vqFlag, vqFlagPos, vqCode;
	int		xPos, yPos, xMot, yMot;
	int		x, y;
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

				vqCode = vqFlag & ROQ_ID_CCC;
				vqFlag <<= 2;

				switch (vqCode) {
				case ROQ_ID_FCC:
					CIN_ApplyMotion8x8(x, y, xMot, yMot, data[index]);
					index += 1;
					break;
				case ROQ_ID_SLD:
					CIN_ApplyVector4x4(x, y, cin.quadCells[data[index]].index);
					index += 1;
					break;
				case ROQ_ID_CCC:
					for (i = 0; i < 4; i++) {
						const int xp = x + cin_quadOffsets4[0][i];
						const int yp = y + cin_quadOffsets4[1][i];

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
							CIN_ApplyMotion4x4(xp, yp, xMot, yMot, data[index]);
							index += 1;
							break;
						case ROQ_ID_SLD:
							CIN_ApplyVector2x2(xp, yp, cin.quadCells[data[index]].index);
							index += 1;
							break;
						case ROQ_ID_CCC:
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
 * @sa CIN_DecodeSoundStereo
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeSoundMono (const byte *data, short *samples)
{
	int i;
	int prev = cin.chunk.flags;

	for (i = 0; i < cin.chunk.size; i++) {
		prev = (short)(prev + cin_sqrTable[data[i]]);
		samples[i] = (short)prev;
	}
}

/**
 * @sa CIN_DecodeSoundMono
 * @sa CIN_DecodeChunk
 */
static void CIN_DecodeSoundStereo (const byte *data, short *samples)
{
	int i;
	int prevL = (cin.chunk.flags & 0xFF00) << 0;
	int prevR = (cin.chunk.flags & 0x00FF) << 8;

	for (i = 0; i < cin.chunk.size; i += 2) {
		prevL = (short)(prevL + cin_sqrTable[data[i + 0]]);
		prevR = (short)(prevR + cin_sqrTable[data[i + 1]]);

		samples[i + 0] = (short)prevL;
		samples[i + 1] = (short)prevR;
	}
}

/**
 * @sa CIN_RunCinematic
 */
static qboolean CIN_DecodeChunk (void)
{
	int frame;

	if (cin.startTime + ((1000 / cin.frameRate) * cin.currentFrame) > cls.realtime)
		return qtrue;

	frame = cin.currentFrame;

	do {
		if (cin.offset >= cin.size)
			return qfalse;	/* Finished */

		/* Parse the chunk header */
		cin.chunk.id = LittleShort(*(short *)&cin.header[0]);
		cin.chunk.size = LittleLong(*(int *)&cin.header[2]);
		cin.chunk.flags = LittleShort(*(short *)&cin.header[6]);

		if (cin.chunk.id == ROQ_IDENT || cin.chunk.size > ROQ_MAX_CHUNK_SIZE) {
			Com_Printf("Invalid chunk id during decode: %i\n", cin.chunk.id);
			cin.replay = qfalse;
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
		/* skip sound here - it's precached */
		case ROQ_SOUND_MONO:
		case ROQ_SOUND_STEREO:
			break;
		default:
			Com_Printf("Invalid chunk id: %i\n", cin.chunk.id);
			break;
		}
	/* loop until we finally got a new frame */
	} while (frame == cin.currentFrame && cls.playingCinematic);

	return qtrue;
}

/**
 * @sa MN_Draw
 * @note Coordinates should be relative to VID_NORM_WIDTH and VID_NORM_HEIGHT
 * they are normalized inside this function
 */
void CIN_SetParameters (int x, int y, int w, int h, int cinStatus, qboolean noSound)
{
	cin.x = x * viddef.rx;
	cin.y = y * viddef.ry;
	cin.w = w * viddef.rx;
	cin.h = h * viddef.ry;
	if (cinStatus > CIN_STATUS_NONE)
		cls.playingCinematic = cinStatus;
	cin.noSound = noSound;
}

/**
 * @sa R_DrawImagePixelData
 */
static void CIN_DrawCinematic (void)
{
	int texnum;

	assert(cls.playingCinematic != CIN_STATUS_NONE);

	if (!cin.frameBuffer[1])
		return;
	texnum = R_DrawImagePixelData("***cinematic***", cin.frameBuffer[1], cin.frameWidth, cin.frameHeight);
	R_DrawTexture(texnum, cin.x, cin.y, cin.w, cin.h);
}

/**
 * @sa CL_Frame
 */
void CIN_RunCinematic (void)
{
	assert(cls.playingCinematic != CIN_STATUS_NONE);

	/* Decode chunks until the desired frame is reached */
	if (CIN_DecodeChunk()) {
		CIN_DrawCinematic();
		return;
	}

	/* If we get here, the cinematic has either finished or failed */
	if (cin.replay) {
		char name[MAX_QPATH];
		Q_strncpyz(name, cin.name, sizeof(name));
		CIN_PlayCinematic(name);
		cin.replay = qtrue;
	} else {
		CIN_StopCinematic();
	}
}

/**
 * @sa CIN_StopCinematic
 */
void CIN_PlayCinematic (const char *name)
{
	roqChunk_t chunk;
	int size;
	byte header[ROQ_CHUNK_HEADER_SIZE];

	if (cls.playingCinematic <= CIN_STATUS_FULLSCREEN) {
		/* Make sure sounds aren't playing */
		S_StopAllSounds();
		/* also stop the background music */
		M_Shutdown();
	}

	/* If already playing a cinematic, stop it */
	CIN_StopCinematic();

	memset(&cin, 0, sizeof(cin));

	/* Open the file */
	size = FS_OpenFile(name, &cin.file, FILE_READ);
	if (!cin.file.f && !cin.file.z) {
		Com_Printf("Cinematic %s not found\n", name);
		return;
	}

	/* Parse the header */
	FS_Read(header, sizeof(header), &cin.file);

	/* first 8 bytes are the header */
	chunk.id = LittleShort(*(short *)&header[0]);
	chunk.size = LittleLong(*(int *)&header[2]);
	chunk.flags = LittleShort(*(short *)&header[6]);

	if (chunk.id != ROQ_IDENT) {
		FS_CloseFile(&cin.file);
		Com_Error(ERR_DROP, "CIN_PlayCinematic: invalid RoQ header");
	}

	/* Fill it in */
	Q_strncpyz(cin.name, name, sizeof(cin.name));

	/* Set to play the cinematic in fullscreen mode */
	CIN_SetParameters(0, 0, VID_NORM_WIDTH, VID_NORM_HEIGHT, CIN_STATUS_FULLSCREEN, qfalse);

	cin.size = size;
	cin.offset = sizeof(header);

	cin.frameWidth = 0;
	cin.frameHeight = 0;
	cin.startTime = cls.realtime;
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

	/* Force console off in fullscreen mode - not neccessary in menu playback mode */
	if (cls.playingCinematic == CIN_STATUS_FULLSCREEN)
		Con_Close();

	if (!cin.noSound) {
		short samples[ROQ_MAX_CHUNK_SIZE];
		const size_t initialSize = ROQ_MAX_CHUNK_SIZE * 100;

		cin.soundBuffer = (short *)Mem_PoolAlloc(initialSize, cl_soundSysPool, 0);
		cin.soundBufferSize = Mem_Size(cin.soundBuffer);

		/* buffer the sound chunks */
		while (cin.offset <= cin.size) {
			/* first 8 bytes are the header */
			cin.chunk.id = LittleShort(*(short *)&cin.header[0]);
			cin.chunk.size = LittleLong(*(int *)&cin.header[2]);
			cin.chunk.flags = LittleShort(*(short *)&cin.header[6]);

			if (cin.chunk.id == ROQ_IDENT || cin.chunk.size > ROQ_MAX_CHUNK_SIZE) {
				Com_Printf("Invalid chunk at sound precaching\n");
				cin.noSound = qtrue;
				break;	/* Invalid chunk */
			}

			/* Read the chunk data and the next chunk header */
			FS_Read(cin.data, cin.chunk.size + ROQ_CHUNK_HEADER_SIZE, &cin.file);
			cin.offset += cin.chunk.size + ROQ_CHUNK_HEADER_SIZE;

			cin.header = cin.data + cin.chunk.size;

			switch (cin.chunk.id) {
			case ROQ_SOUND_MONO:
				if (cin.soundChannels == 2)
					Sys_Error("Mixed sound channels in roq file\n");
				cin.soundChannels = 1;
				CIN_DecodeSoundMono(cin.data, samples);
				break;
			case ROQ_SOUND_STEREO:
				if (cin.soundChannels == 1)
					Sys_Error("Mixed sound channels in roq file\n");
				cin.soundChannels = 2;
				CIN_DecodeSoundStereo(cin.data, samples);
				break;
			case ROQ_QUAD_INFO:
			case ROQ_QUAD_CODEBOOK:
			case ROQ_QUAD_VQ:
				continue;
			default:
				Com_Printf("Invalid chunk id at sound precaching: %i\n", cin.chunk.id);
				continue;
			}
			if (cin.soundBufferPos + cin.chunk.size >= cin.soundBufferSize) {
				cin.soundBuffer = Mem_ReAlloc(cin.soundBuffer, cin.soundBufferSize + ROQ_MAX_CHUNK_SIZE * 10);
				if (!cin.soundBuffer)
					Com_Error(ERR_DROP, "Could not allocate memory for cinematic sound\n");
				cin.soundBufferSize = Mem_Size(cin.soundBuffer);
			}
			memcpy(&cin.soundBuffer[cin.soundBufferPos], samples, cin.chunk.size * sizeof(short));
			cin.soundBufferPos += cin.chunk.size;
		}

		/* check whether this roq even has sound data */
		if (!cin.noSound && cin.soundChannels)
			/* Send samples to mixer */
			cin.soundChannel = S_PlaySoundFromMem(cin.soundBuffer, cin.soundBufferPos * sizeof(short), ROQ_SOUND_RATE, cin.soundChannels, -1);

		if (cin.soundBuffer) {
			Mem_Free(cin.soundBuffer);
			cin.soundBuffer = NULL;
		}

		/* prepare for video parsing */
		cin.offset = sizeof(header);
		FS_Seek(&cin.file, cin.offset, FS_SEEK_SET);
		/* Read the first chunk header */
		FS_Read(cin.data, ROQ_CHUNK_HEADER_SIZE, &cin.file);
		cin.offset += ROQ_CHUNK_HEADER_SIZE;
		cin.header = cin.data;
	}
}

/**
 * @sa CIN_PlayCinematic
 */
void CIN_StopCinematic (void)
{
	if (cls.playingCinematic == CIN_STATUS_NONE)
		return;			/* Not playing */

	cls.playingCinematic = CIN_STATUS_NONE;

	if (cin.file.f || cin.file.z)
		FS_CloseFile(&cin.file);
	else
		Com_Printf("CIN_StopCinematic: Warning no opened file\n");

	if (!cin.noSound) {
		/** @todo only stop the cin.soundChannel channel - but don't call
		 * @c Mix_HaltChannel directly */
		S_StopAllSounds();
	}

	if (cin.frameBuffer[0]) {
		Mem_Free(cin.frameBuffer[0]);
		Mem_Free(cin.frameBuffer[1]);
	}
	memset(&cin, 0, sizeof(cin));
}

static void CIN_Cinematic_f (void)
{
	char name[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <videoName>\n", Cmd_Argv(0));
		return;
	}

	Com_sprintf(name, sizeof(name), "videos/%s", Cmd_Argv(1));
	Com_DefaultExtension(name, sizeof(name), ".roq");

	/* If running a local server, kill it */
	SV_Shutdown("Server quit", qfalse);

	/* If connected to a server, disconnect */
	CL_Disconnect();

	CIN_PlayCinematic(name);
}

/**
 * @brief Stop the current active cinematic
 */
static void CIN_CinematicStop_f (void)
{
	if (cls.playingCinematic == CIN_STATUS_NONE) {
		Com_Printf("No cinematic active\n");
		return;			/* Not playing */
	}
	CIN_StopCinematic();
}

static int CIN_CompleteCommand (const char *partial, const char **match)
{
	const char *filename;
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;

	FS_BuildFileList("videos/*.roq");

	len = strlen(partial);
	if (!len) {
		while ((filename = FS_NextFileFromFileList("videos/*.roq")) != NULL) {
			Com_Printf("%s\n", filename);
		}
		FS_NextFileFromFileList(NULL);
		return 0;
	}

	/* start from first file entry */
	FS_NextFileFromFileList(NULL);

	/* check for partial matches */
	while ((filename = FS_NextFileFromFileList("videos/*.roq")) != NULL) {
		if (!strncmp(partial, filename, len)) {
			Com_Printf("%s\n", filename);
			localMatch[matches++] = filename;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}
	FS_NextFileFromFileList(NULL);

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

void CIN_Init (void)
{
	int i;

	memset(&cin, 0, sizeof(cin));

	/* Register our commands */
	Cmd_AddCommand("cinematic", CIN_Cinematic_f, "Plays a cinematic");
	Cmd_AddParamCompleteFunction("cinematic", CIN_CompleteCommand);
	Cmd_AddCommand("cinematic_stop", CIN_CinematicStop_f, "Stops a cinematic");

	/* Build YUV table */
	for (i = 0; i < 256; i++) {
		const float f = (float)(i - 128);
		cin_yuvTable.vr[i] = Q_ftol(f * 1.40200f);
		cin_yuvTable.ug[i] = Q_ftol(f * 0.34414f);
		cin_yuvTable.vg[i] = Q_ftol(f * 0.71414f);
		cin_yuvTable.ub[i] = Q_ftol(f * 1.77200f);
	}

	/* Build square table */
	for (i = 0; i < 128; i++) {
		const short s = (short)(i * i);
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

void CIN_Shutdown (void)
{
	/* Unregister our commands */
	Cmd_RemoveCommand("cinematic");
	Cmd_RemoveCommand("cinematic_stop");

	/* If playing a cinematic, stop it */
	CIN_StopCinematic();
}
