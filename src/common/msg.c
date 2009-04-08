/**
 * @file msg.c
 * @brief Message IO functions - handles byte ordering and avoids alignment errors
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
#include "msg.h"

const vec3_t bytedirs[NUMVERTEXNORMALS] = {
#include "../shared/vertex_normals.h"
};

/* writing functions */


void SZ_Init (sizebuf_t * buf, byte * data, int length)
{
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear (sizebuf_t * buf)
{
	buf->cursize = 0;
	buf->overflowed = qfalse;
}

void *SZ_GetSpace (sizebuf_t * buf, int length)
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

void SZ_Write (sizebuf_t * buf, const void *data, int length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print (sizebuf_t * buf, const char *data)
{
	int len;

	len = strlen(data) + 1;

	if (buf->cursize) {
		if (buf->data[buf->cursize - 1])
			memcpy((byte *) SZ_GetSpace(buf, len), data, len);	/* no trailing 0 */
		else
			memcpy((byte *) SZ_GetSpace(buf, len - 1) - 1, data, len);	/* write over trailing 0 */
	} else
		memcpy((byte *) SZ_GetSpace(buf, len), data, len);
}

/*=========================================================================== */

void MSG_WriteChar (sizebuf_t * sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < SCHAR_MIN || c > SCHAR_MAX)
		Com_Error(ERR_FATAL, "MSG_WriteChar: range error %i", c);
#endif

	buf = SZ_GetSpace(sb, 1);
	buf[0] = (char)c;
}

#ifdef DEBUG
void MSG_WriteByteDebug (sizebuf_t * sb, int c, const char *file, int line)
#else
void MSG_WriteByte (sizebuf_t * sb, int c)
#endif
{
	byte *buf;

	/* PARANOID is only possible in debug mode (when DEBUG was set, too) */
#ifdef DEBUG
	if (c < 0 || c > UCHAR_MAX)
		Com_Printf("MSG_WriteByte: range error %i ('%s', line %i)\n", c, file, line);
#endif

	buf = SZ_GetSpace(sb, 1);
	buf[0] = c & UCHAR_MAX;
}

#ifdef DEBUG
void MSG_WriteShortDebug (sizebuf_t * sb, int c, const char* file, int line)
#else
void MSG_WriteShort (sizebuf_t * sb, int c)
#endif
{
#if 1
	unsigned short *sp = (unsigned short *)SZ_GetSpace(sb, 2);
	*sp = LittleShort(c);
#else
	byte *buf;

	buf = SZ_GetSpace(sb, 2);
	buf[0] = c & UCHAR_MAX;
	buf[1] = (c >> 8) & UCHAR_MAX;
#endif

#ifdef DEBUG
	if (c < SHRT_MIN || c > SHRT_MAX)
		Com_Printf("MSG_WriteShort: range error %i ('%s', line %i)\n", c, file, line);
#endif
}

void MSG_WriteLong (sizebuf_t * sb, int c)
{
#if 1
	unsigned int *ip = (unsigned int *)SZ_GetSpace(sb, 4);
	*ip = LittleLong(c);
#else
	byte *buf;

	buf = SZ_GetSpace(sb, 4);
	buf[0] = c & UCHAR_MAX;
	buf[1] = (c >> 8) & UCHAR_MAX;
	buf[2] = (c >> 16) & UCHAR_MAX;
	buf[3] = (c >> 24) & UCHAR_MAX;
#endif
}

void MSG_WriteFloat (sizebuf_t * sb, float f)
{
	union {
		float f;
		int l;
	} dat;


	dat.f = f;
	dat.l = LittleLong(dat.l);

	SZ_Write(sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t * sb, const char *s)
{
	if (!s)
		SZ_Write(sb, "", 1);
	else
		SZ_Write(sb, s, strlen(s) + 1);
}

void MSG_WriteCoord (sizebuf_t * sb, float f)
{
	MSG_WriteLong(sb, (int) (f * 32));
}

/**
 * @sa MSG_Read2Pos
 */
void MSG_Write2Pos (sizebuf_t * sb, const vec2_t pos)
{
	MSG_WriteLong(sb, (long) (pos[0] * 32.));
	MSG_WriteLong(sb, (long) (pos[1] * 32.));
}

/**
 * @sa MSG_ReadPos
 */
void MSG_WritePos (sizebuf_t * sb, const vec3_t pos)
{
	MSG_WriteLong(sb, (long) (pos[0] * 32.));
	MSG_WriteLong(sb, (long) (pos[1] * 32.));
	MSG_WriteLong(sb, (long) (pos[2] * 32.));
}

void MSG_WriteGPos (sizebuf_t * sb, pos3_t pos)
{
	MSG_WriteByte(sb, pos[0]);
	MSG_WriteByte(sb, pos[1]);
	MSG_WriteByte(sb, pos[2]);
}

void MSG_WriteAngle (sizebuf_t * sb, float f)
{
	MSG_WriteByte(sb, (int) (f * 256 / 360) & 255);
}

void MSG_WriteAngle16 (sizebuf_t * sb, float f)
{
	MSG_WriteShort(sb, ANGLE2SHORT(f));
}


void MSG_WriteDir (sizebuf_t * sb, vec3_t dir)
{
	int i, best;
	float bestd;

	if (!dir) {
		MSG_WriteByte(sb, 0);
		return;
	}

	bestd = 0;
	best = 0;
	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		const float d = DotProduct(dir, bytedirs[i]);
		if (d > bestd) {
			bestd = d;
			best = i;
		}
	}
	MSG_WriteByte(sb, best);
}


/**
 * @brief Writes to buffer according to format; version without syntactic sugar for variable arguments, to call it from other functions with variable arguments
 */
void MSG_vWriteFormat (sizebuf_t * sb, const char *format, va_list ap)
{
	Com_DPrintf(DEBUG_ENGINE, "MSG_WriteFormat: %s\n", format);

	while (*format) {
		const char typeID = *format++;

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
				int i;
				const int n = va_arg(ap, int);
				const byte *p = va_arg(ap, byte *);

				MSG_WriteShort(sb, n);
				for (i = 0; i < n; i++)
					MSG_WriteByte(sb, *p++);
			}
			break;
		default:
			Com_Error(ERR_DROP, "WriteFormat: Unknown type!");
		}
	}
	/* Too many arguments for the given format; too few cause crash above */
	if (!ap)
		Com_Error(ERR_DROP, "WriteFormat: Too many arguments!");
}

/**
 * @brief The user-friendly version of MSG_WriteFormat that writes variable arguments to buffer according to format
 */
void MSG_WriteFormat (sizebuf_t * sb, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	MSG_vWriteFormat(sb, format, ap);
	va_end(ap);
}


/* reading functions */

void MSG_BeginReading (sizebuf_t * msg)
{
	msg->readcount = 0;
}

/**
 * returns -1 if no more characters are available
 */
int MSG_ReadChar (sizebuf_t * msg_read)
{
	int c;

	if (msg_read->readcount + 1 > msg_read->cursize)
		c = -1;
	else
		c = (signed char) msg_read->data[msg_read->readcount];
	msg_read->readcount++;

	return c;
}

/**
 * @brief Reads a byte from the netchannel
 * @note Beware that you don't put this into a byte or short - this will overflow
 * use an int value to store the return value!!!
 */
int MSG_ReadByte (sizebuf_t * msg_read)
{
	int c;

	if (msg_read->readcount + 1 > msg_read->cursize)
		c = -1;
	else
		c = (unsigned char) msg_read->data[msg_read->readcount];
	msg_read->readcount++;

	return c;
}

int MSG_ReadShort (sizebuf_t * msg_read)
{
	int c;

	if (msg_read->readcount + 2 > msg_read->cursize)
		c = -1;
	else {
#if 1
		short *sp = (short *)&msg_read->data[msg_read->readcount];
		c = LittleShort(*sp);
#else
		c = (short) (msg_read->data[msg_read->readcount]
					+ (msg_read->data[msg_read->readcount + 1] << 8));
#endif
	}

	msg_read->readcount += 2;

	return c;
}

int MSG_ReadLong (sizebuf_t * msg_read)
{
	int c;

	if (msg_read->readcount + 4 > msg_read->cursize)
		c = -1;
	else {
#if 1
		unsigned int *ip = (unsigned int *)&msg_read->data[msg_read->readcount];
		c = LittleLong(*ip);
#else
		c = msg_read->data[msg_read->readcount]
			+ (msg_read->data[msg_read->readcount + 1] << 8)
			+ (msg_read->data[msg_read->readcount + 2] << 16)
			+ (msg_read->data[msg_read->readcount + 3] << 24);
#endif
	}

	msg_read->readcount += 4;

	return c;
}

float MSG_ReadFloat (sizebuf_t * msg_read)
{
	union {
		byte b[4];
		float f;
		int l;
	} dat;

	if (msg_read->readcount + 4 > msg_read->cursize)
		dat.f = -1;
	else {
		dat.b[0] = msg_read->data[msg_read->readcount];
		dat.b[1] = msg_read->data[msg_read->readcount + 1];
		dat.b[2] = msg_read->data[msg_read->readcount + 2];
		dat.b[3] = msg_read->data[msg_read->readcount + 3];
	}
	msg_read->readcount += 4;

	dat.l = LittleLong(dat.l);

	return dat.f;
}

/**
 * @note Don't use this function in a way like
 * <code> char *s = MSG_ReadString(sb);
 * char *t = MSG_ReadString(sb);</code>
 * The second reading uses the same data buffer for the string - so
 * s is no longer the first - but the second string
 * @note strip high bits - don't use this for utf-8 strings
 * @sa MSG_ReadStringLine
 */
char *MSG_ReadString (sizebuf_t * msg_read)
{
	static char string[2048];
	unsigned int l;
	int c;

	l = 0;
	do {
		c = MSG_ReadByte(msg_read);
		if (c == -1 || c == 0)
			break;
		/* translate all format specs to avoid crash bugs */
		/* don't allow higher ascii values */
		if (c == '%' || c > 127)
			c = '.';
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord (sizebuf_t * msg_read)
{
	return (float) MSG_ReadLong(msg_read) * (1.0 / 32);
}

/**
 * @sa MSG_Write2Pos
 */
void MSG_Read2Pos (sizebuf_t * msg_read, vec2_t pos)
{
	pos[0] = MSG_ReadLong(msg_read) / 32.;
	pos[1] = MSG_ReadLong(msg_read) / 32.;
}

/**
 * @sa MSG_WritePos
 */
void MSG_ReadPos (sizebuf_t * msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadLong(msg_read) / 32.;
	pos[1] = MSG_ReadLong(msg_read) / 32.;
	pos[2] = MSG_ReadLong(msg_read) / 32.;
}

/**
 * @sa MSG_WriteGPos
 * @sa MSG_ReadByte
 * @note pos3_t are byte values
 */
void MSG_ReadGPos (sizebuf_t * msg_read, pos3_t pos)
{
	pos[0] = MSG_ReadByte(msg_read);
	pos[1] = MSG_ReadByte(msg_read);
	pos[2] = MSG_ReadByte(msg_read);
}

float MSG_ReadAngle (sizebuf_t * msg_read)
{
	return (float) MSG_ReadChar(msg_read) * (360.0 / 256);
}

float MSG_ReadAngle16 (sizebuf_t * msg_read)
{
	short s;

	s = MSG_ReadShort(msg_read);
	return (float) SHORT2ANGLE(s);
}

void MSG_ReadData (sizebuf_t * msg_read, void *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
		((byte *) data)[i] = MSG_ReadByte(msg_read);
}

void MSG_ReadDir (sizebuf_t * sb, vec3_t dir)
{
	int b;

	b = MSG_ReadByte(sb);
	if (b >= NUMVERTEXNORMALS)
		Com_Error(ERR_DROP, "MSG_ReadDir: out of range");
	VectorCopy(bytedirs[b], dir);
}


/**
 * @brief Reads from a buffer according to format; version without syntactic sugar for variable arguments, to call it from other functions with variable arguments
 * @sa SV_ReadFormat
 * @param[in] format The format string (see e.g. pa_format array) - may not be NULL
 */
void MSG_vReadFormat (sizebuf_t * msg_read, const char *format, va_list ap)
{
	assert(format); /* may not be null */
	while (*format) {
		const char typeID = *format++;

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

				n = MSG_ReadShort(msg_read);
				*va_arg(ap, int *) = n;
				p = va_arg(ap, void *);

				for (i = 0; i < n; i++)
					*p++ = MSG_ReadByte(msg_read);
			}
			break;
		default:
			Com_Error(ERR_DROP, "ReadFormat: Unknown type!");
		}
	}
	/* Too many arguments for the given format; too few cause crash above */
	if (!ap)
		Com_Error(ERR_DROP, "ReadFormat: Too many arguments!");
}

/**
 * @brief The user-friendly version of MSG_ReadFormat that reads variable arguments from a buffer according to format
 */
void MSG_ReadFormat (sizebuf_t * msg_read, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	MSG_vReadFormat(msg_read, format, ap);
	va_end(ap);
}


/**
 * @brief returns the length of a sizebuf_t
 *
 * calculated the length of a sizebuf_t by summing up
 * the size of each format char
 */
int MSG_LengthFormat (sizebuf_t * sb, const char *format)
{
	int length, delta = 0;
	int oldCount;

	length = 0;
	oldCount = sb->readcount;

	while (*format) {
		const char typeID = *format++;

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
			delta = 12;
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
			delta = 0;
			break;
		case '*':
			delta = MSG_ReadShort(sb);
			length += 2;
			break;
		default:
			Com_Error(ERR_DROP, "LengthFormat: Unknown type!");
		}

		/* advance in buffer */
		sb->readcount += delta;
		length += delta;
	}

	sb->readcount = oldCount;
	return length;
}
