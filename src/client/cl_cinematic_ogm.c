/**
 * @file cl_cinematic_ogm.c
 * @note Taken from World Of Padman Engine
 * @note This is a "ogm"-decoder to use a "better"(smaller files,higher resolutions) Cinematic-Format than roq
 * In this code "ogm" is only: ogg wrapper, vorbis audio, xvid video (or theora video)
 * (ogm(Ogg Media) in general is ogg wrapper with all kind of audio/video/subtitle/...)
 * ... infos used for this src:
 * xvid:
 * <li>examples/xvid_decraw.c
 * <li>xvid.h
 * ogg/vobis:
 * <li>decoder_example.c (libvorbis src)
 * <li>libogg Documentation ( http://www.xiph.org/ogg/doc/libogg/ )
 * <li>VLC ogg demux ( http://trac.videolan.org/vlc/browser/trunk/modules/demux/ogg.c )
 * theora:
 * <li>theora doxygen docs (1.0beta1)
 */

#include "client.h"
#include "cl_cinematic.h"
#include "cl_cinematic_ogm.h"
#include "renderer/r_draw.h"

#if defined(USE_CODEC_VORBIS) && (defined(USE_CIN_XVID) || defined(USE_CIN_THEORA))

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#ifdef USE_CIN_XVID
#include <xvid.h>
#endif
#ifdef USE_CIN_THEORA
#include <theora/theora.h>
#endif

#include "client.h"
#include "cl_sound.h"

#define OGG_BUFFER_SIZE	8 * 1024

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
#ifdef USE_CIN_XVID
	xvid_dec_stats_t xvidDecodeStats;
	void *xvidDecodeHandle;
#endif
	qboolean videoStreamIsTheora;
#ifdef USE_CIN_THEORA
	theora_info th_info;
	theora_comment th_comment;
	theora_state th_state;

	yuv_buffer th_yuvbuffer;
#endif

	byte* outputBuffer;
	int outputWidht;
	int outputHeight;
	int outputBufferSize; /**< in pixel (so "real bytesize" = outputBufferSize * 4) */
	int videoFrameCount; /**< output video-stream */
	ogg_int64_t Vtime_unit;
	int currentTime; /**< input from run-function */
} ogmCinematic_t;

static ogmCinematic_t ogmCin;

static int CIN_THEORA_NextNeededFrame(void);

#ifdef USE_CIN_XVID

#define OGM_CINEMATIC_BPP 4

static int CIN_XVID_Init (void)
{
	int ret;

	xvid_gbl_init_t xvid_gbl_init;
	xvid_dec_create_t xvid_dec_create;

	/* Reset the structure with zeros */
	memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init_t));
	memset(&xvid_dec_create, 0, sizeof(xvid_dec_create_t));

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

	ogmCin.xvidDecodeHandle = xvid_dec_create.handle;

	return ret;
}

static int CIN_XVID_Decode (unsigned char *input, int inputSize)
{
	int ret;

	xvid_dec_frame_t xvid_dec_frame;

	/* Reset all structures */
	memset(&xvid_dec_frame, 0, sizeof(xvid_dec_frame_t));
	memset(&ogmCin.xvidDecodeStats, 0, sizeof(xvid_dec_stats_t));

	/* Set version */
	xvid_dec_frame.version = XVID_VERSION;
	ogmCin.xvidDecodeStats.version = XVID_VERSION;

	/* No general flags to set */
	xvid_dec_frame.general = XVID_LOWDELAY;

	/* Input stream */
	xvid_dec_frame.bitstream = input;
	xvid_dec_frame.length = inputSize;

	/* Output frame structure */
	xvid_dec_frame.output.plane[0] = ogmCin.outputBuffer;
	xvid_dec_frame.output.stride[0] = ogmCin.outputWidht * OGM_CINEMATIC_BPP;
	if (ogmCin.outputBuffer == NULL)
		xvid_dec_frame.output.csp = XVID_CSP_NULL;
	else
		xvid_dec_frame.output.csp = XVID_CSP_RGBA;

	ret = xvid_decore(ogmCin.xvidDecodeHandle, XVID_DEC_DECODE, &xvid_dec_frame, &ogmCin.xvidDecodeStats);

	return (ret);
}

static int CIN_XVID_Shutdown (void)
{
	int ret = 0;

	if (ogmCin.xvidDecodeHandle)
		ret = xvid_decore(ogmCin.xvidDecodeHandle, XVID_DEC_DESTROY, NULL, NULL);

	return (ret);
}
#endif

/**
 * @returns !0 -> no data transferred
 */
static int CIN_OGM_LoadBlockToSync (void)
{
	int r = -1;

	if (ogmCin.ogmFile.f || ogmCin.ogmFile.z) {
		char *buffer = ogg_sync_buffer(&ogmCin.oy, OGG_BUFFER_SIZE);
		const int bytes = FS_Read(buffer, OGG_BUFFER_SIZE, &ogmCin.ogmFile);
		ogg_sync_wrote(&ogmCin.oy, bytes);

		r = (bytes == 0);
	}

	return r;
}

/**
 * @return !0 -> no data transferred (or not for all Streams)
 */
static int CIN_OGM_LoadPagesToStream (void)
{
	int r = -1;
	int audioPages = 0;
	int videoPages = 0;
	ogg_stream_state* osptr = NULL;
	ogg_page og;

	while (!audioPages || !videoPages) {
		if (ogg_sync_pageout(&ogmCin.oy, &og) != 1)
			break;

		if (ogmCin.os_audio.serialno == ogg_page_serialno(&og)) {
			osptr = &ogmCin.os_audio;
			++audioPages;
		}
		if (ogmCin.os_video.serialno == ogg_page_serialno(&og)) {
			osptr = &ogmCin.os_video;
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

#define SIZEOF_RAWBUFF 4 * 1024
static byte rawBuffer[SIZEOF_RAWBUFF];

#define MIN_AUDIO_PRELOAD 400 /* in ms */
#define MAX_AUDIO_PRELOAD 500 /* in ms */

/**
 * @return true if audio wants more packets
 */
static qboolean CIN_OGM_LoadAudioFrame (void)
{
	qboolean anyDataTransfered = qtrue;
	float **pcm;
	float *right, *left;
	int samples;
	int i;
	ogg_packet op;
	vorbis_block vb;

	memset(&op, 0, sizeof(op));
	memset(&vb, 0, sizeof(vb));
	vorbis_block_init(&ogmCin.vd, &vb);

	while (anyDataTransfered && ogmCin.currentTime + MAX_AUDIO_PRELOAD > (int) (ogmCin.vd.granulepos * 1000
			/ ogmCin.vi.rate)) {
		anyDataTransfered = qfalse;

		if ((samples = vorbis_synthesis_pcmout(&ogmCin.vd, &pcm)) > 0) {
			/* vorbis -> raw */
			short *ptr = (short*) rawBuffer;
			int samplesNeeded = (SIZEOF_RAWBUFF) / (2 * 2 ); /* (width * channel) */
			if (samples < samplesNeeded)
				samplesNeeded = samples;

			left = pcm[0];
			right = (ogmCin.vi.channels > 1) ? pcm[1] : pcm[0];
			for (i = 0; i < samplesNeeded; ++i, ptr += 2) {
				ptr[0] = (left[i] >= -1.0f && left[i] <= 1.0f) ? left[i] * 32767.f : 32767 * ((left[i] > 0.0f) - (left[i] < 0.0f));
				ptr[1] = (right[i] >= -1.0f && right[i] <= 1.0f) ? right[i] * 32767.f : 32767 * ((right[i] > 0.0f) - (right[i] < 0.0f));
			}

			if (i > 0) {
				/* tell libvorbis how many samples we actually consumed */
				vorbis_synthesis_read(&ogmCin.vd, i);

				S_PlaySoundFromMem((const short*)rawBuffer, SIZEOF_RAWBUFF, ogmCin.vi.rate, 2, -1);

				anyDataTransfered = qtrue;
			}
		}

		if (!anyDataTransfered) {
			/* op -> vorbis */
			if (ogg_stream_packetout(&ogmCin.os_audio, &op)) {
				if (vorbis_synthesis(&vb, &op) == 0)
					vorbis_synthesis_blockin(&ogmCin.vd, &vb);
				anyDataTransfered = qtrue;
			}
		}
	}

	vorbis_block_clear(&vb);

	if (ogmCin.currentTime + MIN_AUDIO_PRELOAD > (int)(ogmCin.vd.granulepos * 1000 / ogmCin.vi.rate))
		return qtrue;
	else
		return qfalse;
}

/**
 * @return 1 -> loaded a new Frame (ogmCin.outputBuffer points to the actual frame), 0 -> no new Frame
 * <0 -> error
 */
#ifdef USE_CIN_XVID
static int CIN_XVID_LoadVideoFrame (void)
{
	int r = 0;
	ogg_packet op;

	memset(&op, 0, sizeof(op));

	while (!r && (ogg_stream_packetout(&ogmCin.os_video, &op))) {
		int usedBytes = CIN_XVID_Decode(op.packet, op.bytes);
		if (ogmCin.xvidDecodeStats.type == XVID_TYPE_VOL) {
			if (ogmCin.outputWidht != ogmCin.xvidDecodeStats.data.vol.width || ogmCin.outputHeight
					!= ogmCin.xvidDecodeStats.data.vol.height) {
				ogmCin.outputWidht = ogmCin.xvidDecodeStats.data.vol.width;
				ogmCin.outputHeight = ogmCin.xvidDecodeStats.data.vol.height;
				Com_DPrintf(DEBUG_CLIENT, "[XVID]new resolution %dx%d\n", ogmCin.outputWidht, ogmCin.outputHeight);
			}

			if (ogmCin.outputBufferSize < ogmCin.xvidDecodeStats.data.vol.width * ogmCin.xvidDecodeStats.data.vol.height) {
				ogmCin.outputBufferSize = ogmCin.xvidDecodeStats.data.vol.width * ogmCin.xvidDecodeStats.data.vol.height;

				/* Free old output buffer*/
				if (ogmCin.outputBuffer)
					Mem_Free(ogmCin.outputBuffer);

				/* Allocate the new buffer */
				ogmCin.outputBuffer = (byte*) Mem_PoolAlloc(ogmCin.outputBufferSize * OGM_CINEMATIC_BPP, cl_genericPool, 0);
				if (ogmCin.outputBuffer == NULL) {
					ogmCin.outputBufferSize = 0;
					r = -2;
					break;
				}
			}

			/* use the rest of this packet */
			usedBytes += CIN_XVID_Decode(op.packet + usedBytes, op.bytes - usedBytes);
		}

		/* we got a real output frame ... */
		if (ogmCin.xvidDecodeStats.type > 0) {
			r = 1;

			++ogmCin.videoFrameCount;
		}
	}

	return r;
}
#endif

#ifdef USE_CIN_THEORA
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

static void CIN_THEORA_FrameYUVtoRGB24 (const unsigned char* y, const unsigned char* u, const unsigned char* v, int width,
		int height, int y_stride, int uv_stride, int yWShift, int uvWShift, int yHShift, int uvHShift,
		unsigned int* output)
{
	int i, j;

	for (j = 0; j < height; ++j) {
		for (i = 0; i < width; ++i) {
			const long YY = (long) (cin.yuvTable.yy[(y[(i >> yWShift) + (j >> yHShift) * y_stride])]);
			const int uvI = (i >> uvWShift) + (j >> uvHShift) * uv_stride;

			int r = (YY + cin.yuvTable.vr[v[uvI]]) >> 6;
			int g = (YY + cin.yuvTable.ug[u[uvI]] + cin.yuvTable.vg[v[uvI]]) >> 6;
			int b = (YY + cin.yuvTable.ub[u[uvI]]) >> 6;

			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;
			if (r > 255)
				r = 255;
			if (g > 255)
				g = 255;
			if (b > 255)
				b = 255;

			*output = LittleLong((r) | (g << 8) | (b << 16) | (255 << 24));
			++output;
		}
	}
}

/**
 * @return 1 -> loaded a new frame (ogmCin.outputBuffer points to the actual frame)
 * 0 -> no new frame <0 -> error
 */
static int CIN_THEORA_LoadVideoFrame (void)
{
	int r = 0;
	ogg_packet op;

	memset(&op, 0, sizeof(op));

	while (!r && (ogg_stream_packetout(&ogmCin.os_video, &op))) {
		ogg_int64_t th_frame;
		theora_decode_packetin(&ogmCin.th_state, &op);

		th_frame = theora_granule_frame(&ogmCin.th_state, ogmCin.th_state.granulepos);

		if ((ogmCin.videoFrameCount < th_frame && th_frame >= CIN_THEORA_NextNeededFrame()) || !ogmCin.outputBuffer) {
			int yWShift, uvWShift;
			int yHShift, uvHShift;

			if (theora_decode_YUVout(&ogmCin.th_state, &ogmCin.th_yuvbuffer))
				continue;

			if (ogmCin.outputWidht != ogmCin.th_info.width || ogmCin.outputHeight != ogmCin.th_info.height) {
				ogmCin.outputWidht = ogmCin.th_info.width;
				ogmCin.outputHeight = ogmCin.th_info.height;
				Com_DPrintf(DEBUG_CLIENT, "[Theora(ogg)]new resolution %dx%d\n", ogmCin.outputWidht, ogmCin.outputHeight);
			}

			if (ogmCin.outputBufferSize < ogmCin.th_info.width * ogmCin.th_info.height) {
				ogmCin.outputBufferSize = ogmCin.th_info.width * ogmCin.th_info.height;

				/* Free old output buffer*/
				if (ogmCin.outputBuffer)
					Mem_Free(ogmCin.outputBuffer);

				/* Allocate the new buffer */
				ogmCin.outputBuffer = (byte*) Mem_PoolAlloc(ogmCin.outputBufferSize * OGM_CINEMATIC_BPP, cl_genericPool, 0);
				if (ogmCin.outputBuffer == NULL) {
					ogmCin.outputBufferSize = 0;
					r = -2;
					break;
				}
			}

			yWShift = CIN_THEORA_FindSizeShift(ogmCin.th_yuvbuffer.y_width, ogmCin.th_info.width);
			uvWShift = CIN_THEORA_FindSizeShift(ogmCin.th_yuvbuffer.uv_width, ogmCin.th_info.width);
			yHShift = CIN_THEORA_FindSizeShift(ogmCin.th_yuvbuffer.y_height, ogmCin.th_info.height);
			uvHShift = CIN_THEORA_FindSizeShift(ogmCin.th_yuvbuffer.uv_height, ogmCin.th_info.height);

			if (yWShift < 0 || uvWShift < 0 || yHShift < 0 || uvHShift < 0) {
				Com_Printf("[Theora] unexpected resolution in a yuv-Frame\n");
				r = -1;
			} else {
				CIN_THEORA_FrameYUVtoRGB24(ogmCin.th_yuvbuffer.y, ogmCin.th_yuvbuffer.u, ogmCin.th_yuvbuffer.v,
						ogmCin.th_info.width, ogmCin.th_info.height, ogmCin.th_yuvbuffer.y_stride,
						ogmCin.th_yuvbuffer.uv_stride, yWShift, uvWShift, yHShift, uvHShift,
						(unsigned int*) ogmCin.outputBuffer);

				r = 1;
				ogmCin.videoFrameCount = th_frame;
			}
		}

	}

	return r;
}
#endif

/**
 * @return 1 -> loaded a new Frame (ogmCin.outputBuffer points to the actual frame), 0 -> no new Frame
 * <0 -> error
 */
static int CIN_OGM_LoadVideoFrame (void)
{
#ifdef USE_CIN_XVID
	if (ogmCin.videoStreamIsXvid)
		return CIN_XVID_LoadVideoFrame();
#endif
#ifdef USE_CIN_THEORA
	if (ogmCin.videoStreamIsTheora)
		return CIN_THEORA_LoadVideoFrame();
#endif

	/* if we come to this point, there will be no codec that use the stream content ... */
	if (ogmCin.os_video.serialno) {
		ogg_packet op;

		while (ogg_stream_packetout(&ogmCin.os_video, &op))
			;
	}

	return 1;
}

/**
 * @return true => noDataTransfered
 */
static qboolean CIN_OGM_LoadFrame (void)
{
	qboolean anyDataTransferred = qtrue;
	qboolean needVOutputData = qtrue;
	qboolean audioWantsMoreData = qfalse;
	int status;

	while (anyDataTransferred && (needVOutputData || audioWantsMoreData)) {
		anyDataTransferred = qfalse;
			if (needVOutputData && (status = CIN_OGM_LoadVideoFrame())) {
				needVOutputData = qfalse;
				if (status > 0)
					anyDataTransferred = qtrue;
				else
					/* error (we don't need any videodata and we had no transferred) */
					anyDataTransferred = qfalse;
			}

			if (needVOutputData || audioWantsMoreData) {
				/* try to transfer Pages to the audio- and video-Stream */
				if (CIN_OGM_LoadPagesToStream())
					/* try to load a datablock from file */
					anyDataTransferred |= !CIN_OGM_LoadBlockToSync();
				else
					/* successful loadPagesToStreams() */
					anyDataTransferred = qtrue;
			}

			/* load all Audio after loading new pages ... */
			if (ogmCin.videoFrameCount > 1)
				/* wait some videoframes (it's better to have some delay, than a laggy sound) */
				audioWantsMoreData = CIN_OGM_LoadAudioFrame();
	}

	return !anyDataTransferred;
}

/** @brief from VLC ogg.c (http://trac.videolan.org/vlc/browser/trunk/modules/demux/ogg.c) */
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

static qboolean isPowerOf2 (int x)
{
	int bitsSet = 0;
	int i;

	for (i = 0; i < sizeof(int) * 8; ++i)
		if (x & (1 << i))
			++bitsSet;

	return (bitsSet <= 1);
}

/**
 * @return 0 -> no problem
 * @todo vorbis/theora-header & init in sub-functions
 * @todo "clean" error-returns ...
 */
int CIN_OGM_PlayCinematic (const char* filename)
{
	int status;
	ogg_page og;
	ogg_packet op;
	int i;

	if (ogmCin.ogmFile.f || ogmCin.ogmFile.z) {
		Com_Printf("WARNING: it seams there was already a ogm running, it will be killed to start %s\n", filename);
		CIN_OGM_StopCinematic();
	}

	memset(&ogmCin, 0, sizeof(ogmCin));

	if (FS_OpenFile(filename, &ogmCin.ogmFile, FILE_READ) == -1) {
		Com_Printf("Can't open ogm-file for reading (%s)\n", filename);
		return -1;
	}

	cin.cinematicType = CINEMATIC_TYPE_OGM;

	ogg_sync_init(&ogmCin.oy); /* Now we can read pages */

	/** @todo FIXME? can serialno be 0 in ogg? (better way to check initialized?) */
	/** @todo support for more than one audio stream? / detect files with one stream(or without correct ones) */
	while (!ogmCin.os_audio.serialno || !ogmCin.os_video.serialno) {
		if (ogg_sync_pageout(&ogmCin.oy, &og) == 1) {
			if (strstr((char*) (og.body + 1), "vorbis")) { /** @todo FIXME? better way to find audio stream */
				if (ogmCin.os_audio.serialno) {
					Com_Printf("more than one audio stream, in ogm-file(%s) ... we will stay at the first one\n",
							filename);
				} else {
					ogg_stream_init(&ogmCin.os_audio, ogg_page_serialno(&og));
					ogg_stream_pagein(&ogmCin.os_audio, &og);
				}
			}
#ifdef USE_CIN_THEORA
			if (strstr((char*) (og.body + 1), "theora")) {
				if (ogmCin.os_video.serialno) {
					Com_Printf("more than one video stream, in ogm-file(%s) ... we will stay at the first one\n",
							filename);
				} else {
					ogmCin.videoStreamIsTheora = qtrue;
					ogg_stream_init(&ogmCin.os_video, ogg_page_serialno(&og));
					ogg_stream_pagein(&ogmCin.os_video, &og);
				}
			}
#endif
#ifdef USE_CIN_XVID
			if (strstr((char*) (og.body + 1), "video")) { /** @todo better way to find video stream */
				if (ogmCin.os_video.serialno) {
					Com_Printf("more than one video stream, in ogm-file(%s) ... we will stay at the first one\n",
							filename);
				} else {
					stream_header_t* sh;

					ogmCin.videoStreamIsXvid = qtrue;

					sh = (stream_header_t*) (og.body + 1);
					/** @todo one solution for checking xvid and theora */
					if (!isPowerOf2(sh->sh.stream_header_video.width)) {
						Com_Printf("VideoWidth of the ogm-file isn't a power of 2 value (%s)\n", filename);

						return -5;
					}
					if (!isPowerOf2(sh->sh.stream_header_video.height)) {
						Com_Printf("VideoHeight of the ogm-file isn't a power of 2 value (%s)\n", filename);

						return -6;
					}

					ogmCin.Vtime_unit = sh->time_unit;

					ogg_stream_init(&ogmCin.os_video, ogg_page_serialno(&og));
					ogg_stream_pagein(&ogmCin.os_video, &og);
				}
			}
#endif
		} else if (CIN_OGM_LoadBlockToSync())
			break;
	}

	if (ogmCin.videoStreamIsXvid && ogmCin.videoStreamIsTheora) {
		Com_Printf("Found \"video\"- and \"theora\"-stream, ogm-file (%s)\n", filename);
		return -2;
	}

	if (!ogmCin.os_audio.serialno) {
		Com_Printf("Haven't found a audio (vorbis) stream in ogm-file (%s)\n", filename);
		return -2;
	}
	if (!ogmCin.os_video.serialno) {
		Com_Printf("Haven't found a video stream in ogm-file (%s)\n", filename);
		return -3;
	}

	/* load vorbis header */
	vorbis_info_init(&ogmCin.vi);
	vorbis_comment_init(&ogmCin.vc);
	i = 0;
	while (i < 3) {
		status = ogg_stream_packetout(&ogmCin.os_audio, &op);
		if (status < 0) {
			Com_Printf("Corrupt ogg packet while loading vorbis-headers, ogm-file(%s)\n", filename);
			return -8;
		}
		if (status > 0) {
			status = vorbis_synthesis_headerin(&ogmCin.vi, &ogmCin.vc, &op);
			if (i == 0 && status < 0) {
				Com_Printf("This Ogg bitstream does not contain Vorbis audio data, ogm-file(%s)\n", filename);
				return -9;
			}
			++i;
		} else if (CIN_OGM_LoadPagesToStream()) {
			if (CIN_OGM_LoadBlockToSync()) {
				Com_Printf("Couldn't find all vorbis headers before end of ogm-file (%s)\n", filename);
				return -10;
			}
		}
	}

	vorbis_synthesis_init(&ogmCin.vd, &ogmCin.vi);

#ifdef USE_CIN_XVID
	status = CIN_XVID_Init();
	if (status) {
		Com_Printf("[Xvid]Decore INIT problem, return value %d(ogm-file: %s)\n", status, filename);
		return -4;
	}
#endif

#ifdef USE_CIN_THEORA
	if (ogmCin.videoStreamIsTheora) {
		theora_info_init(&ogmCin.th_info);
		theora_comment_init(&ogmCin.th_comment);

		i = 0;
		while (i < 3) {
			status = ogg_stream_packetout(&ogmCin.os_video, &op);
			if (status < 0) {
				Com_Printf("Corrupt ogg packet while loading theora-headers, ogm-file(%s)\n", filename);

				return -8;
			}
			if (status > 0) {
				status = theora_decode_header(&ogmCin.th_info, &ogmCin.th_comment, &op);
				if (i == 0 && status != 0) {
					Com_Printf("This Ogg bitstream does not contain theora data, ogm-file(%s)\n", filename);

					return -9;
				}
				++i;
			} else if (CIN_OGM_LoadPagesToStream()) {
				if (CIN_OGM_LoadBlockToSync()) {
					Com_Printf("Couldn't find all theora headers before end of ogm-file (%s)\n", filename);

					return -10;
				}
			}
		}

		theora_decode_init(&ogmCin.th_state, &ogmCin.th_info);
		if (!isPowerOf2(ogmCin.th_info.width)) {
			Com_Printf("VideoWidth of the ogm-file isn't a power of 2 value (%s)\n", filename);

			return -5;
		}
		if (!isPowerOf2(ogmCin.th_info.height)) {
			Com_Printf("VideoHeight of the ogm-file isn't a power of 2 value (%s)\n", filename);

			return -6;
		}
		ogmCin.Vtime_unit = ((ogg_int64_t) ogmCin.th_info.fps_denominator * 1000* 10000 / ogmCin .th_info.fps_numerator);
	}
#endif

	/* Set to play the cinematic in fullscreen mode */
	CIN_SetParameters(0, 0, viddef.virtualWidth, viddef.virtualHeight, CIN_STATUS_FULLSCREEN, qfalse);

	return 0;
}

static int CIN_THEORA_NextNeededFrame (void)
{
	return (int) (ogmCin.currentTime * (ogg_int64_t) 10000 / ogmCin.Vtime_unit);
}

/**
 * @sa R_DrawImagePixelData
 */
static void CIN_OGM_DrawCinematic (void)
{
	int texnum;

	assert(cls.playingCinematic != CIN_STATUS_NONE);

	if (!ogmCin.outputBuffer)
		return;
	texnum = R_DrawImagePixelData("***cinematic***", ogmCin.outputBuffer, ogmCin.outputWidht, ogmCin.outputHeight);
	R_DrawTexture(texnum, cin.x, cin.y, cin.w, cin.h);
}

/**
 * @param[in] time time in ms to which the movie should run
 * @return true if the cinematic is still running, false otherwise
 */
qboolean CIN_OGM_RunCinematic (void)
{
	ogmCin.currentTime = cl.time;

	while (!ogmCin.videoFrameCount || ogmCin.currentTime + 20 >= (int) (ogmCin.videoFrameCount * ogmCin.Vtime_unit / 10000)) {
		if (CIN_OGM_LoadFrame())
			return qfalse;
	}

	CIN_OGM_DrawCinematic();

	return qtrue;
}

void CIN_OGM_StopCinematic (void)
{
#ifdef USE_CIN_XVID
	int status;

	status = CIN_XVID_Shutdown();
	if (status)
		Com_Printf("[Xvid]Decore RELEASE problem, return value %d\n", status);
#endif
#ifdef USE_CIN_THEORA
	theora_clear(&ogmCin.th_state);
	theora_comment_clear(&ogmCin.th_comment);
	theora_info_clear(&ogmCin.th_info);
#endif
	if (ogmCin.outputBuffer)
		Mem_Free(ogmCin.outputBuffer);
	ogmCin.outputBuffer = NULL;

	vorbis_dsp_clear(&ogmCin.vd);
	vorbis_comment_clear(&ogmCin.vc);
	vorbis_info_clear(&ogmCin.vi); /* must be called last (comment from vorbis example code) */

	ogg_stream_clear(&ogmCin.os_audio);
	ogg_stream_clear(&ogmCin.os_video);

	ogg_sync_clear(&ogmCin.oy);

	FS_CloseFile(&ogmCin.ogmFile);
}

void CIN_OGM_Init (void)
{
	memset(&ogmCin, 0, sizeof(ogmCin));
}

#else
int CIN_OGM_PlayCinematic (const char* filename)
{
	return 1;
}

qboolean CIN_OGM_RunCinematic (void)
{
	return qfalse;
}

void CIN_OGM_StopCinematic (void)
{
}

void CIN_OGM_Init (void)
{
}
#endif
