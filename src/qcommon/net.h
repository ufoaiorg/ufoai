/**
 * @file net.h
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

#ifndef QCOMMON_NET_H
#define QCOMMON_NET_H

struct net_stream;
typedef void stream_callback_func(struct net_stream *s);

qboolean start_server(const char *node, const char *service, stream_callback_func *func);
void stop_server(void);

void init_net(void);
void wait_for_net(int timeout);
struct net_stream *connect_to_host(const char *node, const char *service);
struct net_stream *connect_to_loopback(void);
void stream_enqueue(struct net_stream *s, const char *data, int len);
qboolean stream_closed(struct net_stream *s);
int stream_length(struct net_stream *s);
int stream_peek(struct net_stream *s, char *data, int len);
int stream_dequeue(struct net_stream *s, char *data, int len);
void *stream_data(struct net_stream *s);
void set_stream_data(struct net_stream *s, void *data);
const char *stream_peer_name(struct net_stream *s, char *dst, int len);
qboolean stream_is_loopback(struct net_stream *s);

/* Call free_stream to dump the whole thing right now */
void free_stream(struct net_stream *s);

/* Call stream_finished to mark the stream as uninteresting, but to
   finish sending any data in the buffer. The stream will appear
   closed after this call, and at some unspecified point in the future
   s will become an invalid pointer, so it should not be further
   referenced.
 */
void stream_finished(struct net_stream *s);

void stream_callback(struct net_stream *s, stream_callback_func *func);

#endif
