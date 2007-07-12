/**
 * @file snd_ogg.c
 * @brief OGG vorbis sound code
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/snd_dma.c
Copyright (C) 1997-2001 Id Software, Inc.

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

static snd_stream_t* stream = NULL;

cvar_t *snd_music_volume;
cvar_t *snd_music_loop;
/**< fading speed for music - see OGG_Read */
static cvar_t* snd_fadingspeed;

/**< should music fading be activated */
static cvar_t* snd_fadingenable;

/*
==========================================================
OGG Vorbis decoding
==========================================================
*/

/**
 * @brief fread() replacement
 */
static size_t S_OGG_Callback_read (void *ptr, size_t size, size_t nmemb, void *datasource)
{
	snd_stream_t *stream;
	int byteSize = 0;
	int bytesRead = 0;
	size_t nMembRead = 0;

	/* check if input is valid */
	if (!ptr) {
		errno = EFAULT;
		return 0;
	}

	if (!(size && nmemb)) {
		/* It's not an error, caller just wants zero bytes! */
		errno = 0;
		return 0;
	}

	if (!datasource) {
		errno = EBADF;
		return 0;
	}

	/* we use a snd_stream_t in the generic pointer to pass around */
	stream = (snd_stream_t *) datasource;

	/* FS_Read does not support multi-byte elements */
	byteSize = nmemb * size;

	/* read it with the UFO function FS_Read() */
	bytesRead = FS_Read(ptr, byteSize, &stream->file);

	/* update the file position */
	stream->pos += bytesRead;

	/* this function returns the number of elements read not the number of bytes */
	nMembRead = bytesRead / size;

	/* even if the last member is only read partially */
	/* it is counted as a whole in the return value */
	if (bytesRead % size)
		nMembRead++;

	return nMembRead;
}

/**
 * @brief fseek() replacement
 */
static int S_OGG_Callback_seek (void *datasource, ogg_int64_t offset, int whence)
{
	snd_stream_t *stream;
	int retVal = 0;

	/* check if input is valid */
	if (!datasource) {
		errno = EBADF;
		return -1;
	}

	/* snd_stream_t in the generic pointer */
	stream = (snd_stream_t *) datasource;

	/* we must map the whence to its UFO counterpart */
	switch (whence) {
		case SEEK_SET :
		{
			/* set the file position in the actual file with the Q3 function */
			retVal = FS_Seek(&stream->file, (long) offset, FS_SEEK_SET);

			/* something has gone wrong, so we return here */
			if (retVal < 0)
				return retVal;

			/* keep track of file position */
			stream->pos = (int) offset;
			break;
		}

		case SEEK_CUR :
		{
			/* set the file position in the actual file with the Q3 function */
			retVal = FS_Seek(&stream->file, (long) offset, FS_SEEK_CUR);

			/* something has gone wrong, so we return here */
			if (retVal < 0)
				return retVal;

			/* keep track of file position */
			stream->pos += (int) offset;
			break;
		}

		case SEEK_END :
		{
			/* UFO seems to have trouble with FS_SEEK_END */
			/* so we use the file length and FS_SEEK_SET */

			/* set the file position in the actual file with the Q3 function */
			retVal = FS_Seek(&stream->file, (long)stream->length + (long) offset, FS_SEEK_SET);

			/* something has gone wrong, so we return here */
			if (retVal < 0)
				return retVal;

			/* keep track of file position */
			stream->pos = stream->length + (int) offset;
			break;
		}

		default :
		{
			/* unknown whence, so we return an error */
			errno = EINVAL;
			return -1;
		}
	}

	/* stream->pos shouldn't be smaller than zero or bigger than the filesize */
	stream->pos = (stream->pos < 0) ? 0 : stream->pos;
	stream->pos = (stream->pos > stream->length) ? stream->length : stream->pos;

	return 0;
}

/**
 * @brief fclose() replacement
 */
static int S_OGG_Callback_close (void *datasource)
{
	/* we do nothing here and close all things manually in S_OGG_CodecCloseStream() */
	return 0;
}

/**
 * @brief ftell() replacement
 */
static long S_OGG_Callback_tell (void *datasource)
{
	/* check if input is valid */
	if (!datasource) {
		errno = EBADF;
		return -1;
	}

	/* we keep track of the file position in stream->pos */
	return (long)(((snd_stream_t *)datasource)->pos);
}

/* the callback structure */
static const ov_callbacks S_OGG_Callbacks = {
	&S_OGG_Callback_read,
	&S_OGG_Callback_seek,
	&S_OGG_Callback_close,
	&S_OGG_Callback_tell
};


/**
 * @brief Opens the given ogg file
 * @sa S_OGG_Stop
 */
qboolean S_OGG_Open (const char *filename)
{
	int length;
	vorbis_info *vi;
	char *checkFilename = NULL;

	assert(filename);

	if (snd_music_volume->value <= 0)
		return qfalse;

	if (!stream) {
		/* Allocate a stream */
		stream = Mem_Alloc(sizeof(snd_stream_t));
		if (!stream)
			return qfalse;
	}

	/* strip extension */
	checkFilename = strstr(filename, ".ogg");
	if (checkFilename)
		*checkFilename = '\0';

	/* FIXME */
	music.rate = 44100;
#ifdef HAVE_OPENAL
	music.format = AL_FORMAT_STEREO16;
#endif
	/* check running music */
	if (music.ovPlaying[0]) {
		if (!Q_strcmp(music.ovPlaying, filename)) {
			Com_DPrintf("S_OGG_Open: Already playing %s\n", filename);
			return qtrue;
		} else {
			if (snd_fadingenable->value) {
				Q_strncpyz(music.newFilename, filename, sizeof(music.newFilename));
				return qtrue;
			} else
				S_OGG_Stop();
		}
	}

	music.fading = snd_music_volume->value;

	/* find file */
	length = FS_FOpenFile(va("music/%s.ogg", filename), &stream->file);
	if (!stream->file.f && !stream->file.z) {
		Com_Printf("Couldn't open 'music/%s.ogg'\n", filename);
		return qfalse;
	}

	stream->length = length;

	/* open ogg vorbis file */
	if (ov_open_callbacks(stream, &music.ovFile, NULL, 0, S_OGG_Callbacks) != 0) {
		Com_Printf("'music/%s.ogg' isn't a valid ogg vorbis file (error %i)\n", filename, errno);
		FS_FCloseFile(&stream->file);
		return qfalse;
	}

	vi = ov_info(&music.ovFile, -1);
	if ((vi->channels != 1) && (vi->channels != 2)) {
		Com_Printf("%s has an unsupported number of channels: %i\n", filename, vi->channels);
		FS_FCloseFile(&stream->file);
		return qfalse;
	}

/*	Com_Printf("Playing '%s'\n", filename); */
	Q_strncpyz(music.ovPlaying, filename, sizeof(music.ovPlaying));
	music.ovSection = 0;
	return qtrue;
}

/**
 * @brief Clears the music.ovPlaying string and stop the currently playing ogg file (music.ovFile)
 * @sa S_OGG_Open
 * @sa S_OGG_Start
 * @sa S_OGG_Play
 */
void S_OGG_Stop (void)
{
	if (!stream)
		return;

	music.ovPlaying[0] = 0;
	music.fading = snd_music_volume->value;
	ov_clear(&music.ovFile);

	FS_FCloseFile(&stream->file);
}

/**
 * @brief
 * @sa OGG_Stop
 * @sa S_RawSamples
 */
int S_OGG_Read (void)
{
	int res;

	if (!stream)
		Sys_Error("No stream allocated\n");

	/* music faded out - so stop is and open the new track */
	if (music.fading <= 0.0) {
		S_OGG_Stop();
		S_OGG_Open(music.newFilename);
		music.newFilename[0] = '\0';
		return 0;
	}

	/* read and resample */
	/* this read function will use our callbacks to be even able to read from zip files */
	res = ov_read(&music.ovFile, music.ovBuf, sizeof(music.ovBuf), 0, 2, 1, &music.ovSection);
	if (res == OV_HOLE) {
		Com_Printf("S_OGG_Read: interruption in the data (one of: garbage between pages, loss of sync followed by recapture, or a corrupt page)\n");
		res = 0;
	} else if (res == OV_EBADLINK) {
		Com_Printf("S_OGG_Read: invalid stream section was supplied to libvorbisfile, or the requested link is corrupt\n");
		res = 0;
	}
	S_RawSamples(res >> 2, music.rate, 2, 2, (byte *) music.ovBuf, music.fading);
	if (*music.newFilename)
		music.fading -= snd_fadingspeed->value;

#ifdef HAVE_OPENAL
	SND_OAL_Stream(music.ovPlaying);
#endif

	/* end of file? */
	if (!res) {
		if (snd_music_loop->integer)
			ov_raw_seek(&music.ovFile, 0);
		else
			S_OGG_Stop();
	}
	return res;
}

/**
 * @brief Script command that plays the music track given via parameter
 * @sa S_OGG_Open
 * @sa S_OGG_Start
 * @sa S_OGG_Stop
 */
void S_OGG_Play (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: music_play <filename>\n");
		return;
	}
	S_OGG_Open(Cmd_Argv(1));
}

/**
 * @brief Script command that plays the music track stored in cvar music
 * @sa S_OGG_Open
 * @sa S_OGG_Play
 * @sa S_OGG_Stop
 */
void S_OGG_Start (void)
{
	const char *str = Cvar_VariableString("music");
	if (str[0])
		S_OGG_Open(str);
}

/**
 * @brief
 * @sa S_SoundInfo_f
 */
void S_OGG_Info (void)
{
	vorbis_info *vi;
	int i;

	if (music.ovPlaying[0]) {
		Com_Printf("\nogg infos:\n");
		Com_Printf("...currently playing: %s\n", music.ovPlaying);
		for (i = 0; i < ov_streams(&music.ovFile); i++) {
			vi = ov_info(&music.ovFile,i);
			Com_Printf("...logical bitstream section %d information:\n", i + 1);
			Com_Printf("......%ldHz %d channels bitrate %ldkbps serial number=%ld\n",
				vi->rate, vi->channels, ov_bitrate(&music.ovFile,i)/1000, ov_serialnumber(&music.ovFile, i));
			Com_Printf("......compressed length: %ld bytes\n", (long)(ov_raw_total(&music.ovFile, i)));
			Com_Printf("...play time: %.0f s\n", ov_time_total(&music.ovFile, i));
			Com_Printf("...compressed length: %ld bytes\n", (long)(ov_raw_total(&music.ovFile, i)));
		}
	}
}


static int musicTrackCount = 0;

/**
 * @brief Sets the music cvar to a random track
 * @note You have to start the music track afterwards
 */
void S_OGG_RandomTrack_f (void)
{
	char findname[MAX_OSPATH];
	int i, randomID;
	char **dirnames;
	const char *musicTrack;
	int ndirs;
	searchpath_t *search;
	pack_t *pak;
	int count = 0;

	if (musicTrackCount == 0) {
		/* search through the path, one element at a time */
		for (search = fs_searchpaths; search; search = search->next) {
			/* is the element a pak file? */
			if (search->pack) {
				/* look through all the pak file elements */
				pak = search->pack;
				for (i = 0; i < pak->numfiles; i++)
					if (strstr(pak->files[i].name, ".ogg"))
						musicTrackCount++;
			} else {
				Com_sprintf(findname, sizeof(findname), "%s/music/*.ogg", search->filename);
				FS_NormPath(findname);

				if ((dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_HIDDEN | SFF_SYSTEM)) != 0) {
					for (i = 0; i < ndirs - 1; i++) {
						musicTrackCount++;
						Mem_Free(dirnames[i]);
					}
					Mem_Free(dirnames);
				}
			}
		}
	}

	randomID = rand() & musicTrackCount;
	Com_DPrintf("S_OGG_RandomTrack_f: random track id: %i/%i\n", randomID, musicTrackCount);

	/* search through the path, one element at a time */
	for (search = fs_searchpaths; search; search = search->next) {
		/* is the element a pak file? */
		if (search->pack) {
			/* look through all the pak file elements */
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				if (strstr(pak->files[i].name, ".ogg")) {
					count++;
					if (randomID == count) {
						Com_sprintf(findname, sizeof(findname), "%s", pak->files[i].name);
						musicTrack = COM_SkipPath(findname);
						Com_Printf("..playing next: '%s'\n", musicTrack);
						Cvar_Set("music", musicTrack);
					}
				}
		} else {
			Com_sprintf(findname, sizeof(findname), "%s/music/*.ogg", search->filename);
			FS_NormPath(findname);

			if ((dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_HIDDEN | SFF_SYSTEM)) != 0) {
				for (i = 0; i < ndirs - 1; i++) {
					count++;
					if (randomID == count) {
						musicTrack = COM_SkipPath(dirnames[i]);
						Com_Printf("..playing next: '%s'\n", musicTrack);
						Cvar_Set("music", musicTrack);
					}
					Mem_Free(dirnames[i]);
				}
				Mem_Free(dirnames);
			}
		}
	}
}

/**
 * @brief List all available music tracks
 */
void S_OGG_List_f (void)
{
	char findname[MAX_OSPATH];
	int i;
	char **dirnames;
	int ndirs;
	searchpath_t *search;
	pack_t *pak;

	Com_Printf("music tracks\n");
	/* search through the path, one element at a time */
	for (search = fs_searchpaths; search; search = search->next) {
		Com_Printf("..searchpath: %s\n", search->filename);
		/* is the element a pak file? */
		if (search->pack) {
			/* look through all the pak file elements */
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				if (strstr(pak->files[i].name, ".ogg"))
					Com_Printf("...%s\n", pak->files[i].name);
		} else {
			Com_sprintf(findname, sizeof(findname), "%s/music/*.ogg", search->filename);
			FS_NormPath(findname);

			if ((dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_HIDDEN | SFF_SYSTEM)) != 0) {
				for (i = 0; i < ndirs - 1; i++) {
					Com_Printf("...%s\n", dirnames[i]);
					Mem_Free(dirnames[i]);
				}
				Mem_Free(dirnames);
			}
		}
	}
}

/**
 * @brief
 */
sfxcache_t *S_OGG_LoadSFX (sfx_t *s)
{
	OggVorbis_File vorbisFile;
	vorbis_info *vi;
	qFILE file;
	sfxcache_t *sc;
	char *buffer;
	int bitstream, bytes_read, bytes_read_total, len, samples;

	Com_Printf("loading %s.ogg\n", s->name);

	len = FS_FOpenFile(va("sound/%s.ogg", s->name), &file);
	if (!file.f && !file.z)
		return NULL;

	if (!s->stream)
		s->stream = Mem_PoolAlloc(sizeof(snd_stream_t), cl_soundSysPool, 0);

	if (!s->stream) {
		FS_FCloseFile(&file);
		return NULL;
	}

	s->stream->file = file;
	s->stream->length = len;

	if (ov_open_callbacks(s->stream, &vorbisFile, NULL, 0, S_OGG_Callbacks) != 0) {
		Com_Printf("Error getting OGG callbacks: %s\n", s->name);
		FS_FCloseFile(&file);
		Mem_Free(s->stream);
		return NULL;
	}

	if (!ov_seekable(&vorbisFile)) {
		Com_Printf("Error unsupported .ogg file (not seekable): %s\n", s->name);
		ov_clear(&vorbisFile); /* Does FS_FCloseFile */
		Mem_Free(s->stream);
		return NULL;
	}

	if (ov_streams(&vorbisFile) != 1 ) {
		Com_Printf("Error unsupported .ogg file (multiple logical bitstreams): %s\n", s->name);
		ov_clear(&vorbisFile); /* Does FS_FCloseFile */
		Mem_Free(s->stream);
		return NULL;
	}

	vi = ov_info(&vorbisFile, -1);
	if (vi->channels != 1 && vi->channels != 2) {
		Com_Printf("Error unsupported .ogg file (unsupported number of channels: %i): %s\n", vi->channels, s->name);
		ov_clear(&vorbisFile); /* Does FS_FCloseFile */
		Mem_Free(s->stream);
		return NULL;
	}

	samples = (int)ov_pcm_total(&vorbisFile, -1);
	len = (int) ((double) samples * (double) dma.speed / (double) vi->rate);
	len = len * 2 * vi->channels;

	sc = s->cache = Mem_PoolAlloc(len + sizeof(sfxcache_t), cl_soundSysPool, 0);
	sc->length = samples;
	sc->loopstart = -1;
	sc->speed = vi->rate;
	sc->width = 2;

	if (dma.speed != vi->rate) {
		len = samples * 2 * vi->channels;
		buffer = Mem_PoolAlloc(len, cl_soundSysPool, 0);
	} else {
		buffer = (char *)sc->data;
	}

	bytes_read_total = 0;
	do {
		if (Q_IsBigEndian())
			bytes_read = ov_read(&vorbisFile, buffer + bytes_read_total, len - bytes_read_total, 1, 2, 1, &bitstream);
		else
			bytes_read = ov_read(&vorbisFile, buffer + bytes_read_total, len - bytes_read_total, 0, 2, 1, &bitstream);
		bytes_read_total += bytes_read;
	} while (bytes_read > 0 && bytes_read_total < len);
	ov_clear(&vorbisFile); /* Does FS_FCloseFile */

	if (bytes_read_total != len) {
		Com_Printf("Error reading .ogg file: %s\n", s->name);
		if ((void *)buffer != sc->data)
			Mem_Free(buffer);
		Mem_Free(sc);
		s->cache = NULL;
		return NULL;
	}

	if (dma.speed != vi->rate) {
		S_ResampleSfx(s, sc->speed, sc->width, (byte*)buffer);
		Mem_Free(buffer);
	}

	return sc;
}

/**
 * @brief Free the stream
 * @sa S_OGG_Init
 */
void S_OGG_Shutdown (void)
{
	if (stream) {
		Mem_Free(stream);
		stream = NULL;
	}
}

/**
 * @brief Register cvars for OGG music support
 * @sa S_OGG_Shutdown
 */
void S_OGG_Init (void)
{
	snd_music_volume = Cvar_Get("snd_music_volume", "0.5", CVAR_ARCHIVE, "Music volume");
	snd_music_loop = Cvar_Get("snd_music_loop", "1", 0, "Music loop");
	snd_fadingspeed = Cvar_Get("snd_fadingspeed", "0.01", 0, "Music fading speed");
	snd_fadingenable = Cvar_Get("snd_fadingenable", "1", 0, "Music fading enabled");
}
