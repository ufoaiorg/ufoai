#ifndef IRC_NET_H
#define IRC_NET_H

#ifdef _WIN32
#	include <winsock.h>
	typedef SOCKET irc_socket_t;
#else
	typedef int irc_socket_t;
#endif

qboolean Irc_Net_Connect(const char *host, unsigned short port);
qboolean Irc_Net_Disconnect(void);

qboolean Irc_Net_Send(const char *msg, size_t msg_len);
qboolean Irc_Net_Receive(char *buf, size_t buf_len, int *recvd);

#endif
