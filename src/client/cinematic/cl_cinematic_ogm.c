/**
 * @file cl_cinematic_ogm.c
 *
 * @note Taken from World Of Padman Engine
 * @note This is a "ogm"-decoder to use a "better" (smaller files, higher resolutions) Cinematic-Format than roq
 * In this code "ogm" is only: ogg wrapper, vorbis audio, xvid video (or theora video)
 * (ogm(Ogg Media) in general is ogg wrapper with all kind of audio/video/subtitle/...)
 */

#include "cl_cinematic_ogm.h"
#include "cl_cinematic.h"
#include "../client.h"
#include "../renderer/r_draw.h"
#include "../sound/s_main.h"
#include "../sound/s_music.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#ifdef HAVE_XVID_H
#include <xvid.h>
#endif
#ifdef HAVE_THEORA_THEORA_H
#include <theora/theora.h>
#endif


typedef struct {
	long vr[256];
	long ug[256];
	long vg[256];
	long ub[256];
	long yy[256];
} yuvTable_t;

#define OGG_BUFFER_SIZE	(8 * 1024)

typedef struct
{
	qFILE ogmFile;

	ogg_sync_state oy; /**< sync and verify incoming physical bitstream */
	ogg_stream_state os_audio;
	ogg_stream_state os_video;

	vorbis_dsp_state vd; /**< central working state for the packet->PCM decoder */
	vorbis_info vi; /**< struct that stores all the static vorbis bitstream settings */
	vorbis_comment vc; /**< struct that stores all the bitstream user comments */

	/** @todo atm there isn't really a check for this (all "video" streams are handled
	 * as xvid, because xvid support more than one "subtype") */
	qboolean videoStreamIsXvid;
#ifdef HAVE_XVID_H
	xvid_dec_stats_t xvidDecodeStats;
	void *xvidDecodeHandle;
#endif
	qboolean videoStreamIsTheora;
#ifdef HAVE_THEORA_THEORA_H
	theora_info th_info;
	theora_comment th_comment;
	theora_state th_state;

	yuv_buffer th_yuvbuffer;
#endif

	byte* outputBuffer;
	int outputWidth;
	int outputHeight;
	int outputBufferSize; /**< in pixel (so "real bytesize" = outputBufferSize * 4) */
	int videoFrameCount; /**< output video-stream */
	ogg_int64_t Vtime_unit;
	int currentTime; /**< input from run-function */
	int startTime;

	musicStream_t musicStream;
} ogmCinematic_t;

static yuvTable_t ogmCin_yuvTable;

#define OGMCIN (*((ogmCinematic_t*)cin->codecData))

#define OGM_CINEMATIC_BPP 4

#ifdef HAVE_XVID_H

static int CIN_XVID_Init (cinematic_t *cin)
{
	int ret;

	xvid_gbl_init_t xvid_gbl_init;
	xvid_dec_create_t xvid_dec_create;

	/* Reset the structure with zeros */
	OBJZERO(xvid_gbl_init);
	OBJZERO(xvid_dec_create);

	/* Version */
	xvid_gbl_init.version = XVID_VERSION;

	xvid_gbl_init.cpu_flags = 0;
	xvid_gbl_init.debug = 0;

	xvid_global(NULL, 0, &xvid_gbl_init, NULL);

	/* Version */
	xvid_dec_create.version = XVID_VERSION;

	/* Image dimensions -- set to 0, xvidcore will resize when ever it is needed */
	xvid_dec_create.width = 0;
	xvid_dec_create.height = 0;

	ret = xvid_decore(NULL, XVID_DEC_CREATE, &xvid_dec_create, NULL);

	OGMCIN.xvidDecodeHandle = xvid_dec_create.handle;

	return ret;
}

static int CIN_XVID_Decode (cinematic_t *cin, unsigned char *input, int inputSize)
{
	int ret;

	xvid_dec_frame_t xvid_dec_frame;

	/* Reset all structures */
	OBJZERO(xvid_dec_frame);
	OBJZERO(OGMCIN.xvidDecodeStats);

	/* Set version */
	xvid_dec_frame.version = XVID_VERSION;
	OGMCIN.xvidDecodeStats.version = XVID_VERSION;

	/* No general flags to set */
	xvid_dec_frame.general = XVID_LOWDELAY;

	/* Input stream */
	xvid_dec_frame.bitstream = input;
	xvid_dec_frame.length = inputSize;

	/* Output frame structure */
	xvid_dec_frame.output.plane[0] = OGMCIN.outputBuffer;
	xvid_dec_frame.output.stride[0] = OGMCIN.outputWidth * OGM_CINEMATIC_BPP;
	if (OGMCIN.outputBuffer == NULL)
		xvid_dec_frame.output.csp = XVID_CSP_NULL;
	else
		xvid_dec_frame.output.csp = XVID_CSP_RGBA;

	ret = xvid_decore(OGMCIN.xvidDecodeHandle, XVID_DEC_DECODE, &xvid_dec_frame, &OGMCIN.xvidDecodeStats);

	return ret;
}

static int CIN_XVID_Shutdown (cinematic_t *cin)
{
	int ret = 0;

	if (OGMCIN.xvidDecodeHandle)
		ret = xvid_decore(OGMCIN.xvidDecodeHandle, XVID_DEC_DESTROY, NULL, NULL);

	return ret;
}
#endif

/**
 * @returns !0 -> no data transferred
 */
static int CIN_OGM_LoadBlockToSync (cinematic_t *cin)
{
	int r = -1;

	if (OGMCIN.ogmFile.f || OGMCIN.ogmFile.z) {
		char *buffer = ogg_sync_buffer(&OGMCIN.oy, OGG_BUFFER_SIZE);
		const int bytes = FS_Read(buffer, OGG_BUFFER_SIZE, &OGMCIN.ogmFile);
		if (bytes > 0)
			ogg_sync_wrote(&OGMCIN.oy, bytes);

		r = (bytes == 0);
	}

	return r;
}

/**
 * @return !0 -> no data transferred (or not for all streams)
 */
static int CIN_OGM_LoadPagesToStream (cinematic_t *cin)
{
	int r = -1;
	int audioPages = 0;
	int videoPages = 0;
	ogg_stream_state* osptr = NULL;
	ogg_page og;

	while (!audioPages || !videoPages) {
		if (ogg_sync_pageout(&OGMCIN.oy, &og) != 1)
			break;

		if (OGMCIN.os_audio.serialno == ogg_page_serialno(&og)) {
			osptr = &OGMCIN.os_audio;
			++audioPages;
		}
		if (OGMCIN.os_video.serialno == ogg_page_serialno(&og)) {
			osptr = &OGMCIN.os_video;
			++videoPages;
		}

		if (osptr != NULL) {
			ogg_stream_pagein(osptr, &og);
		}
	}

	if (audioPages && videoPages)
		r = 0;

	return r;
}

#define SIZEOF_RAWBUFF SAMPLE_SIZE * 1024
static byte rawBuffer[SIZEOF_RAWBUFF];

/**
 * @return true if audio wants more packets
 */
static qboolean CIN_OGM_LoadAudioFrame (cinematic_t *cin)
{
	vorbis_block vb;

	OBJZERO(vb);
	vorbis_block_init(&OGMCIN.vd, &vb);

	while (OGMCIN.currentTime > (int) (OGMCIN.vd.granulepos * 1000 / OGMCIN.vi.rate)) {
		float **pcm;
		const int samples = vorbis_synthesis_pcmout(&OGMCIN.vd, &pcm);

		if (samples > 0) {
			/* vorbis -> raw */
			const int width = 2;
			const int channel = 2;
			int samplesNeeded = sizeof(rawBuffer) / (width * channel);
			const float *left = pcm[0];
			const float *right = (OGMCIN.vi.channels > 1) ? pcm[1] : pcm[0];
			short *ptr = (short*)rawBuffer;
			int i;

			if (samples < samplesNeeded)
				samplesNeeded = samples;

			for (i = 0; i < samplesNeeded; ++i, ptr += channel) {
				ptr[0] = (left[i] >= -1.0f && left[i] <= 1.0f) ? left[i] * 32767.f : 32767 * ((left[i] > 0.0f) - (left[i] < 0.0f));
				ptr[1] = (right[i] >= -1.0f && right[i] <= 1.0f) ? right[i] * 32767.f : 32767 * ((right[i] > 0.0f) - (right[i] < 0.0f));
			}

			/* tell libvorbis how many samples we actually consumed */
			vorbis_synthesis_read(&OGMCIN.vd, i);

			if (!cin->noSound)
				M_AddToSampleBuffer(&OGMCIN.musicStream, OGMCIN.vi.rate, i, rawBuffer);
		} else {
			ogg_packet op;
			/* op -> vorbis */
			if (ogg_stream_packetout(&OGMCIN.os_audio, &op)) {
				if (vorbis_synthesis(&vb, &op) == 0)
					vorbis_synthesis_blockin(&OGMCIN.vd, &vb);
			} else
				break;
		}
	}

	vorbis_block_clear(&vb);

	return OGMCIN.currentTime > (int)(OGMCIN.vd.granulepos * 1000 / OGMCIN.vi.rate);
}

/**
 * @return 1 -> loaded a new frame (OGMCIN.outputBuffer points to the actual frame), 0 -> no new frame
 * <0 -> error
 */
#ifdef HAVE_XVID_H
static int CIN_XVID_LoadVideoFrame (cinematic_t *cin)
{
	int r = 0;
	ogg_packet op;

	OBJZERO(op);

	while (!r && (ogg_stream_packetout(&OGMCIN.os_video, &op))) {
		int usedBytes = CIN_XVID_Decode(cin, op.packet, op.bytes);
		if (OGMCIN.xvidDecodeStats.type == XVID_TYPE_VOL) {
			if (OGMCIN.outputWidth != OGMCIN.xvidDecodeStats.data.vol.width || OGMCIN.outputHeight
					!= OGMCIN.xvidDecodeStats.data.vol.height) {
				OGMCIN.outputWidth = OGMCIN.xvidDecodeStats.data.vol.width;
				OGMCIN.outputHeight = OGMCIN.xvidDecodeStats.data.vol.height;
				Com_DPrintf(DEBUG_CLIENT, "[XVID]new resolution %dx%d\n", OGMCIN.outputWidth, OGMCIN.outputHeight);
			}

			if (OGMCIN.outputBufferSize < OGMCIN.xvidDecodeStats.data.vol.width * OGMCIN.xvidDecodeStats.data.vol.height) {
				OGMCIN.outputBufferSize = OGMCIN.xvidDecodeStats.data.vol.width * OGMCIN.xvidDecodeStats.data.vol.height;

				/* Free old output buffer*/
				if (OGMCIN.outputBuffer)
					Mem_Free(OGMCIN.outputBuffer);

				/* Allocate the new buffer */
				OGMCIN.outputBuffer = (byte*) Mem_PoolAlloc(OGMCIN.outputBufferSize * OGM_CINEMATIC_BPP, cl_genericPool, 0);
				if (OGMCIN.outputBuffer == NULL) {
					OGMCIN.outputBufferSize = 0;
					r = -2;
					break;
				}
			}

			/* use the rest of this packet */
			usedBytes += CIN_XVID_Decode(cin, op.packet + usedBytes, op.bytes - usedBytes);
		}

		/* we got a real output frame ... */
		if (OGMCIN.xvidDecodeStats.type > 0) {
			r = 1;

			++OGMCIN.videoFrameCount;
		}
	}

	return r;
}
#endif

#ifdef HAVE_THEORA_THEORA_H
/**
 * @brief how many >> are needed to make y == x (shifting y >> i)
 * @return -1 -> no match, >=0 -> number of shifts
 */
static int CIN_THEORA_FindSizeShift (int x, int y)
{
	int i;

	for (i = 0; (y >> i); ++i)
		if (x == (y >> i))
			return i;

	return -1;
}


/**
 * @brief Clamps integer value into byte
 */
static inline byte CIN_THEORA_ClampByte (int value)
{
	if (value < 0)
		return 0;

	if (value > 255)
		return 255;

	return value;
}

static void CIN_THEORA_FrameYUVtoRGB24 (const unsigned char* y, const unsigned char* u, const unsigned char* v, int width,
		int height, int y_stride, int uv_stride, int yWShift, int uvWShift, int yHShift, int uvHShift,
		uint32_t* output)
{
	int i, j;

	for (j = 0; j < height; ++j) {
		for (i = 0; i < width; ++i) {
			const long YY = (long) (ogmCin_yuvTable.yy[(y[(i >> yWShift) + (j >> yHShift) * y_stride])]);
			const int uvI = (i >> uvWShift) + (j >> uvHShift) * uv_stride;

			const byte r = CIN_THEORA_ClampByte((YY + ogmCin_yuvTable.vr[v[uvI]]) >> 6);
			const byte g = CIN_THEORA_ClampByte((YY + ogmCin_yuvTable.ug[u[uvI]] + ogmCin_yuvTable.vg[v[uvI]]) >> 6);
			const byte b = CIN_THEORA_ClampByte((YY + ogmCin_yuvTable.ub[u[uvI]]) >> 6);

			const uint32_t rgb24 = LittleLong(r | (g << 8) | (b << 16) | (255 << 24));
			*output++ = rgb24;
		}
	}
}

static int CIN_THEORA_NextNeededFrame (cinematic_t *cin)
{
	return (int) (OGMCIN.currentTime * (ogg_int64_t) 10000 / OGMCIN.Vtime_unit);
}

/**
 * @return 1 -> loaded a new frame (OGMCIN.outputBuffer points to the actual frame)
 * 0 -> no new frame <0 -> error
 */
static int CIN_THEORA_LoadVideoFrame (cinematic_t *cin)
{
	int r = 0;
	ogg_packet op;

	OBJZERO(op);

	while (!r && (ogg_stream_packetout(&OGMCIN.os_video, &op))) {
		ogg_int64_t th_frame;
		theora_decode_packetin(&OGMCIN.th_state, &op);

		th_frame = theora_granule_frame(&OGMCIN.th_state, OGMCIN.th_state.granulepos);

		if ((OGMCIN.videoFrameCount < th_frame && th_frame >= CIN_THEORA_NextNeededFrame(cin)) || !OGMCIN.outputBuffer) {
			int yWShift, uvWShift;
			int yHShift, uvHShift;

			if (theora_decode_YUVout(&OGMCIN.th_state, &OGMCIN.th_yuvbuffer))
				continue;

			if (OGMCIN.outputWidth != OGMCIN.th_info.width || OGMCIN.outputHeight != OGMCIN.th_info.height) {
				OGMCIN.outputWidth = OGMCIN.th_info.width;
				OGMCIN.outputHeight = OGMCIN.th_info.height;
				Com_DPrintf(DEBUG_CLIENT, "[Theora(ogg)]new resolution %dx%d\n", OGMCIN.outputWidth, OGMCIN.outputHeight);
			}

			if (OGMCIN.outputBufferSize < OGMCIN.th_info.width * OGMCIN.th_info.height) {
				OGMCIN.outputBufferSize = OGMCIN.th_info.width * OGMCIN.th_info.height;

				/* Free old output buffer*/
				if (OGMCIN.outputBuffer)
					Mem_Free(OGMCIN.outputBuffer);

				/* Allocate the new buffer */
				OGMCIN.outputBuffer = (byte*) Mem_PoolAlloc(OGMCIN.outputBufferSize * OGM_CINEMATIC_BPP, cl_genericPool, 0);
				if (OGMCIN.outputBuffer == NULL) {
					OGMCIN.outputBufferSize = 0;
					r = -2;
					break;
				}
			}

			yWShift = CIN_THEORA_FindSizeShift(OGMCIN.th_yuvbuffer.y_width, OGMCIN.th_info.width);
			uvWShift = CIN_THEORA_FindSizeShift(OGMCIN.th_yuvbuffer.uv_width, OGMCIN.th_info.width);
			yHShift = CIN_THEORA_FindSizeShift(OGMCIN.th_yuvbuffer.y_height, OGMCIN.th_info.height);
			uvHShift = CIN_THEORA_FindSizeShift(OGMCIN.th_yuvbuffer.uv_height, OGMCIN.th_info.height);

			if (yWShift < 0 || uvWShift < 0 || yHShift < 0 || uvHShift < 0) {
				Com_Printf("[Theora] unexpected resolution in a yuv-frame\n");
				r = -1;
			} else {
				CIN_THEORA_FrameYUVtoRGB24(OGMCIN.th_yuvbuffer.y, OGMCIN.th_yuvbuffer.u, OGMCIN.th_yuvbuffer.v,
						OGMCIN.th_info.width, OGMCIN.th_info.height, OGMCIN.th_yuvbuffer.y_stride,
						OGMCIN.th_yuvbuffer.uv_stride, yWShift, uvWShift, yHShift, uvHShift,
						(uint32_t*) OGMCIN.outputBuffer);

				r = 1;
				OGMCIN.videoFrameCount = th_frame;
			}
		}
	}

	return r;
}
#endif

/**
 * @return 1 -> loaded a new frame (OGMCIN.outputBuffer points to the actual frame), 0 -> no new frame
 * <0 -> error
 */
static int CIN_OGM_LoadVideoFrame (cinematic_t *cin)
{
#ifdef HAVE_XVID_H
	if (OGMCIN.videoStreamIsXvid)
		return CIN_XVID_LoadVideoFrame(cin);
#endif
#ifdef HAVE_THEORA_THEORA_H
	if (OGMCIN.videoStreamIsTheora)
		return CIN_THEORA_LoadVideoFrame(cin);
#endif

	/* if we come to this point, there will be no codec that use the stream content ... */
	if (OGMCIN.os_video.serialno) {
		ogg_packet op;

		while (ogg_stream_packetout(&OGMCIN.os_video, &op))
			;
	}

	return 1;
}

/**
 * @return true => noDataTransfered
 */
static qboolean CIN_OGM_LoadFrame (cinematic_t *cin)
{
	qboolean anyDataTransferred = qtrue;
	qboolean needVOutputData = qtrue;
	qboolean audioWantsMoreData = qfalse;
	int status;

	while (anyDataTransferred && (needVOutputData || audioWantsMoreData)) {
		anyDataTransferred = qfalse;
		if (needVOutputData && (status = CIN_OGM_LoadVideoFrame(cin))) {
			needVOutputData = qfalse;
			if (status > 0)
				anyDataTransferred = qtrue;
			else
				/* error (we don't need any videodata and we had no transferred) */
				anyDataTransferred = qfalse;
		}

		if (needVOutputData || audioWantsMoreData) {
			/* try to transfer Pages to the audio- and video-Stream */
			if (CIN_OGM_LoadPagesToStream(cin))
				/* try to load a datablock from file */
				anyDataTransferred |= !CIN_OGM_LoadBlockToSync(cin);
			else
				/* successful loadPagesToStreams() */
				anyDataTransferred = qtrue;
		}

		/* load all audio after loading new pages ... */
		if (OGMCIN.videoFrameCount > 1)
			/* wait some videoframes (it's better to have some delay, than a laggy sound) */
			audioWantsMoreData = CIN_OGM_LoadAudioFrame(cin);
	}

	return !anyDataTransferred;
}

#ifdef HAVE_XVID_H
/** @brief from VLC ogg.c (http://trac.videolan.org/vlc/browser/modules/demux/ogg.c) */
typedef struct
{
	char streamtype[8];
	char subtype[4];

	ogg_int32_t size; /* size of the structure */

	/* in 10^-7 seconds (dT between frames) */
	ogg_int64_t time_unit; /* in reference time */
	ogg_int64_t samples_per_unit;
	ogg_int32_t default_len; /* in media time */

	ogg_int32_t buffersize;
	ogg_int16_t bits_per_sample;
	union
	{
		struct
		{
			ogg_int32_t width;
			ogg_int32_t height;
		} stream_header_video;

		struct
		{
			ogg_int16_t channels;
			ogg_int16_t blockalign;
			ogg_int32_t avgbytespersec;
		} stream_header_audio;
	} sh;
} stream_header_t;
#endif

/**
 * @return 0 -> no problem
 * @todo vorbis/theora-header & init in sub-functions
 * @todo "clean" error-returns ...
 */
int CIN_OGM_OpenCinematic (cinematic_t *cin, const char* filename)
{
	int status;
	ogg_page og;
	ogg_packet op;
	int i;

	if (cin->codecData && (OGMCIN.ogmFile.f || OGMCIN.ogmFile.z)) {
		Com_Printf("WARNING: it seams there was already a ogm running, it will be killed to start %s\n", filename);
		CIN_OGM_CloseCinematic(cin);
	}

	/* alloc memory for decoding of this video */
	assert(cin->codecData == NULL);
	cin->codecData = Mem_PoolAlloc(sizeof(OGMCIN), vid_genericPool, 0);
	memset(cin->codecData, 0, sizeof(ogmCinematic_t));

	if (FS_OpenFile(filename, &OGMCIN.ogmFile, FILE_READ) == -1) {
		Com_Printf("Can't open ogm-file for reading (%s)\n", filename);
		return -1;
	}

	cin->cinematicType = CINEMATIC_TYPE_OGM;
	OGMCIN.startTime = CL_Milliseconds();

	ogg_sync_init(&OGMCIN.oy); /* Now we can read pages */

	/** @todo FIXME? can serialno be 0 in ogg? (better way to check initialized?) */
	/** @todo support for more than one audio stream? / detect files with one stream(or without correct ones) */
	while (!OGMCIN.os_audio.serialno || !OGMCIN.os_video.serialno) {
		if (ogg_sync_pageout(&OGMCIN.oy, &og) == 1) {
			if (og.body_len >= 7 && !memcmp(og.body, "\x01vorbis", 7)) {
				if (OGMCIN.os_audio.serialno) {
					Com_Printf("more than one audio stream, in ogm-file(%s) ... we will stay at the first one\n",
							filename);
				} else {
					ogg_stream_init(&OGMCIN.os_audio, ogg_page_serialno(&og));
					ogg_stream_pagein(&OGMCIN.os_audio, &og);
				}
			}
#ifdef HAVE_THEORA_THEORA_H
			else if (og.body_len >= 7 && !memcmp(og.body, "\x80theora", 7)) {
				if (OGMCIN.os_video.serialno) {
					Com_Printf("more than one video stream, in ogm-file(%s) ... we will stay at the first one\n",
							filename);
				} else {
					OGMCIN.videoStreamIsTheora = qtrue;
					ogg_stream_init(&OGMCIN.os_video, ogg_page_serialno(&og));
					ogg_stream_pagein(&OGMCIN.os_video, &og);
				}
			}
#endif
#ifdef HAVE_XVID_H
			else if (strstr((const char*) (og.body + 1), "video")) { /** @todo better way to find video stream */
				if (OGMCIN.os_video.serialno) {
					Com_Printf("more than one video stream, in ogm-file(%s) ... we will stay at the first one\n",
							filename);
				} else {
					stream_header_t* sh;

					OGMCIN.videoStreamIsXvid = qtrue;

					sh = (stream_header_t*) (og.body + 1);

					OGMCIN.Vtime_unit = sh->time_unit;

					ogg_stream_init(&OGMCIN.os_video, ogg_page_serialno(&og));
					ogg_stream_pagein(&OGMCIN.os_video, &og);
				}
			}
#endif
		} else if (CIN_OGM_LoadBlockToSync(cin))
			break;
	}

	if (OGMCIN.videoStreamIsXvid && OGMCIN.videoStreamIsTheora) {
		Com_Printf("Found \"video\"- and \"theora\"-stream, ogm-file (%s)\n", filename);
		return -2;
	}

	if (!OGMCIN.os_audio.serialno) {
		Com_Printf("Haven't found an audio (vorbis) stream in ogm-file (%s)\n", filename);
		return -2;
	}
	if (!OGMCIN.os_video.serialno) {
		Com_Printf("Haven't found a video stream in ogm-file (%s)\n", filename);
		return -3;
	}

	/* load vorbis header */
	vorbis_info_init(&OGMCIN.vi);
	vorbis_comment_init(&OGMCIN.vc);
	i = 0;
	while (i < 3) {
		status = ogg_stream_packetout(&OGMCIN.os_audio, &op);
		if (status < 0) {
			Com_Printf("Corrupt ogg packet while loading vorbis-headers, ogm-file(%s)\n", filename);
			return -8;
		}
		if (status > 0) {
			status = vorbis_synthesis_headerin(&OGMCIN.vi, &OGMCIN.vc, &op);
			if (i == 0 && status < 0) {
				Com_Printf("This Ogg bitstream does not contain Vorbis audio data, ogm-file(%s)\n", filename);
				return -9;
			}
			++i;
		} else if (CIN_OGM_LoadPagesToStream(cin)) {
			if (CIN_OGM_LoadBlockToSync(cin)) {
				Com_Printf("Couldn't find all vorbis headers before end of ogm-file (%s)\n", filename);
				return -10;
			}
		}
	}

	vorbis_synthesis_init(&OGMCIN.vd, &OGMCIN.vi);

#ifdef HAVE_XVID_H
	status = CIN_XVID_Init(cin);
	if (status) {
		Com_Printf("[Xvid]Decore INIT problem, return value %d(ogm-file: %s)\n", status, filename);
		return -4;
	}
#endif

#ifdef HAVE_THEORA_THEORA_H
	if (OGMCIN.videoStreamIsTheora) {
		theora_info_init(&OGMCIN.th_info);
		theora_comment_init(&OGMCIN.th_comment);

		i = 0;
		while (i < 3) {
			status = ogg_stream_packetout(&OGMCIN.os_video, &op);
			if (status < 0) {
				Com_Printf("Corrupt ogg packet while loading theora-headers, ogm-file(%s)\n", filename);

				return -8;
			}
			if (status > 0) {
				status = theora_decode_header(&OGMCIN.th_info, &OGMCIN.th_comment, &op);
				if (i == 0 && status != 0) {
					Com_Printf("This Ogg bitstream does not contain theora data, ogm-file(%s)\n", filename);

					return -9;
				}
				++i;
			} else if (CIN_OGM_LoadPagesToStream(cin)) {
				if (CIN_OGM_LoadBlockToSync(cin)) {
					Com_Printf("Couldn't find all theora headers before end of ogm-file (%s)\n", filename);

					return -10;
				}
			}
		}

		theora_decode_init(&OGMCIN.th_state, &OGMCIN.th_info);
		OGMCIN.Vtime_unit = ((ogg_int64_t) OGMCIN.th_info.fps_denominator * 1000 * 10000 / OGMCIN.th_info.fps_numerator);
	}
#endif

	M_PlayMusicStream(&OGMCIN.musicStream);

	return 0;
}

/**
 * @sa R_UploadData
 */
static void CIN_OGM_DrawCinematic (cinematic_t *cin)
{
	int texnum;

	assert(cin->status != CIN_STATUS_NONE);

	if (!OGMCIN.outputBuffer)
		return;
	texnum = R_UploadData("***cinematic***", OGMCIN.outputBuffer, OGMCIN.outputWidth, OGMCIN.outputHeight);
	R_DrawTexture(texnum, cin->x, cin->y, cin->w, cin->h);
}

/**
 * @return true if the cinematic is still running, false otherwise
 */
qboolean CIN_OGM_RunCinematic (cinematic_t *cin)
{
	/* no video stream found */
	if (!OGMCIN.os_video.serialno)
		return qfalse;

	OGMCIN.currentTime = CL_Milliseconds() - OGMCIN.startTime;

	while (!OGMCIN.videoFrameCount || OGMCIN.currentTime + 20 >= (int) (OGMCIN.videoFrameCount * OGMCIN.Vtime_unit / 10000)) {
		if (CIN_OGM_LoadFrame(cin))
			return qfalse;
	}

	CIN_OGM_DrawCinematic(cin);

	return qtrue;
}

void CIN_OGM_CloseCinematic (cinematic_t *cin)
{
#ifdef HAVE_XVID_H
	/** @todo is it at the right place? StopCinematic mean we only stop 1 cinematic */
	CIN_XVID_Shutdown(cin);
#endif

#ifdef HAVE_THEORA_THEORA_H
	theora_clear(&OGMCIN.th_state);
	theora_comment_clear(&OGMCIN.th_comment);
	theora_info_clear(&OGMCIN.th_info);
#endif

	M_StopMusicStream(&OGMCIN.musicStream);

	if (OGMCIN.outputBuffer)
		Mem_Free(OGMCIN.outputBuffer);
	OGMCIN.outputBuffer = NULL;

	vorbis_dsp_clear(&OGMCIN.vd);
	vorbis_comment_clear(&OGMCIN.vc);
	vorbis_info_clear(&OGMCIN.vi); /* must be called last (comment from vorbis example code) */

	ogg_stream_clear(&OGMCIN.os_audio);
	ogg_stream_clear(&OGMCIN.os_video);

	ogg_sync_clear(&OGMCIN.oy);

	FS_CloseFile(&OGMCIN.ogmFile);

	/* free data allocated for decodage */
	Mem_Free(cin->codecData);
	cin->codecData = NULL;
}

void CIN_OGM_Init (void)
{
	long i;
	const float t_ub = (1.77200f / 2.0f) * (float)(1 << 6) + 0.5f;
	const float t_vr = (1.40200f / 2.0f) * (float)(1 << 6) + 0.5f;
	const float t_ug = (0.34414f / 2.0f) * (float)(1 << 6) + 0.5f;
	const float t_vg = (0.71414f / 2.0f) * (float)(1 << 6) + 0.5f;

	for (i = 0; i < 256; i++) {
		const float x = (float)(2 * i - 255);

		ogmCin_yuvTable.ub[i] = (long)(( t_ub * x) + (1 << 5));
		ogmCin_yuvTable.vr[i] = (long)(( t_vr * x) + (1 << 5));
		ogmCin_yuvTable.ug[i] = (long)((-t_ug * x));
		ogmCin_yuvTable.vg[i] = (long)((-t_vg * x) + (1 << 5));
		ogmCin_yuvTable.yy[i] = (long)((i << 6) | (i >> 2));
	}
}
