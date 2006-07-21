/**
 * @file common.c
 * @brief Misc functions used in client and server
 */

/*
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

#include "common.h"

#include "qcommon.h"
#include <setjmp.h>
#include <ctype.h>
#include <limits.h>

#if defined DEBUG && defined _MSC_VER
#include <intrin.h>
#endif

#define	MAXPRINTMSG	4096
#define MAX_NUM_ARGVS	50


csi_t csi;

int com_argc;
char *com_argv[MAX_NUM_ARGVS + 1];

int realtime;

jmp_buf abortframe;				/* an ERR_DROP occured, exit the entire frame */


FILE *log_stats_file;

cvar_t *s_language;
cvar_t *host_speeds;
cvar_t *log_stats;
cvar_t *developer;
cvar_t *timescale;
cvar_t *fixedtime;
cvar_t *logfile_active;			/* 1 = buffer log, 2 = flush after each print */
cvar_t *showtrace;
cvar_t *dedicated;

static FILE *logfile;

static int server_state;

/* host_speeds times */
int time_before_game;
int time_after_game;
int time_before_ref;
int time_after_ref;

/*
============================================================================

CLIENT / SERVER interactions

============================================================================
*/

static int rd_target;
static char *rd_buffer;
static int rd_buffersize;
static void (*rd_flush) (int target, char *buffer);

/**
 * @brief Redirect pakets/ouput from server to client
 * @sa Com_EndRedirect
 *
 * This is used to redirect printf outputs for rcon commands
 */
void Com_BeginRedirect(int target, char *buffer, int buffersize, void (*flush) (int, char *))
{
	if (!target || !buffer || !buffersize || !flush)
		return;
	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

/**
 * @brief End the redirection of pakets/output
 * @sa Com_BeginRedirect
 */
void Com_EndRedirect(void)
{
	rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

/**
 * @brief
 *
 * Both client and server can use this, and it will output
 * to the apropriate place.
 */
void Com_Printf(char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
#ifndef _WIN32
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
#else
	vsprintf(msg, fmt, argptr);
#endif
	va_end(argptr);

	if (rd_target) {
		if ((strlen(msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_target, rd_buffer);
			*rd_buffer = 0;
		}
		Q_strcat(rd_buffer, msg, sizeof(rd_buffer));
		return;
	}

	Con_Print(msg);

	/* also echo to debugging console */
	Sys_ConsoleOutput(msg);

	/* logfile */
	if (logfile_active && logfile_active->value) {
		char name[MAX_QPATH];

		if (!logfile) {
			Com_sprintf(name, sizeof(name), "%s/ufoconsole.log", FS_Gamedir());
			if (logfile_active->value > 2)
				logfile = fopen(name, "a");
			else
				logfile = fopen(name, "w");
		}
		if (logfile)
			fprintf(logfile, "%s", msg);
		if (logfile_active->value > 1)
			fflush(logfile);	/* force it to save every time */
	}
}


/**
 * @brief
 *
 * A Com_Printf that only shows up if the "developer" cvar is set
 */
void Com_DPrintf(char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	/* don't confuse non-developers with techie stuff... */
	if (!developer || !developer->value)
		return;

	va_start(argptr, fmt);
#ifndef _WIN32
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
#else
	vsprintf(msg, fmt, argptr);
#endif
	va_end(argptr);

	Com_Printf("%s", msg);
}


/**
 * @brief
 *
 * Both client and server can use this, and it will
 * do the apropriate things.
 */
void Com_Error(int code, char *fmt, ...)
{
	va_list argptr;
	static char msg[MAXPRINTMSG];
	static qboolean recursive = qfalse;

#if defined DEBUG && defined _MSC_VER
	__debugbreak();				/* break execution before game shutdown */
#endif

	if (recursive)
		Sys_Error("recursive error after: %s", msg);
	recursive = qtrue;

	va_start(argptr, fmt);
#ifndef _WIN32
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
#else
	vsprintf(msg, fmt, argptr);
#endif
	va_end(argptr);

	if (code == ERR_DISCONNECT) {
		CL_Drop();
		recursive = qfalse;
		longjmp(abortframe, -1);
	} else if (code == ERR_DROP) {
		Com_Printf("********************\nERROR: %s\n********************\n", msg);
		SV_Shutdown(va("Server crashed: %s\n", msg), qfalse);
		CL_Drop();
		recursive = qfalse;
		longjmp(abortframe, -1);
	} else {
		SV_Shutdown(va("Server fatal crashed: %s\n", msg), qfalse);
		CL_Shutdown();
	}

	if (logfile) {
		fclose(logfile);
		logfile = NULL;
	}

	Sys_Error("%s", msg);
}


/**
 * @brief
 */
void Com_Drop(void)
{
	SV_Shutdown("Server disconnected\n", qfalse);
	CL_Drop();
	longjmp(abortframe, -1);
}


/**
 * @brief
 *
 * Both client and server can use this, and it will
 * do the apropriate things.
 */
void Com_Quit(void)
{
	SV_Shutdown("Server quit\n", qfalse);
#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif
	if (logfile) {
		fclose(logfile);
		logfile = NULL;
	}

	Sys_Quit();
}


/**
 * @brief
 */
int Com_ServerState(void)
{
	return server_state;
}

/**
 * @brief
 */
void Com_SetServerState(int state)
{
	server_state = state;
}


/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

vec3_t bytedirs[NUMVERTEXNORMALS] = {
#include "../client/anorms.h"
};

/* writing functions */

/**
 * @brief Write a character into a buffer.
 *
 * NOTE: This used to take an int as a parameter, which just doesn't make sense.
 */
void MSG_WriteChar(sizebuf_t *sb, char c)
{
	char *buf;
	
	buf = SZ_GetSpace(sb, 1);
	*buf = c;
}

/**
 * @brief
 */
void MSG_WriteByte(sizebuf_t *sb, uint8_t c)
{
	uint8_t *buf;

	buf = SZ_GetSpace(sb, 1);
	*buf = c;
}

/**
 * @brief
 */
void MSG_WriteShort(sizebuf_t *sb, int16_t c)
{
	int16_t *buf;

	buf = SZ_GetSpace(sb, 2);
	*buf = c;
}

/**
 * @brief
 */
void MSG_WriteLong(sizebuf_t *sb, int32_t c)
{
	int32_t *buf;

	buf = SZ_GetSpace(sb, 4);
	*buf = c;
}

/**
 * @brief
 */
void MSG_WriteFloat(sizebuf_t *sb, float32_t f)
{
	float32_t *buf;

	buf = SZ_GetSpace(sb, 4);
	*buf = f;
}

/**
 * @brief
 */
void MSG_WriteString(sizebuf_t *sb, char *s)
{
	if (!s) {
		SZ_Write(sb, "", 1);
	} else {
		SZ_Write(sb, s, strlen(s) + 1);
	}
}

/**
 * @brief
 */
void MSG_WriteCoord(sizebuf_t *sb, float f)
{
	MSG_WriteLong(sb, (int) (f * 32));
}

/**
 * @brief
 */
void MSG_WritePos(sizebuf_t *sb, vec3_t pos)
{
	MSG_WriteShort(sb, (int) (pos[0] * 8));
	MSG_WriteShort(sb, (int) (pos[1] * 8));
	MSG_WriteShort(sb, (int) (pos[2] * 8));
}

/**
  * @brief
  */
void MSG_WriteGPos(sizebuf_t *sb, pos3_t pos)
{
	MSG_WriteByte(sb, pos[0]);
	MSG_WriteByte(sb, pos[1]);
	MSG_WriteByte(sb, pos[2]);
}

/**
 * @brief
 */
void MSG_WriteAngle(sizebuf_t *sb, float f)
{
	MSG_WriteByte(sb, (int) (f * 256 / 360) & 255);
}

/**
 * @brief
 */
void MSG_WriteAngle16(sizebuf_t *sb, float f)
{
	MSG_WriteShort(sb, ANGLE2SHORT(f));
}


/**
 * @brief
 */
void MSG_WriteDir(sizebuf_t *sb, vec3_t dir)
{
	int i, best;
	float d, bestd;

	if (!dir) {
		MSG_WriteByte(sb, 0);
		return;
	}

	bestd = 0;
	best = 0;
	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		d = DotProduct(dir, bytedirs[i]);
		if (d > bestd) {
			bestd = d;
			best = i;
		}
	}
	MSG_WriteByte(sb, best);
}


/**
 * @brief
 */
void MSG_WriteFormat(sizebuf_t *sb, char *format, ...)
{
	va_list ap;
	char typeID;

	/* initialize ap */
	va_start(ap, format);

	while (*format) {
		typeID = *format++;

		switch (typeID) {
		case 'c':
			MSG_WriteChar(sb, va_arg(ap, int));

			break;
		case 'b':
			MSG_WriteByte(sb, va_arg(ap, int));

			break;
		case 's':
			MSG_WriteShort(sb, va_arg(ap, int));

			break;
		case 'l':
			MSG_WriteLong(sb, va_arg(ap, int));

			break;
		case 'f':
			/* NOTE: float is promoted to double through ... */
			MSG_WriteFloat(sb, va_arg(ap, double));

			break;
		case 'p':
			MSG_WritePos(sb, va_arg(ap, float *));

			break;
		case 'g':
			MSG_WriteGPos(sb, va_arg(ap, byte *));
			break;
		case 'd':
			MSG_WriteDir(sb, va_arg(ap, float *));

			break;
		case 'a':
			/* NOTE: float is promoted to double through ... */
			MSG_WriteAngle(sb, va_arg(ap, double));

			break;
		case '!':
			break;
		case '*':
			{
				int i, n;
				byte *p;

				n = va_arg(ap, int);

				p = va_arg(ap, byte *);
				MSG_WriteByte(sb, n);
				for (i = 0; i < n; i++)
					MSG_WriteByte(sb, *p++);
			}
			break;
		default:
			Com_Error(ERR_DROP, "WriteFormat: Unknown type!\n");
		}
	}

	va_end(ap);
}


/*============================================================ */

/* reading functions */

/**
 * @brief
 */
void MSG_BeginReading(sizebuf_t *msg)
{
	msg->readcount = 0;
}

/**
 * @brief
 *
 * returns -1 if no more characters are available
 */
char MSG_ReadChar(sizebuf_t *msg_read)
{
	char c;

	if (msg_read->readcount + 1 > msg_read->cursize)
		c = -1;
	else
		c = ((char *)msg_read->data)[msg_read->readcount];
	msg_read->readcount++;

	return c;
}

/**
 * @brief
 */
uint8_t MSG_ReadByte(sizebuf_t *msg_read)
{
	uint8_t c;

	if (msg_read->readcount + 1 > msg_read->cursize) {
		c = -1;
	} else {
		c = ((uint8_t *)msg_read->data)[msg_read->readcount];
	}
	msg_read->readcount++;

	return c;
}

/**
 * @brief
 */
uint16_t MSG_ReadShort(sizebuf_t *msg_read)
{
	int16_t c;

	if (msg_read->readcount + 2 > msg_read->cursize) {
		c = -1;
	} else {
		c = ((int16_t *)msg_read->data)[msg_read->readcount];
	}

	msg_read->readcount += 2;

	return c;
}

/**
 * @brief
 */
int32_t MSG_ReadLong(sizebuf_t *msg_read)
{
	int32_t c;

	if (msg_read->readcount + 4 > msg_read->cursize) {
		c = -1;
	} else {
		c =  ((int32_t *)msg_read->data)[msg_read->readcount];
	}
	msg_read->readcount += 4;

	return c;
}

/**
 * @brief
 */
float MSG_ReadFloat(sizebuf_t *msg_read)
{
	float32_t f;
	
	if (msg_read->readcount + 4 > msg_read->cursize) {
		f = -1;
	} else {
		f = ((float32_t *)msg_read->data)[msg_read->readcount];
	}
	
	msg_read->readcount += 4;

	return f;
}

/**
 * @brief
 */
char *MSG_ReadString(sizebuf_t *msg_read)
{
	static char string[2048];
	int l, c;

	l = 0;
	do {
		c = MSG_ReadByte(msg_read);
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

/**
 * @brief
 */
char *MSG_ReadStringLine(sizebuf_t *msg_read)
{
	static char string[2048];
	int l, c;

	l = 0;
	do {
		c = MSG_ReadByte(msg_read);
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

/**
 * @brief
 */
float MSG_ReadCoord(sizebuf_t *msg_read)
{
	return (float) MSG_ReadLong(msg_read) * (1.0 / 32);
}

/**
 * @brief
 */
void MSG_ReadPos(sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadShort(msg_read) * (1.0 / 8);
	pos[1] = MSG_ReadShort(msg_read) * (1.0 / 8);
	pos[2] = MSG_ReadShort(msg_read) * (1.0 / 8);
}

/**
 * @brief
 */
void MSG_ReadGPos(sizebuf_t *msg_read, pos3_t pos)
{
	pos[0] = MSG_ReadByte(msg_read);
	pos[1] = MSG_ReadByte(msg_read);
	pos[2] = MSG_ReadByte(msg_read);
}

/**
 * @brief
 */
float MSG_ReadAngle(sizebuf_t *msg_read)
{
	return (float) MSG_ReadChar(msg_read) * (360.0 / 256);
}

/**
 * @brief
 */
float MSG_ReadAngle16(sizebuf_t *msg_read)
{
	return (float) SHORT2ANGLE(MSG_ReadShort(msg_read));
}

/**
 * @brief
 */
void MSG_ReadData(sizebuf_t *msg_read, void *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
		((byte *) data)[i] = MSG_ReadByte(msg_read);
}

/**
 * @brief
 */
void MSG_ReadDir(sizebuf_t *sb, vec3_t dir)
{
	int b;

	b = MSG_ReadByte(sb);
	if (b >= NUMVERTEXNORMALS)
		Com_Error(ERR_DROP, "MSG_ReadDir: out of range");
	VectorCopy(bytedirs[b], dir);
}


/**
 * @brief
 */
void MSG_ReadFormat(sizebuf_t *msg_read, char *format, ...)
{
	va_list ap;
	char typeID;

	/* initialize ap */
	va_start(ap, format);

	while (*format) {
		typeID = *format++;

		switch (typeID) {
		case 'c':
			*va_arg(ap, int *) = MSG_ReadChar(msg_read);

			break;
		case 'b':
			*va_arg(ap, int *) = MSG_ReadByte(msg_read);

			break;
		case 's':
			*va_arg(ap, int *) = MSG_ReadShort(msg_read);

			break;
		case 'l':
			*va_arg(ap, int *) = MSG_ReadLong(msg_read);

			break;
		case 'f':
			*va_arg(ap, float *) = MSG_ReadFloat(msg_read);

			break;
		case 'p':
			MSG_ReadPos(msg_read, *va_arg(ap, vec3_t *));
			break;
		case 'g':
			MSG_ReadGPos(msg_read, *va_arg(ap, pos3_t *));
			break;
		case 'd':
			MSG_ReadDir(msg_read, *va_arg(ap, vec3_t *));
			break;
		case 'a':
			*va_arg(ap, float *) = MSG_ReadAngle(msg_read);

			break;
		case '!':
			format++;
			break;
		case '*':
			{
				int i, n;
				byte *p;

				n = MSG_ReadByte(msg_read);
				*va_arg(ap, int *) = n;
				p = va_arg(ap, void *);

				for (i = 0; i < n; i++)
					*p++ = MSG_ReadByte(msg_read);
			}
			break;
		default:
			Com_Error(ERR_DROP, "ReadFormat: Unknown type!\n");
		}
	}

	va_end(ap);
}


/**
 * @brief returns the length of a sizebuf_t
 *
 * calculated the length of a sizebuf_t by summing up
 * the size of each format char
 */
int MSG_LengthFormat(sizebuf_t *sb, char *format)
{
	char typeID;
	int length, delta;
	int oldCount;

	length = 0;
	delta = 0;
	oldCount = sb->readcount;

	while (*format) {
		typeID = *format++;

		switch (typeID) {
		case 'c':
			delta = 1;
			break;
		case 'b':
			delta = 1;
			break;
		case 's':
			delta = 2;
			break;
		case 'l':
			delta = 4;
			break;
		case 'f':
			delta = 4;
			break;
		case 'p':
			delta = 6;
			break;
		case 'g':
			delta = 3;
			break;
		case 'd':
			delta = 1;
			break;
		case 'a':
			delta = 1;
			break;
		case '!':
			break;
		case '*':
			delta = MSG_ReadByte(sb);
			length++;
			break;
		case '&':
			delta = 1;
			while (MSG_ReadByte(sb) != NONE)
				length++;
			break;
		default:
			Com_Error(ERR_DROP, "LengthFormat: Unknown type!\n");
		}

		/* advance in buffer */
		sb->readcount += delta;
		length += delta;
	}

	sb->readcount = oldCount;
	return length;
}


/*=========================================================================== */

/**
 * @brief
 */
void SZ_Init(sizebuf_t *buf, void *data, int length)
{
	/* Don't like this - how do you KNOW everything should be initialised to 0?
	 * It's just lazy and unmaintainable.
	memset(buf, 0, sizeof(*buf)); */
	buf->allowoverflow = false;
	buf->overflowed = false;
	buf->data = data;
	buf->maxsize = length;
	buf->cursize = 0;
	buf->readcount = 0;
}

/**
 * @brief
 */
void SZ_Clear(sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

/**
 * @brief
 */
void *SZ_GetSpace(sizebuf_t *buf, int length)
{
	void *data;

	if (buf->cursize + length > buf->maxsize) {
		if (!buf->allowoverflow)
			Com_Error(ERR_FATAL, "SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Com_Error(ERR_FATAL, "SZ_GetSpace: %i is > full buffer size", length);

		Com_Printf("SZ_GetSpace: overflow\n");
		SZ_Clear(buf);
		buf->overflowed = qtrue;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

/**
 * @brief
 */
void SZ_Write(sizebuf_t *buf, void *data, int length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}

/**
 * @brief
 */
void SZ_WriteString(sizebuf_t *buf, char *data)
{
	int len;

	len = strlen(data) + 1;

	if (buf->cursize) {
		if (((uint8_t *)buf->data)[buf->cursize - 1]) {
			memcpy((uint8_t *) SZ_GetSpace(buf, len), data, len);	/* no trailing 0 */
		} else {
			memcpy((uint8_t *) SZ_GetSpace(buf, len - 1) - 1, data, len);	/* write over trailing 0 */
		}
	} else {
		memcpy((uint8_t *) SZ_GetSpace(buf, len), data, len);
	}
}


/*============================================================================ */


/**
 * @brief
 *
 * Returns the position (1 to argc-1) in the program's argument list
 * where the given parameter apears, or 0 if not present
 */
int COM_CheckParm(char *parm)
{
	int i;

	for (i = 1; i < com_argc; i++) {
		if (!strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

/**
 * @brief Returns the script commandline argument count
 */
int COM_Argc(void)
{
	return com_argc;
}

/**
 * @brief Returns an argument of script commandline
 */
char *COM_Argv(int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return "";
	return com_argv[arg];
}

/**
 * @brief
 */
void COM_ClearArgv(int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return;
	com_argv[arg] = "";
}


/**
 * @brief
 */
void COM_InitArgv(int argc, char **argv)
{
	int i;

	if (argc > MAX_NUM_ARGVS)
		Com_Error(ERR_FATAL, "argc > MAX_NUM_ARGVS");
	com_argc = argc;
	for (i = 0; i < argc; i++) {
		if (!argv[i] || strlen(argv[i]) >= MAX_TOKEN_CHARS)
			com_argv[i] = "";
		else
			com_argv[i] = argv[i];
	}
}

/**
 * @brief Adds the given string at the end of the current argument list
 */
void COM_AddParm(char *parm)
{
	if (com_argc == MAX_NUM_ARGVS)
		Com_Error(ERR_FATAL, "COM_AddParm: MAX_NUM)ARGS");
	com_argv[com_argc++] = parm;
}

/**
 * @brief
 *
 * just for debugging
 */
int memsearch(byte * start, int count, int search)
{
	int i;

	for (i = 0; i < count; i++)
		if (start[i] == search)
			return i;
	return -1;
}


/**
 * @brief
 */
char *CopyString(char *in)
{
	char *out;
	int l = strlen(in);

	out = Z_Malloc(l + 1);
	Q_strncpyz(out, in, l + 1);
	return out;
}


/**
 * @brief
 */
void Info_Print(char *s)
{
	char key[512];
	char value[512];
	char *o;
	int l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20) {
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Com_Printf("%s", key);

		if (!*s) {
			Com_Printf("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf("%s\n", value);
	}
}


/*
==============================================================================

ZONE MEMORY ALLOCATION

just cleared malloc with counters now...

==============================================================================
*/

#define	Z_MAGIC		0x1d1d

typedef struct zhead_s {
	struct zhead_s *prev, *next;
	short magic;
	short tag;					/* for group free */
	size_t size;
} zhead_t;

zhead_t z_chain;
int z_count, z_bytes;

/**
 * @brief Frees a Z_Malloc'ed pointer
 */
void Z_Free(void *ptr)
{
	zhead_t *z;

	if (!ptr)
		return;

	z = ((zhead_t *) ptr) - 1;

	if (z->magic != Z_MAGIC)
		Com_Error(ERR_FATAL, "Z_Free: bad magic (%i)", z->magic);

	z->prev->next = z->next;
	z->next->prev = z->prev;

	z_count--;
	z_bytes -= z->size;
	free(z);
}


/**
 * @brief Stats about the allocated bytes via Z_Malloc
 */
void Z_Stats_f(void)
{
	Com_Printf("%i bytes in %i blocks\n", z_bytes, z_count);
}

/**
 * @brief Frees a memory block with a given tag
 */
void Z_FreeTags(int tag)
{
	zhead_t *z, *next;

	for (z = z_chain.next; z != &z_chain; z = next) {
		next = z->next;
		if (z->tag == tag)
			Z_Free((void *) (z + 1));
	}
}

/**
 * @brief Allocates a memory block with a given tag
 *
 * and fills with 0
 */
void *Z_TagMalloc(size_t size, int tag)
{
	zhead_t *z;

	size = size + sizeof(zhead_t);
	z = malloc(size);
	if (!z)
		Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes", size);
	memset(z, 0, size);
	z_count++;
	z_bytes += size;
	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	return (void *) (z + 1);
}

/**
 * @brief Allocate a memory block with default tag
 *
 * and fills with 0
 */
void *Z_Malloc(size_t size)
{
	return Z_TagMalloc(size, 0);
}

/*======================================================== */

void Key_Init(void);
void SCR_EndLoadingPlaque(void);

/**
 * @brief
 *
 * Just throw a fatal error to
 * test error shutdown procedures
 */
void Com_Error_f(void)
{
	Com_Error(ERR_FATAL, "%s", Cmd_Argv(1));
}

#ifdef HAVE_GETTEXT
/**
 * @brief Gettext init function
 *
 * Initialize the locale settings for gettext
 * po files are searched in ./base/i18n
 * You can override the language-settings in setting
 * the cvar s_language to a valid language-string like
 * e.g. de_DE, en or en_US
 *
 * This function is only build in and called when
 * defining HAVE_GETTEXT
 * Under Linux see Makefile options for this
 *
 * @sa Qcommon_LocaleInit
 */
void Qcommon_LocaleInit(void)
{
	char *locale;

#ifdef _WIN32
	char languageID[32];
#endif
	s_language = Cvar_Get("s_language", "", CVAR_ARCHIVE);
	s_language->modified = qfalse;

#ifdef _WIN32
	Com_sprintf(languageID, 32, "LANG=%s", s_language->string);
	Q_putenv(languageID);
#else
#ifndef SOLARIS
	unsetenv("LANGUAGE");
#endif
#ifdef __APPLE__
	if (setenv("LANGUAGE", s_language->string, 1) == -1)
		Com_Printf("...setenv for LANGUAGE failed: %s\n", s_language->string);
	if (setenv("LC_ALL", s_language->string, 1) == -1)
		Com_Printf("...setenv for LC_ALL failed: %s\n", s_language->string);
#endif
#endif

	/* set to system default */
	setlocale(LC_ALL, "C");
	locale = setlocale(LC_MESSAGES, s_language->string);
	if (!locale) {
		Com_Printf("...could not set to language: %s\n", s_language->string);
		locale = setlocale(LC_MESSAGES, "");
		if (!locale) {
			Com_Printf("...could not set to system language\n");
			return;
		}
	} else {
		Com_Printf("...using language: %s\n", locale);
		Cvar_Set("s_language", locale);
	}
	s_language->modified = qfalse;
}
#endif

/**
 * @brief Init function
 *
 * @param[in] argc int
 * @param[in] argv char**
 * @sa Com_ParseScripts
 * @sa Sys_Init
 * @sa CL_Init
 * @sa Qcommon_LocaleInit
 *
 * To compile language support into UFO:AI you need to activate the preprocessor variable
 * HAVE_GETTEXT (for linux have a look at the makefile)
 */
void Qcommon_Init(int argc, char **argv)
{
	char *s;

#ifdef HAVE_GETTEXT
	/* i18n through gettext */
	char languagePath[MAX_OSPATH];
#endif

	if (setjmp(abortframe))
		Sys_Error("Error during initialization");

	z_chain.next = z_chain.prev = &z_chain;

	/* prepare enough of the subsystems to handle
	   cvar and command buffer management */
	COM_InitArgv(argc, argv);

	Swap_Init();
	Cbuf_Init();

	Cmd_Init();
	Cvar_Init();

	Key_Init();

	/* we need to add the early commands twice, because
	   a basedir needs to be set before execing
	   config files, but we want other parms to override
	   the settings of the config files */
	Cbuf_AddEarlyCommands(qfalse);
	Cbuf_Execute();

	FS_InitFilesystem();

	Cbuf_ExecuteText("exec default.cfg\n", EXEC_APPEND);
	Cbuf_ExecuteText("exec config.cfg\n", EXEC_APPEND);
	Cbuf_ExecuteText("exec keys.cfg\n", EXEC_APPEND);

	Cbuf_AddEarlyCommands(qtrue);
	Cbuf_Execute();

	/* init commands and vars */
	Cmd_AddCommand("z_stats", Z_Stats_f);
	Cmd_AddCommand("error", Com_Error_f);

	host_speeds = Cvar_Get("host_speeds", "0", 0);
	log_stats = Cvar_Get("log_stats", "0", 0);
	developer = Cvar_Get("developer", "0", 0);
	timescale = Cvar_Get("timescale", "1", 0);
	fixedtime = Cvar_Get("fixedtime", "0", 0);
	logfile_active = Cvar_Get("logfile", "1", 0);
	showtrace = Cvar_Get("showtrace", "0", 0);
#ifdef DEDICATED_ONLY
	dedicated = Cvar_Get("dedicated", "1", CVAR_NOSET);
#else
	dedicated = Cvar_Get("dedicated", "0", CVAR_NOSET);
#endif

	s = va("UFO: Alien Invasion %s %s %s %s", UFO_VERSION, CPUSTRING, __DATE__, BUILDSTRING);
	Cvar_Get("version", s, CVAR_SERVERINFO | CVAR_NOSET);
	Cvar_Get("ver", va("%4.2f", UFO_VERSION), CVAR_NOSET);

	if (dedicated->value)
		Cmd_AddCommand("quit", Com_Quit);

	Sys_Init();

	NET_Init();
	Netchan_Init();

	SV_Init();
	CL_Init();

#ifdef HAVE_GETTEXT
	/* i18n through gettext */
	setlocale(LC_ALL, "C");
	setlocale(LC_MESSAGES, "");
	/* use system locale dir if we can't find in gamedir */
	Com_sprintf(languagePath, MAX_QPATH, "%s/base/i18n/", FS_GetCwd());
	Com_DPrintf("...using mo files from %s\n", languagePath);
	bindtextdomain(TEXT_DOMAIN, languagePath);
	bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
	/* load language file */
	textdomain(TEXT_DOMAIN);

	Qcommon_LocaleInit();
#else
	Com_Printf("..no gettext compiled into this binary\n");
#endif

	Com_ParseScripts();

	/* add + commands from command line
	   if the user didn't give any commands, run default action */
	if (!Cbuf_AddLateCommands()) {
		if (!dedicated->value)
			Cbuf_AddText("init\n");
		else
			Cbuf_AddText("dedicated_start\n");
		Cbuf_Execute();
		/* the user asked for something explicit */
	} else {
		/* so drop the loading plaque */
		SCR_EndLoadingPlaque();
	}

#ifndef DEDICATED_ONLY
	if ((int) Cvar_VariableValue("cl_precachemenus"))
		MN_PrecacheMenus();
#endif

	Com_Printf("====== UFO Initialized ======\n\n");
}

/**
 * @brief This is the function that is called directly from main()
 * @sa main
 * @sa Qcommon_Init
 * @sa Qcommon_Shutdown
 * @sa SV_Frame
 * @sa CL_Frame
 */
float Qcommon_Frame(int msec)
{
	char *s;
	int time_before = 0, time_between = 0, time_after = 0;

	/* an ERR_DROP was thrown */
	if (setjmp(abortframe))
		return 1.0;

	if (timescale->value > 5.0)
		Cvar_SetValue("timescale", 5.0);
	else if (timescale->value < 0.2)
		Cvar_SetValue("timescale", 0.2);

	if (log_stats->modified) {
		log_stats->modified = qfalse;
		if (log_stats->value) {
			if (log_stats_file) {
				fclose(log_stats_file);
				log_stats_file = 0;
			}
			log_stats_file = fopen("stats.log", "w");
			if (log_stats_file)
				fprintf(log_stats_file, "entities,dlights,parts,frame time\n");
		} else {
			if (log_stats_file) {
				fclose(log_stats_file);
				log_stats_file = 0;
			}
		}
	}

/*	if (fixedtime->value) */
/*		msec = fixedtime->value; */

	if (showtrace->value) {
		extern int c_traces, c_brush_traces;
		extern int c_pointcontents;

		Com_Printf("%4i traces  %4i points\n", c_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}

	do {
		s = Sys_ConsoleInput();
		if (s)
			Cbuf_AddText(va("%s\n", s));
	} while (s);
	Cbuf_Execute();

	if (host_speeds->value)
		time_before = Sys_Milliseconds();

	SV_Frame(msec);

	if (host_speeds->value)
		time_between = Sys_Milliseconds();

	CL_Frame(msec);

	if (host_speeds->value)
		time_after = Sys_Milliseconds();


	if (host_speeds->value) {
		int all, sv, gm, cl, rf;

		all = time_after - time_before;
		sv = time_between - time_before;
		cl = time_after - time_between;
		gm = time_after_game - time_before_game;
		rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;
		Com_Printf("all:%3i sv:%3i gm:%3i cl:%3i rf:%3i\n", all, sv, gm, cl, rf);
	}

	return timescale->value;
}

/**
 * @brief
 */
void Qcommon_Shutdown(void)
{
}
