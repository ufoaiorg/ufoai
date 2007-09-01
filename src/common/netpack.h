#ifndef QCOMMON_NETPACK_H
#define QCOMMON_NETPACK_H

#include "dbuffer.h"

/***
 ____ _____ ___  ____  
/ ___|_   _/ _ \|  _ \ 
\___ \ | || | | | |_) |
 ___) || || |_| |  __/ 
|____/ |_| \___/|_|    
                       
This is a temporary hack until the network protocol can be
rewritten. It will go away. Do not expect it to be here next time you
look.

This is not the right way to do things. Do not emulate it.

***/

void NET_WriteChar(struct dbuffer *buf, char c);

void NET_WriteByte(struct dbuffer *buf, unsigned char c);
void NET_WriteShort(struct dbuffer *buf, int c);
void NET_WriteLong(struct dbuffer *buf, int c);
void NET_WriteString(struct dbuffer *buf, const char *str);
void NET_WriteRawString(struct dbuffer *buf, const char *str);
void NET_WriteCoord(struct dbuffer *buf, float f);
void NET_WritePos(struct dbuffer *buf, vec3_t pos);
void NET_Write2Pos(struct dbuffer *buf, vec2_t pos);
void NET_WriteGPos(struct dbuffer *buf, pos3_t pos);
void NET_WriteAngle(struct dbuffer *buf, float f);
void NET_WriteAngle16(struct dbuffer *buf, float f);
void NET_WriteDir(struct dbuffer *buf, vec3_t vector);
void NET_V_WriteFormat(struct dbuffer *buf, const char *format, va_list ap);
void NET_WriteFormat(struct dbuffer *buf, const char *format, ...);
void NET_VPrintf(struct dbuffer *buf, const char *format, va_list ap);
void NET_Printf(struct dbuffer *buf, const char *format, ...) __attribute__((format(printf,2,3)));

/* This frees the buf argument */
void NET_WriteMsg(struct net_stream *s, struct dbuffer *buf);

/* This keeps buf intact */
void NET_WriteConstMsg(struct net_stream *s, const struct dbuffer *buf);

void NET_OOB_Printf(struct net_stream *s, const char *format, ...) __attribute__((format(printf,2,3)));

int NET_ReadChar(struct dbuffer *buf);
int NET_ReadByte(struct dbuffer *buf);
int NET_ReadShort(struct dbuffer *buf);
int NET_ReadLong(struct dbuffer *buf);
char *NET_ReadString(struct dbuffer *buf);
char *NET_ReadStringLine(struct dbuffer *buf);
char *NET_ReadStringRaw(struct dbuffer *buf);

float NET_ReadCoord(struct dbuffer *buf);
void NET_ReadPos(struct dbuffer *buf, vec3_t pos);
void NET_Read2Pos(struct dbuffer *buf, vec2_t pos);
void NET_ReadGPos(struct dbuffer *buf, pos3_t pos);
float NET_ReadAngle(struct dbuffer *buf);
float NET_ReadAngle16(struct dbuffer *buf);
void NET_ReadDir(struct dbuffer *buf, vec3_t vector);

void NET_ReadData(struct dbuffer *buf, void *buffer, int size);
void NET_V_ReadFormat(struct dbuffer *buf, const char *format, va_list ap);
void NET_ReadFormat(struct dbuffer *buf, const char *format, ...);

struct dbuffer *NET_ReadMsg(struct net_stream *s);

#endif
