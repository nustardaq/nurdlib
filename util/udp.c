/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021-2022, 2024
 * Håkan T Johansson
 * Hans Toshihide Törnqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <nconf/util/udp.c>
#include <util/udp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <nurdlib/base.h>
#include <util/assert.h>
#include <util/memcpy.h>
#include <util/err.h>

#if NCONF_mSOCKET_H_bSOCKET_H
/* NCONF_NOLINK */
#	include <socket.h>
#elif NCONF_mSOCKET_H_bSYS_SOCKET_H
/* NCONF_NOLINK */
#	include <sys/socket.h>
#elif NCONF_mSOCKET_H_bNONE
#endif

#if NCONF_mIPPROTO_UDP_bNETINET_IN_H
/* NCONF_NOLINK */
#	include <netinet/in.h>
#elif NCONF_mIPPROTO_UDP_bNOWARN_NETINET_IN_H
/* NCONF_CPPFLAGS=-D__NO_INCLUDE_WARN__ */
/* NCONF_NOLINK */
#	include <netinet/in.h>
#endif
#if NCONFING_mIPPROTO_UDP
#	define NCONF_TEST IPPROTO_UDP
#endif

#if NCONF_mUDP_LOOKUP_bGETADDRINFO
/* NCONF_NOEXEC */
#	include <netdb.h>
#	if NCONFING_mUDP_LOOKUP
#		define NCONF_TEST 0 == getaddrinfo(NULL, NULL, NULL, NULL)
#	endif
#elif NCONF_mUDP_LOOKUP_bGETHOSTBYNAME_SOCKLEN
/* NCONF_NOEXEC */
#	include <arpa/inet.h>
#	include <netdb.h>
#	if NCONFING_mUDP_LOOKUP
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	socklen_t len;
	return NULL != gethostbyname(NULL) &&
	    -1 != recvfrom(0, NULL, 0, 0, NULL, &len);
}
#	endif
#endif

#if NCONF_mUDP_EVENT_bPOLL
/* NCONF_NOEXEC */
#	include <poll.h>
#	if NCONFING_mUDP_EVENT
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	struct pollfd fds[1];
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	return -1 != poll(fds, 1, 0);
}
#	endif
#elif NCONF_mUDP_EVENT_bSYS_SELECT_H
/* NCONF_NOEXEC */
#	include <sys/select.h>
#	if NCONFING_mUDP_EVENT
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	struct timeval tv = {0, 0};
	return -1 != select(0, NULL, NULL, NULL, &tv);
}
#	endif
#elif NCONF_mUDP_EVENT_bSELECT_TIME_H
/* NCONF_NOEXEC */
#	include <time.h>
#	if NCONFING_mUDP_EVENT
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	struct timeval tv = {0, 0};
	return -1 != select(0, NULL, NULL, NULL, &tv);
}
#	endif
#endif

#if !NCONFING

#if defined(NCONF_mUDP_LOOKUP_bGETADDRINFO)
#	define GETADDRINFO
#	include <fcntl.h>
#	include <time.h>
#	include <stdarg.h>
#	include <stdio.h>
#	include <unistd.h>
#	define INVALID_SOCKET -1
#	define NONBLOCK_FCNTL
#	define SOCKADDR_STORAGE sockaddr_storage
#	define SOCKET int
#	define SOCKET_ERROR -1
#else
#	define GETHOSTBYNAME
#	include <fcntl.h>
#	include <stdarg.h>
#	include <stdio.h>
#	include <unistd.h>
#	define INVALID_SOCKET -1
#	define NONBLOCK_FCNTL
#	define SOCKADDR_STORAGE sockaddr_in
#	define SOCKET int
#	define SOCKET_ERROR -1
#endif

#if defined(NCONF_mUDP_LOOKUP_bGETADDRINFO)
FUNC_PRINTF(2, 0) static void
gaif(int a_error, char const *a_fmt, ...)
{
	va_list args;

	va_start(args, a_fmt);
	vwarnx_(a_fmt, args);
	va_end(args);
	warnx_("%s", gai_strerror(a_error));
}
#endif

#ifdef NONBLOCK_FCNTL
FUNC_RETURNS static int
set_non_blocking(SOCKET a_socket)
{
	return 0 == fcntl(a_socket, F_SETFL, O_NONBLOCK);
}
#endif

/*
 * A few things can happen:
 *  1) Error -> warn(), return 0.
 *  2) Interruption/timeout -> return 0.
 *  3) Socket -> return 1;
 *  4) Extra -> return 2;
 */
static int
event_wait(SOCKET a_socket, int a_fd_extra, double a_timeout)
{
#if defined(NCONF_mUDP_EVENT_bPOLL)
	struct pollfd pfd[2];
	nfds_t nfds;
	int ret;

	pfd[0].fd = a_socket;
	pfd[0].events = POLLIN;
	nfds = 1;
	if (0 < a_fd_extra) {
		pfd[1].fd = a_fd_extra;
		pfd[1].events = POLLIN;
		nfds = 2;
	}
	ret = poll(pfd, nfds, (int)(a_timeout * 1e3));
	if (-1 == ret && EINTR != errno) {
		warn_("poll");
	}
	if (1 > ret) {
		return 0;
	}
	if (-1 != a_fd_extra && (POLLIN & pfd[1].revents)) {
		return 2;
	}
	return 1;
#elif defined(NCONF_mUDP_EVENT_bSYS_SELECT_H) || \
	defined(NCONF_mUDP_EVENT_bSELECT_TIME_H)
	fd_set socks;
	struct timeval timeout;
	int nfds, ret;

	FD_ZERO(&socks);
	FD_SET(a_socket, &socks);
	if (0 < a_fd_extra) {
		FD_SET(a_fd_extra, &socks);
	}
	nfds = MAX(a_socket, a_fd_extra);
	timeout.tv_sec = (long)a_timeout;
	timeout.tv_usec = (long)(1e6 * (a_timeout - timeout.tv_sec));
	ret = select(nfds + 1, &socks, NULL, NULL, &timeout);
	if (-1 == ret && EINTR != errno) {
		warn_("select");
	}
	if (1 > ret) {
		return 0;
	}
	if (-1 != a_fd_extra && FD_ISSET(a_fd_extra, &socks)) {
		return 2;
	}
	return 1;
#endif
}

#include <util/string.h>

struct UDPAddress {
	struct	SOCKADDR_STORAGE addr;
	socklen_t	len;
};
struct UDPClient {
	SOCKET	socket;
};
struct UDPServer {
	SOCKET	socket;
	int	pfd[2];
};

static int	get_family(unsigned) FUNC_RETURNS;
static int	receive_datagram(SOCKET, int, struct UDPDatagram *, struct
    UDPAddress **, double);

int
get_family(unsigned a_flags)
{
#if defined(AF_INET6)
	return UDP_IPV4 == ((UDP_IPV4 | UDP_IPV6) & a_flags) ? AF_INET :
	    AF_INET6;
#else
	if (UDP_IPV4 != a_flags) {
		warn_x("Platform only support IPv4, something else "
		    "requested.");
	}
	return AF_INET;
#endif
}

/*
 * 'a_dgram.bytes' says how big the buffer is, 0 means to take sizeof(buf).
 * Return:
 *  1) Error -> return 0.
 *  2) Timeout -> return 1, size = 0.
 *  3) Data -> return 1, size != 0.
 * "a_addr" is only allocated in case 3.
 */
int
receive_datagram(SOCKET a_socket, int a_fd_extra, struct UDPDatagram *a_dgram,
    struct UDPAddress **a_addr, double a_timeout)
{
	struct UDPAddress addr;
	size_t bytes;
	int ret;

	if (NULL != a_addr) {
		*a_addr = NULL;
	}
	if (0 == a_dgram->bytes) {
		bytes = sizeof a_dgram->buf;
	} else {
		bytes = a_dgram->bytes;
		a_dgram->bytes = 0;
	}
	ret = event_wait(a_socket, a_fd_extra, a_timeout);
	if (0 == ret) {
		/* We got nothing. */
		a_dgram->bytes = 0;
		return 1;
	}
	if (2 == ret) {
		ssize_t got;

		got = read(a_fd_extra, a_dgram->buf, bytes);
		if (-1 == got) {
			if (EAGAIN == errno || EWOULDBLOCK == errno) {
				return 1;
			}
			warn_("recvfrom");
			return 0;
		}
		a_dgram->bytes = got;
		return 1;
	}
	addr.len = sizeof addr.addr;
	ZERO(addr.addr);
	ret = recvfrom(a_socket, (char *)a_dgram->buf, bytes, 0, (struct
	    sockaddr *)&addr.addr, &addr.len);
	if (SOCKET_ERROR == ret) {
		if (EAGAIN == errno || EWOULDBLOCK == errno) {
			return 1;
		}
		warn_("recvfrom");
		return 0;
	}
	a_dgram->bytes = ret;
	if (0 != ret && NULL != a_addr) {
		struct UDPAddress *paddr;

		CALLOC(paddr, 1);
		COPY(*paddr, addr);
		*a_addr = paddr;
	}
	return 1;
}

struct UDPAddress *
udp_address_create(unsigned a_flags, char const *a_hostname, uint16_t a_port)
{
	struct UDPAddress *addr;

	MALLOC(addr, 1);
#if defined(GETADDRINFO)
	{
		struct addrinfo addri;
		char port_str[10];
		struct addrinfo *result;
		int ret;

		ZERO(addri);
		addri.ai_family = get_family(a_flags);
		addri.ai_socktype = SOCK_DGRAM;
		addri.ai_protocol = IPPROTO_UDP;
		snprintf_(port_str, sizeof port_str, "%d", a_port);
		ret = getaddrinfo(a_hostname, port_str, &addri, &result);
		if (0 != ret) {
			gaif(ret, "%s:%s", a_hostname, port_str);
			goto udp_address_create_fail;
		}
		memcpy_(&addr->addr, result->ai_addr, result->ai_addrlen);
		addr->len = result->ai_addrlen;
		freeaddrinfo(result);
	}
#else
	{
		struct hostent *host;
		struct sockaddr_in *in;
		char *tmp;

		if (UDP_IPV4 != a_flags) {
			fprintf(stderr, "Platform only supports IPv4, "
			    "something else requested.\n");
			goto udp_address_create_fail;
		}
		tmp = strdup_(a_hostname);
		host = gethostbyname(tmp);
		free(tmp);
		if (NULL == host) {
			fprintf(stderr, "gethostbyname(%s): %s.\n",
			    a_hostname, strerror(h_errno));
			goto udp_address_create_fail;
		}
		in = (void *)&addr->addr;
		memcpy_(&in->sin_addr.s_addr, host->h_addr_list[0],
		    host->h_length);
		in->sin_port = htons(a_port);
		addr->len = host->h_length;
	}
#endif
	return addr;
udp_address_create_fail:
			free(addr);
			return NULL;
}

void
udp_address_free(struct UDPAddress **a_address)
{
	FREE(*a_address);
}

char *
udp_address_gets(struct UDPAddress const *a_address)
{
	struct sockaddr_in const *addr;

	addr = (void const *)&a_address->addr;
	if (AF_INET == addr->sin_family) {
		return strdup_(inet_ntoa(addr->sin_addr));
#if defined(AF_INET6)
	} else if (AF_INET6 == addr->sin_family) {
		struct sockaddr_in6 const *addr6;
		char *str;

		addr6 = (void const *)&a_address->addr;
		str = malloc(INET6_ADDRSTRLEN + 1);
		inet_ntop(addr->sin_family, &addr6->sin6_addr, str,
		    INET6_ADDRSTRLEN);
		return str;
#endif
	} else {
		assert(0 && "Invalid address family.");
		return NULL;
	}
}

uint32_t
udp_address_getu32(struct UDPAddress const *a_address)
{
	struct sockaddr_in const *addr;

	addr = (void const *)&a_address->addr;
	if (AF_INET == addr->sin_family) {
		return ntohl(addr->sin_addr.s_addr);
#if defined(AF_INET6)
	} else if (AF_INET6 == addr->sin_family) {
		struct sockaddr_in6 const *addr6;

		addr6 = (void const *)&a_address->addr;
		return ntohl(*(uint32_t const *)&addr6->sin6_addr);
#endif
	} else {
		assert(0 && "Invalid address family.");
		return 0;
	}
}

struct UDPClient *
udp_client_create(unsigned a_flags, char const *a_hostname, uint16_t a_port)
{
	struct UDPClient *client;
	SOCKET sock;

#if defined(GETADDRINFO)
	{
		struct addrinfo addri;
		char port_str[10];
		struct addrinfo *result, *p;
		int ret;

		ZERO(addri);
		addri.ai_family = get_family(a_flags);
		addri.ai_socktype = SOCK_DGRAM;
		addri.ai_protocol = IPPROTO_UDP;
		snprintf_(port_str, sizeof port_str, "%d", a_port);
		ret = getaddrinfo(a_hostname, port_str, &addri, &result);
		if (0 != ret) {
			gaif(ret, "%s:%s", a_hostname, port_str);
			return NULL;
		}
		sock = INVALID_SOCKET;
		for (p = result; NULL != p; p = p->ai_next) {
			sock = socket(p->ai_family, p->ai_socktype,
			    p->ai_protocol);
			if (INVALID_SOCKET == sock) {
				warn_("socket(%s:%s)", a_hostname,
				    port_str);
				continue;
			}
			if (0 != connect(sock, p->ai_addr, p->ai_addrlen)) {
				warn_("connect(%s:%s)", a_hostname,
				    port_str);
				close(sock);
				sock = INVALID_SOCKET;
				continue;
			}
			break;
		}
		freeaddrinfo(result);
		if (INVALID_SOCKET == sock) {
			return NULL;
		}
	}
#else
	{
		struct SOCKADDR_STORAGE server;
		struct hostent *host;
		char *h;

		/* Not const?! Wtf... */
		h = strdup_(a_hostname);
		host = gethostbyname(h);
		free(h);
		if (NULL == host) {
			fprintf(stderr, "gethostbyname(%s): %s.\n",
			    a_hostname, strerror(h_errno));
			return NULL;
		}
		sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (0 > sock) {
			fprintf(stderr, "socket: %s.\n", strerror(errno));
			return NULL;
		}
		server.sin_family = get_family(a_flags);
		memcpy_(&server.sin_addr.s_addr, host->h_addr_list[0],
		    host->h_length);
		server.sin_port = htons(a_port);
		if (0 != connect(sock, (void *)&server, sizeof server)) {
			fprintf(stderr, "connect: %s.\n", strerror(errno));
			close(sock);
			return NULL;
		}
	}
#endif
	if (!set_non_blocking(sock)) {
		warn_("set_non_blocking(%s:%d)", a_hostname, a_port);
		close(sock);
		return NULL;
	}
	CALLOC(client, 1);
	client->socket = sock;
	return client;
}

void
udp_client_free(struct UDPClient **a_client)
{
	struct UDPClient *client;

	client = *a_client;
	if (NULL == client) {
		return;
	}
	close(client->socket);
	FREE(*a_client);
}

int
udp_client_receive(struct UDPClient const *a_client, struct UDPDatagram
    *a_dgram, double a_timeout)
{
	return receive_datagram(a_client->socket, -1, a_dgram, NULL,
	    a_timeout);
}

int
udp_client_send(struct UDPClient const *a_client, struct UDPDatagram const
    *a_dgram)
{
	if (SOCKET_ERROR == send(a_client->socket, (void const *)a_dgram->buf,
	    a_dgram->bytes, 0)) {
		warn_("send");
		return 0;
	}
	return 1;
}

struct UDPServer *
udp_server_create(unsigned a_flags, uint16_t a_port)
{
	struct UDPServer *server;
	int has_pfd, sock;

	has_pfd = 0;
	sock = INVALID_SOCKET;
	CALLOC(server, 1);
	if (-1 == pipe(server->pfd)) {
		warn_("pipe");
		goto udp_server_create_fail;
	}
	has_pfd = 1;
	if (!set_non_blocking(server->pfd[0])) {
		warn_("fcntl_get(pfd[0])");
		goto udp_server_create_fail;
	}
	if (!set_non_blocking(server->pfd[1])) {
		warn_("fcntl_get(pfd[1])");
		goto udp_server_create_fail;
	}

#if defined(GETADDRINFO)
	{
		struct addrinfo addri;
		char port_str[10];
		struct addrinfo *result, *p;

		snprintf_(port_str, sizeof port_str, "%d", a_port);
		ZERO(addri);
		addri.ai_family = get_family(a_flags);
		addri.ai_socktype = SOCK_DGRAM;
		addri.ai_flags = AI_PASSIVE;
		addri.ai_protocol = IPPROTO_UDP;
		if (0 != getaddrinfo(NULL, port_str, &addri, &result)) {
			/* TODO Must use gai_strerror! */
			warn_("NULL:%s", port_str);
			goto udp_server_create_fail;
		}
		for (p = result; NULL != p; p = p->ai_next) {
			sock = socket(p->ai_family, p->ai_socktype,
			    p->ai_protocol);
			if (INVALID_SOCKET == sock) {
				warn_("socket(NULL:%s)", port_str);
				continue;
			}
			if (0 != bind(sock, p->ai_addr, p->ai_addrlen)) {
				warn_("connect(NULL:%s)", port_str);
				close(sock);
				sock = INVALID_SOCKET;
				continue;
			}
			break;
		}
		freeaddrinfo(result);
		if (INVALID_SOCKET == sock) {
			warnx_("Could not create socket.");
			goto udp_server_create_fail;
		}
	}
#else
	{
		struct SOCKADDR_STORAGE me;

		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (0 > sock) {
			fprintf(stderr, "socket: %s.", strerror(errno));
			goto udp_server_create_fail;
		}
		ZERO(me);
		me.sin_family = get_family(a_flags);
		me.sin_addr.s_addr = htonl(INADDR_ANY);
		me.sin_port = htons(a_port);
		if (0 != bind(sock, (void *)&me, sizeof me)) {
			fprintf(stderr, "bind: %s.", strerror(errno));
			goto udp_server_create_fail;
		}
	}
#endif
	if (!set_non_blocking(sock)) {
		warn_("set_non_blocking(NULL:%d)", a_port);
		goto udp_server_create_fail;
	}
	server->socket = sock;
	return server;

udp_server_create_fail:
	if (INVALID_SOCKET != sock) {
		close(sock);
	}
	if (has_pfd) {
		close(server->pfd[0]);
		close(server->pfd[1]);
	}
	FREE(server);
	return NULL;
}

void
udp_server_free(struct UDPServer **a_server)
{
	struct UDPServer *server;

	server = *a_server;
	if (NULL == server) {
		return;
	}
	close(server->socket);
	close(server->pfd[0]);
	close(server->pfd[1]);
	FREE(*a_server);
}

int
udp_server_receive(struct UDPServer const *a_server, struct UDPAddress
    **a_addr, struct UDPDatagram *a_dgram, double a_timeout)
{
	return receive_datagram(a_server->socket, a_server->pfd[0], a_dgram,
	    a_addr, a_timeout);
}

int
udp_server_send(struct UDPServer const *a_server, struct UDPAddress const
    *a_addr, struct UDPDatagram const *a_dgram)
{
	if (SOCKET_ERROR == sendto(a_server->socket,
	    (void const *)a_dgram->buf, a_dgram->bytes, 0,
	    (void const *)&a_addr->addr, a_addr->len)) {
		warn_("send");
		return 0;
	}
	return 1;
}

int
udp_server_write(struct UDPServer const *a_server, void const *a_data, size_t
    a_data_len)
{
	if (-1 == write(a_server->pfd[1], a_data, a_data_len)) {
		warn_("write(pfd[1])");
		return 0;
	}
	return 1;
}

#endif
