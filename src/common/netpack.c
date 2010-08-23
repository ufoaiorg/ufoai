/**
 * @file netpack.c
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

const vec3_t bytedirs[] = {
#include "../shared/vertex_normals.h"
};

void NET_WriteChar (struct dbuffer *buf, char c)
{
	dbuffer_add(buf, &c, 1);
}

void NET_WriteByte (struct dbuffer *buf, byte c)
{
	dbuffer_add(buf, (char *)&c, 1);
}

void NET_WriteShort (struct dbuffer *buf, int c)
{
	unsigned short v = LittleShort(c);
	dbuffer_add(buf, (char *)&v, 2);
}

void NET_WriteLong (struct dbuffer *buf, int c)
{
	int v = LittleLong(c);
	dbuffer_add(buf, (char *)&v, 4);
}

void NET_WriteString (struct dbuffer *buf, const char *str)
{
	if (!str)
		dbuffer_add(buf, "", 1);
	else
		dbuffer_add(buf, str, strlen(str) + 1);
}

void NET_WriteRawString (struct dbuffer *buf, const char *str)
{
	if (str)
		dbuffer_add(buf, str, strlen(str));
}

void NET_WriteCoord (struct dbuffer *buf, float f)
{
	NET_WriteLong(buf, (int) (f * 32));
}

/**
 * @sa NET_Read2Pos
 */
void NET_Write2Pos (struct dbuffer *buf, vec2_t pos)
{
	NET_WriteLong(buf, (long) (pos[0] * 32.));
	NET_WriteLong(buf, (long) (pos[1] * 32.));
}

/**
 * @sa NET_ReadPos
 */
void NET_WritePos (struct dbuffer *buf, const vec3_t pos)
{
	NET_WriteLong(buf, (long) (pos[0] * 32.));
	NET_WriteLong(buf, (long) (pos[1] * 32.));
	NET_WriteLong(buf, (long) (pos[2] * 32.));
}

void NET_WriteGPos (struct dbuffer *buf, const pos3_t pos)
{
	NET_WriteByte(buf, pos[0]);
	NET_WriteByte(buf, pos[1]);
	NET_WriteByte(buf, pos[2]);
}

void NET_WriteAngle (struct dbuffer *buf, float f)
{
	NET_WriteByte(buf, (int) (f * 256 / 360) & 255);
}

void NET_WriteAngle16 (struct dbuffer *buf, float f)
{
	NET_WriteShort(buf, ANGLE2SHORT(f));
}

/**
 * @note EV_ACTOR_SHOOT is using WriteDir for writing the normal, but ReadByte
 * for reading it - keep that in mind when you change something here
 */
void NET_WriteDir (struct dbuffer *buf, const vec3_t dir)
{
	int i, best;
	float bestd;
	size_t bytedirsLength;

	if (!dir) {
		NET_WriteByte(buf, 0);
		return;
	}

	bestd = 0;
	best = 0;
	bytedirsLength = lengthof(bytedirs);
	for (i = 0; i < bytedirsLength; i++) {
		const float d = DotProduct(dir, bytedirs[i]);
		if (d > bestd) {
			bestd = d;
			best = i;
		}
	}
	NET_WriteByte(buf, best);
}


/**
 * @brief Writes to buffer according to format; version without syntactic sugar
 * for variable arguments, to call it from other functions with variable arguments
 */
void NET_vWriteFormat (struct dbuffer *buf, const char *format, va_list ap)
{
	char typeID;

	while (*format) {
		typeID = *format++;

		switch (typeID) {
		case 'c':
			NET_WriteChar(buf, va_arg(ap, int));

			break;
		case 'b':
			NET_WriteByte(buf, va_arg(ap, int));

			break;
		case 's':
			NET_WriteShort(buf, va_arg(ap, int));

			break;
		case 'l':
			NET_WriteLong(buf, va_arg(ap, int));

			break;
		case 'p':
			NET_WritePos(buf, va_arg(ap, float *));

			break;
		case 'g':
			NET_WriteGPos(buf, va_arg(ap, byte *));
			break;
		case 'd':
			NET_WriteDir(buf, va_arg(ap, float *));

			break;
		case 'a':
			/* NOTE: float is promoted to double through ... */
			NET_WriteAngle(buf, va_arg(ap, double));

			break;
		case '!':
			break;
		case '&':
			NET_WriteString(buf, va_arg(ap, char *));
			break;
		case '*':
			{
				int i, n;
				byte *p;

				n = va_arg(ap, int);

				p = va_arg(ap, byte *);
				NET_WriteShort(buf, n);
				for (i = 0; i < n; i++)
					NET_WriteByte(buf, *p++);
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
 * @brief The user-friendly version of NET_WriteFormat that writes variable arguments to buffer according to format
 */
void NET_WriteFormat (struct dbuffer *buf, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	NET_vWriteFormat(buf, format, ap);
	va_end(ap);
}


/* reading functions */

/**
 * returns -1 if no more characters are available
 */
int NET_ReadChar (struct dbuffer *buf)
{
	char c;
	if (dbuffer_extract(buf, &c, 1) == 0)
		return -1;
	else
		return c;
}

/**
 * @brief Reads a byte from the netchannel
 * @note Beware that you don't put this into a byte or short - this will overflow
 * use an int value to store the return value when you read this via the net format strings!!!
 */
int NET_ReadByte (struct dbuffer *buf)
{
	unsigned char c;
	if (dbuffer_extract(buf, (char *)&c, 1) == 0)
		return -1;
	else
		return c;
}

int NET_ReadShort (struct dbuffer *buf)
{
	unsigned short v;
	if (dbuffer_extract(buf, (char *)&v, 2) < 2)
		return -1;

	return LittleShort(v);
}

/**
 * @brief Peeks into a buffer without changing it to get a short int.
 * @param buf The buffer, returned unchanged, no need to be copied before.
 * @return The short at the beginning of the buffer, -1 if it couldn't be read.
 */
int NET_PeekShort (const struct dbuffer *buf)
{
	uint16_t v;
	if (dbuffer_get(buf, (char *)&v, 2) < 2)
		return -1;

	return LittleShort(v);
}

int NET_ReadLong (struct dbuffer *buf)
{
	unsigned int v;
	if (dbuffer_extract(buf, (char *)&v, 4) < 4)
		return -1;

	return LittleLong(v);
}

/**
 * @note Don't use this function in a way like
 * <code> char *s = NET_ReadString(sb);
 * char *t = NET_ReadString(sb);</code>
 * The second reading uses the same data buffer for the string - so
 * s is no longer the first - but the second string
 * @note strip high bits - don't use this for utf-8 strings
 * @sa NET_ReadStringLine
 * @param[in,out] buf The input buffer to read the string data from
 * @param[out] string The output buffer to read the string into
 * @param[in] length The size of the output buffer
 */
int NET_ReadString (struct dbuffer *buf, char *string, size_t length)
{
	unsigned int l;

	l = 0;
	do {
		int c = NET_ReadByte(buf);
		if (c == -1 || c == 0)
			break;
		/* translate all format specs to avoid crash bugs */
		/* don't allow higher ascii values */
		if (c == '%' || c > 127)
			c = '.';
		string[l] = c;
		l++;
	} while (l < length - 1);

	string[l] = 0;

	return l;
}

/**
 * @sa NET_ReadString
 */
int NET_ReadStringLine (struct dbuffer *buf, char *string, size_t length)
{
	unsigned int l;

	l = 0;
	do {
		int c = NET_ReadByte(buf);
		if (c == -1 || c == 0 || c == '\n')
			break;
		/* translate all format specs to avoid crash bugs */
		/* don't allow higher ascii values */
		if (c == '%' || c > 127)
			c = '.';
		string[l] = c;
		l++;
	} while (l < length - 1);

	string[l] = 0;

	return l;
}

float NET_ReadCoord (struct dbuffer *buf)
{
	return (float) NET_ReadLong(buf) * (1.0 / 32);
}

/**
 * @sa NET_Write2Pos
 */
void NET_Read2Pos (struct dbuffer *buf, vec2_t pos)
{
	pos[0] = NET_ReadLong(buf) / 32.;
	pos[1] = NET_ReadLong(buf) / 32.;
}

/**
 * @sa NET_WritePos
 */
void NET_ReadPos (struct dbuffer *buf, vec3_t pos)
{
	pos[0] = NET_ReadLong(buf) / 32.;
	pos[1] = NET_ReadLong(buf) / 32.;
	pos[2] = NET_ReadLong(buf) / 32.;
}

/**
 * @sa NET_WriteGPos
 * @sa NET_ReadByte
 * @note pos3_t are byte values
 */
void NET_ReadGPos (struct dbuffer *buf, pos3_t pos)
{
	pos[0] = NET_ReadByte(buf);
	pos[1] = NET_ReadByte(buf);
	pos[2] = NET_ReadByte(buf);
}

float NET_ReadAngle (struct dbuffer *buf)
{
	return (float) NET_ReadChar(buf) * (360.0 / 256);
}

float NET_ReadAngle16 (struct dbuffer *buf)
{
	short s;

	s = NET_ReadShort(buf);
	return (float) SHORT2ANGLE(s);
}

void NET_ReadData (struct dbuffer *buf, void *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
		((byte *) data)[i] = NET_ReadByte(buf);
}

void NET_ReadDir (struct dbuffer *buf, vec3_t dir)
{
	const int b = NET_ReadByte(buf);
	if (b >= lengthof(bytedirs))
		Com_Error(ERR_DROP, "NET_ReadDir: out of range");
	VectorCopy(bytedirs[b], dir);
}


/**
 * @brief Reads from a buffer according to format; version without syntactic sugar for variable arguments, to call it from other functions with variable arguments
 * @sa SV_ReadFormat
 * @param[in] buf The buffer we read the data from
 * @param[in] format The format string may not be NULL
 */
void NET_vReadFormat (struct dbuffer *buf, const char *format, va_list ap)
{
	while (*format) {
		const char typeID = *format++;

		switch (typeID) {
		case 'c':
			*va_arg(ap, int *) = NET_ReadChar(buf);
			break;
		case 'b':
			*va_arg(ap, int *) = NET_ReadByte(buf);
			break;
		case 's':
			*va_arg(ap, int *) = NET_ReadShort(buf);
			break;
		case 'l':
			*va_arg(ap, int *) = NET_ReadLong(buf);
			break;
		case 'p':
			NET_ReadPos(buf, *va_arg(ap, vec3_t *));
			break;
		case 'g':
			NET_ReadGPos(buf, *va_arg(ap, pos3_t *));
			break;
		case 'd':
			NET_ReadDir(buf, *va_arg(ap, vec3_t *));
			break;
		case 'a':
			*va_arg(ap, float *) = NET_ReadAngle(buf);
			break;
		case '!':
			format++;
			break;
		case '&': {
			char *str = va_arg(ap, char *);
			const size_t length = va_arg(ap, size_t);
			NET_ReadString(buf, str, length);
			break;
		}
		case '*':
			{
				int i;
				byte *p;
				const int n = NET_ReadShort(buf);

				*va_arg(ap, int *) = n;
				p = va_arg(ap, byte *);

				for (i = 0; i < n; i++)
					*p++ = NET_ReadByte(buf);
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
 * @brief The user-friendly version of NET_ReadFormat that reads variable arguments from a buffer according to format
 */
void NET_ReadFormat (struct dbuffer *buf, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	NET_vReadFormat(buf, format, ap);
	va_end(ap);
}

/**
 * @brief Out of band print
 * @sa clc_oob
 * @sa CL_ReadPackets
 * @sa SV_ConnectionlessPacket
 */
void NET_OOB_Printf (struct net_stream *s, const char *format, ...)
{
	va_list argptr;
	char string[4096];
	const char cmd = (const char)clc_oob;
	int len;

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	len = LittleLong(strlen(string) + 1);
	NET_StreamEnqueue(s, (const char *)&len, 4);
	NET_StreamEnqueue(s, &cmd, 1);
	NET_StreamEnqueue(s, string, strlen(string));
}

/**
 * @brief Enqueue the buffer in the net stream for ONE client
 * @note Frees the msg buffer
 * @sa NET_WriteConstMsg
 */
void NET_WriteMsg (struct net_stream *s, struct dbuffer *buf)
{
	char tmp[4096];
	int len = LittleLong(dbuffer_len(buf));
	NET_StreamEnqueue(s, (char *)&len, 4);

	while (dbuffer_len(buf)) {
		len = dbuffer_extract(buf, tmp, sizeof(tmp));
		NET_StreamEnqueue(s, tmp, len);
	}

	/* and now free the buffer */
	free_dbuffer(buf);
}

/**
 * @brief Enqueue the buffer in the net stream for MULTIPLE clients
 * @note Same as NET_WriteMsg but doesn't free the buffer, use this if you send
 * the same buffer to more than one connected clients
 * @note Make sure that you free the msg buffer after you called this
 * @sa NET_WriteMsg
 */
void NET_WriteConstMsg (struct net_stream *s, const struct dbuffer *buf)
{
	char tmp[4096];
	int len = LittleLong(dbuffer_len(buf));
	int pos = 0;
	NET_StreamEnqueue(s, (char *)&len, 4);

	while (pos < dbuffer_len(buf)) {
		const int x = dbuffer_get_at(buf, pos, tmp, sizeof(tmp));
		assert(x > 0);
		NET_StreamEnqueue(s, tmp, x);
		pos += x;
	}
}

/**
 * @brief Reads messages from the network channel and adds them to the dbuffer
 * where you can use the NET_Read* functions to get the values in the correct
 * order
 * @sa NET_StreamDequeue
 * @sa dbuffer_add
 */
struct dbuffer *NET_ReadMsg (struct net_stream *s)
{
	unsigned int v;
	unsigned int len;
	struct dbuffer *buf;
	char tmp[4096];
	if (NET_StreamPeek(s, (char *)&v, 4) < 4)
		return NULL;

	len = LittleLong(v);
	if (NET_StreamGetLength(s) < (4 + len))
		return NULL;

	NET_StreamDequeue(s, tmp, 4);

	buf = new_dbuffer();
	while (len > 0) {
		const int x = NET_StreamDequeue(s, tmp, min(len, 4096));
		dbuffer_add(buf, tmp, x);
		len -= x;
	}

	return buf;
}

void NET_VPrintf (struct dbuffer *buf, const char *format, va_list ap, char *str, size_t length)
{
	const int len = Q_vsnprintf(str, length, format, ap);
	dbuffer_add(buf, str, len);
}
