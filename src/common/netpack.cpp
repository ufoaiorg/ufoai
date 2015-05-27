/**
 * @file
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

static const float POSSCALE = 32.0f;

void NET_WriteChar (dbuffer* buf, char c)
{
	buf->add(&c, 1);
	Com_DPrintf(DEBUG_EVENTSYS, "char event data: %s (%i)\n", Com_ByteToBinary(c), c);
}

void NET_WriteByte (dbuffer* buf, byte c)
{
	buf->add((const char*)&c, 1);
	Com_DPrintf(DEBUG_EVENTSYS, "byte event data: %s (%i)\n", Com_ByteToBinary(c), c);
}

void NET_WriteShort (dbuffer* buf, int c)
{
	const unsigned short v = LittleShort(c);
	buf->add((const char*)&v, 2);
	Com_DPrintf(DEBUG_EVENTSYS, "short event data: %i\n", c);
}

void NET_WriteLong (dbuffer* buf, int c)
{
	const int v = LittleLong(c);
	buf->add((const char*)&v, 4);
	Com_DPrintf(DEBUG_EVENTSYS, "long event data: %i\n", c);
}

void NET_WriteString (dbuffer* buf, const char* str)
{
	if (!str)
		buf->add("", 1);
	else
		buf->add(str, strlen(str) + 1);
	Com_DPrintf(DEBUG_EVENTSYS, "string event data: %s\n", str);
}

/**
 * @brief Skip the zero string terminal character. If you need it, use @c NET_WriteString.
 */
void NET_WriteRawString (dbuffer* buf, const char* str)
{
	if (!str)
		return;
	buf->add(str, strlen(str));
	Com_DPrintf(DEBUG_EVENTSYS, "string raw event data: %s\n", str);
}

void NET_WriteCoord (dbuffer* buf, float f)
{
	NET_WriteLong(buf, (int) (f * 32));
}

/**
 * @sa NET_Read2Pos
 */
void NET_Write2Pos (dbuffer* buf, const vec2_t pos)
{
	NET_WriteLong(buf, (long) (pos[0] * POSSCALE));
	NET_WriteLong(buf, (long) (pos[1] * POSSCALE));
}

/**
 * @sa NET_ReadPos
 */
void NET_WritePos (dbuffer* buf, const vec3_t pos)
{
	NET_WriteLong(buf, (long) (pos[0] * POSSCALE));
	NET_WriteLong(buf, (long) (pos[1] * POSSCALE));
	NET_WriteLong(buf, (long) (pos[2] * POSSCALE));
}

void NET_WriteGPos (dbuffer* buf, const pos3_t pos)
{
	NET_WriteByte(buf, pos[0]);
	NET_WriteByte(buf, pos[1]);
	NET_WriteByte(buf, pos[2]);
}

void NET_WriteAngle (dbuffer* buf, float f)
{
	NET_WriteByte(buf, (int) (f * 256 / 360) & 255);
}

void NET_WriteAngle16 (dbuffer* buf, float f)
{
	NET_WriteShort(buf, ANGLE2SHORT(f));
}

/**
 * @note EV_ACTOR_SHOOT is using WriteDir for writing the normal, but ReadByte
 * for reading it - keep that in mind when you change something here
 */
void NET_WriteDir (dbuffer* buf, const vec3_t dir)
{
	if (!dir) {
		NET_WriteByte(buf, 0);
		return;
	}

	float bestd = 0.0f;
	int best = 0;
	const size_t bytedirsLength = lengthof(bytedirs);
	for (int i = 0; i < bytedirsLength; i++) {
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
 * @note short and char are promoted to int when passed to variadic functions!
 */
void NET_vWriteFormat (dbuffer* buf, const char* format, va_list ap)
{
	while (*format) {
		const char typeID = *format++;

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
			NET_WritePos(buf, va_arg(ap, float*));
			break;
		case 'g':
			NET_WriteGPos(buf, va_arg(ap, byte*));
			break;
		case 'd':
			NET_WriteDir(buf, va_arg(ap, float*));
			break;
		case 'a':
			/* NOTE: float is promoted to double through ... */
			NET_WriteAngle(buf, va_arg(ap, double));
			break;
		case '!':
			break;
		case '&':
			NET_WriteString(buf, va_arg(ap, char*));
			break;
		case '*': {
			const int n = va_arg(ap, int);
			const byte* p = va_arg(ap, byte*);
			NET_WriteShort(buf, n);
			for (int i = 0; i < n; i++)
				NET_WriteByte(buf, *p++);
			break;
		}
		default:
			Com_Error(ERR_DROP, "WriteFormat: Unknown type!");
		}
	}
	/* Too many arguments for the given format; too few cause crash above */
#ifdef PARANOID
	if (!ap)
		Com_Error(ERR_DROP, "WriteFormat: Too many arguments!");
#endif
}

/**
 * @brief The user-friendly version of NET_WriteFormat that writes variable arguments to buffer according to format
 */
void NET_WriteFormat (dbuffer* buf, const char* format, ...)
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
int NET_ReadChar (dbuffer* buf)
{
	char c;
	if (buf->extract(&c, 1) == 0)
		return -1;
	return c;
}

/**
 * @brief Reads a byte from the netchannel
 * @note Beware that you don't put this into a byte or short - this will overflow
 * use an int value to store the return value when you read this via the net format strings!!!
 */
int NET_ReadByte (dbuffer* buf)
{
	unsigned char c;
	if (buf->extract((char*)&c, 1) == 0)
		return -1;
	return c;
}

int NET_ReadShort (dbuffer* buf)
{
	unsigned short v;
	if (buf->extract((char*)&v, 2) < 2)
		return -1;

	return LittleShort(v);
}

int NET_PeekByte (const dbuffer* buf)
{
	unsigned char c;
	if (buf->get((char*)&c, 1) == 0)
		return -1;
	return c;
}

/**
 * @brief Peeks into a buffer without changing it to get a short int.
 * @param buf The buffer, returned unchanged, no need to be copied before.
 * @return The short at the beginning of the buffer, -1 if it couldn't be read.
 */
int NET_PeekShort (const dbuffer* buf)
{
	uint16_t v;
	if (buf->get((char*)&v, 2) < 2)
		return -1;

	return LittleShort(v);
}

int NET_PeekLong (const dbuffer* buf)
{
	uint32_t v;
	if (buf->get((char*)&v, 4) < 4)
		return -1;

	return LittleLong(v);
}

int NET_ReadLong (dbuffer* buf)
{
	unsigned int v;
	if (buf->extract((char*)&v, 4) < 4)
		return -1;

	return LittleLong(v);
}

/**
 * @note Don't use this function in a way like
 * <code> char* s = NET_ReadString(sb);
 * char* t = NET_ReadString(sb);</code>
 * The second reading uses the same data buffer for the string - so
 * s is no longer the first - but the second string
 * @sa NET_ReadStringLine
 * @param[in,out] buf The input buffer to read the string data from
 * @param[out] string The output buffer to read the string into
 * @param[in] length The size of the output buffer
 */
int NET_ReadString (dbuffer* buf, char* string, size_t length)
{
	unsigned int l = 0;

	for (;;) {
		int c = NET_ReadByte(buf);
		if (c == -1 || c == 0)
			break;
		if (string && l < length - 1) {
			/* translate all format specs to avoid crash bugs */
			if (c == '%')
				c = '.';
			string[l] = c;
		}
		l++;
	}

	if (string)
		string[l] = '\0';

	return l;
}

/**
 * @sa NET_ReadString
 */
int NET_ReadStringLine (dbuffer* buf, char* string, size_t length)
{
	unsigned int l = 0;
	do {
		int c = NET_ReadByte(buf);
		if (c == -1 || c == 0 || c == '\n')
			break;
		/* translate all format specs to avoid crash bugs */
		if (c == '%')
			c = '.';
		string[l] = c;
		l++;
	} while (l < length - 1);

	string[l] = 0;

	return l;
}

float NET_ReadCoord (dbuffer* buf)
{
	return (float) NET_ReadLong(buf) * (1.0 / 32);
}

/**
 * @sa NET_Write2Pos
 */
void NET_Read2Pos (dbuffer* buf, vec2_t pos)
{
	pos[0] = NET_ReadLong(buf) / POSSCALE;
	pos[1] = NET_ReadLong(buf) / POSSCALE;
}

/**
 * @sa NET_WritePos
 */
void NET_ReadPos (dbuffer* buf, vec3_t pos)
{
	pos[0] = NET_ReadLong(buf) / POSSCALE;
	pos[1] = NET_ReadLong(buf) / POSSCALE;
	pos[2] = NET_ReadLong(buf) / POSSCALE;
}

/**
 * @sa NET_WriteGPos
 * @sa NET_ReadByte
 * @note pos3_t are byte values
 */
void NET_ReadGPos (dbuffer* buf, pos3_t pos)
{
	pos[0] = NET_ReadByte(buf);
	pos[1] = NET_ReadByte(buf);
	pos[2] = NET_ReadByte(buf);
}

float NET_ReadAngle (dbuffer* buf)
{
	return (float) NET_ReadChar(buf) * (360.0f / 256);
}

float NET_ReadAngle16 (dbuffer* buf)
{
	const short s = NET_ReadShort(buf);
	return (float) SHORT2ANGLE(s);
}

void NET_ReadData (dbuffer* buf, void* data, int len)
{
	for (int i = 0; i < len; i++)
		((byte*) data)[i] = NET_ReadByte(buf);
}

void NET_ReadDir (dbuffer* buf, vec3_t dir)
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
 * @param[in] format The format string may not be nullptr
 * @param ap The variadic function argument list corresponding to the format string
 */
void NET_vReadFormat (dbuffer* buf, const char* format, va_list ap)
{
	while (*format) {
		const char typeID = *format++;

		switch (typeID) {
		case 'c':
			*va_arg(ap, int*) = NET_ReadChar(buf);
			break;
		case 'b':
			*va_arg(ap, int*) = NET_ReadByte(buf);
			break;
		case 's':
			*va_arg(ap, int*) = NET_ReadShort(buf);
			break;
		case 'l':
			*va_arg(ap, int*) = NET_ReadLong(buf);
			break;
		case 'p':
			NET_ReadPos(buf, *va_arg(ap, vec3_t*));
			break;
		case 'g':
			NET_ReadGPos(buf, *va_arg(ap, pos3_t*));
			break;
		case 'd':
			NET_ReadDir(buf, *va_arg(ap, vec3_t*));
			break;
		case 'a':
			*va_arg(ap, float*) = NET_ReadAngle(buf);
			break;
		case '!':
			format++;
			break;
		case '&': {
			char* str = va_arg(ap, char*);
			const size_t length = va_arg(ap, size_t);
			NET_ReadString(buf, str, length);
			break;
		}
		case '*':
			{
				const int n = NET_ReadShort(buf);

				*va_arg(ap, int*) = n;
				byte* p = va_arg(ap, byte*);

				for (int i = 0; i < n; i++)
					*p++ = NET_ReadByte(buf);
			}
			break;
		default:
			Com_Error(ERR_DROP, "ReadFormat: Unknown type!");
		}
	}
	/* Too many arguments for the given format; too few cause crash above */
#ifdef PARANOID
	if (!ap)
		Com_Error(ERR_DROP, "ReadFormat: Too many arguments!");
#endif
}

void NET_SkipFormat (dbuffer* buf, const char* format)
{
	while (*format) {
		const char typeID = *format++;

		switch (typeID) {
		case 'c':
			NET_ReadChar(buf);
			break;
		case 'b':
			NET_ReadByte(buf);
			break;
		case 's':
			NET_ReadShort(buf);
			break;
		case 'l':
			NET_ReadLong(buf);
			break;
		case 'p': {
			vec3_t v;
			NET_ReadPos(buf, v);
			break;
		}
		case 'g': {
			pos3_t p;
			NET_ReadGPos(buf, p);
			break;
		}
		case 'd': {
			vec3_t v;
			NET_ReadDir(buf, v);
			break;
		}
		case 'a':
			NET_ReadAngle(buf);
			break;
		case '!':
			format++;
			break;
		case '&':
			NET_ReadString(buf, nullptr, 0);
			break;
		case '*': {
			const int n = NET_ReadShort(buf);
			for (int i = 0; i < n; i++)
				NET_ReadByte(buf);
			break;
		}
		default:
			Com_Error(ERR_DROP, "ReadFormat: Unknown type!");
		}
	}
}

/**
 * @brief The user-friendly version of NET_ReadFormat that reads variable arguments from a buffer according to format
 */
void NET_ReadFormat (dbuffer* buf, const char* format, ...)
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
void NET_OOB_Printf (struct net_stream* s, const char* format, ...)
{
	va_list argptr;
	char string[256];
	const char cmd = (const char)clc_oob;

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	const int len = LittleLong(strlen(string) + 1);
	NET_StreamEnqueue(s, (const char*)&len, 4);
	NET_StreamEnqueue(s, &cmd, 1);
	NET_StreamEnqueue(s, string, strlen(string));
}

/**
 * @brief Enqueue the buffer in the net stream for ONE client
 * @note Frees the msg buffer
 * @sa NET_WriteConstMsg
 */
void NET_WriteMsg (struct net_stream* s, dbuffer& buf)
{
	char tmp[256];
	int len = LittleLong(buf.length());
	NET_StreamEnqueue(s, (char*)&len, 4);

	while (buf.length()) {
		len = buf.extract(tmp, sizeof(tmp));
		NET_StreamEnqueue(s, tmp, len);
	}
}

/**
 * @brief Enqueue the buffer in the net stream for MULTIPLE clients
 * @note Same as NET_WriteMsg but doesn't free the buffer, use this if you send
 * the same buffer to more than one connected clients
 * @note Make sure that you free the msg buffer after you called this
 * @sa NET_WriteMsg
 */
void NET_WriteConstMsg (struct net_stream* s, const dbuffer& buf)
{
	char tmp[256];
	const int len = LittleLong(buf.length());
	int pos = 0;
	NET_StreamEnqueue(s, (const char*)&len, 4);

	while (pos < buf.length()) {
		const int x = buf.getAt(pos, tmp, sizeof(tmp));
		assert(x > 0);
		NET_StreamEnqueue(s, tmp, x);
		pos += x;
	}
}

void NET_VPrintf (dbuffer* buf, const char* format, va_list ap, char* str, size_t length)
{
	const int len = Q_vsnprintf(str, length, format, ap);
	buf->add(str, len);
}
