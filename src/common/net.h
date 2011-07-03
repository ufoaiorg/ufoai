/**
 * @file net.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef _COMMON_NET_H
#define _COMMON_NET_H

struct net_stream;
struct datagram_socket;
struct sockaddr;
typedef void stream_onclose_func();
typedef void stream_callback_func(struct net_stream *s);
typedef void datagram_callback_func(struct datagram_socket *s, const char *buf, int len, struct sockaddr *from);

qboolean SV_Start(const char *node, const char *service, stream_callback_func *func);
void SV_Stop(void);

struct datagram_socket *NET_DatagramSocketNew(const char *node, const char *service, datagram_callback_func *datagram_func);
void NET_DatagramSend(struct datagram_socket *s, const char *buf, int len, struct sockaddr *to);
void NET_DatagramBroadcast(struct datagram_socket *s, const char *buf, int len, int port);
void NET_DatagramSocketClose(struct datagram_socket *s);
void NET_SockaddrToStrings(struct datagram_socket *s, struct sockaddr *addr, char *node, size_t nodelen, char *service, size_t servicelen);
void NET_ResolvNode(const char *node, char *buf, size_t bufLength);

void NET_Init(void);
void NET_Shutdown(void);
void NET_Wait(int timeout);
struct net_stream *NET_Connect(const char *node, const char *service, stream_onclose_func *onclose);
struct net_stream *NET_ConnectToLoopBack(stream_onclose_func *onclose);
void NET_StreamEnqueue(struct net_stream *s, const char *data, int len);
qboolean NET_StreamIsClosed(struct net_stream *s);
int NET_StreamGetLength(struct net_stream *s);
int NET_StreamPeek(struct net_stream *s, char *data, int len);
int NET_StreamDequeue(struct net_stream *s, char *data, int len);
void *NET_StreamGetData(struct net_stream *s);
void NET_StreamSetData(struct net_stream *s, void *data);
const char *NET_StreamPeerToName(struct net_stream *s, char *dst, int len, qboolean appendPort);
qboolean NET_StreamIsLoopback(struct net_stream *s);
void NET_StreamFree(struct net_stream *s);
void NET_StreamFinished(struct net_stream *s);
void NET_StreamSetCallback(struct net_stream *s, stream_callback_func *func);

#endif
