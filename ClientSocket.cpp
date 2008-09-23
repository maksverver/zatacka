#include "ClientSocket.h"
#include "Debug.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef _MSC_VER
typedef size_t ssize_t;
#else
#include <unistd.h>
#endif

#ifndef WIN32
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define send(s,b,l,f) send(s,(char*)b,l,f)
#define recv(s,b,l,f) recv(s,(char*)b,l,f)
#define close(s) closesocket(s)
#endif

/* For debugging: simulated probability of packet loss (between 0 and 1)
                  (applies only to unreliable packet data) */
static double g_packetloss = 0;

static int ip_connect(sockaddr_in &sa_local, sockaddr_in &sa_remote, bool reliable)
{
    int s = socket(PF_INET, reliable ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (s < 0)
    {
        error("ip_connect(): socket() failed");
        return -1;
    }

    if (bind(s, (sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        error("ip_connect(): bind() failed");
        close(s);
        return -1;
    }

    if (connect(s, (sockaddr*)&sa_remote, sizeof(sa_remote)) != 0)
    {
        error("ip_connect(): connect() failed (%s)", reliable ? "TCP" : "UDP");
        close(s);
        return -1;
    }

    return s;
}

ClientSocket::ClientSocket(const char *hostname, int port)
    : fd_stream(-1), fd_packet(-1), stream_pos(0)
{
#ifdef WIN32
    static bool winsock_initialized = false;
    if (!winsock_initialized)
    {
	WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) 
        {
            fatal("WSAStartup failed!");
        }
	winsock_initialized = true;
    }
#endif


    info("Resolving host \"%s\"...", hostname);
    hostent *he = gethostbyname(hostname);
    if (!he)
    {
        error("Host not found!");
        return;
    }
    if (he->h_addrtype != AF_INET)
    {
        error("h_addrtype != AF_INET!");
        return;
    }

    sockaddr_in sa_remote;
    sa_remote.sin_family = AF_INET;
    sa_remote.sin_port   = htons(port);
    sa_remote.sin_addr   = *(in_addr*)he->h_addr;

    info("Connecting to IP address %d.%d.%d.%d port %d",
        ((unsigned char*)&sa_remote.sin_addr)[0],
        ((unsigned char*)&sa_remote.sin_addr)[1],
        ((unsigned char*)&sa_remote.sin_addr)[2],
        ((unsigned char*)&sa_remote.sin_addr)[3],
        port);

    sockaddr_in sa_local;
    sa_local.sin_family      = AF_INET;
    sa_local.sin_port        = 0;
    sa_local.sin_addr.s_addr = INADDR_ANY;

    fd_stream = ip_connect(sa_local, sa_remote, true);
    if (fd_stream < 0)
    {
        error("TCP connection failed");
    }
    else
    {
        socklen_t len = sizeof(sa_local);
        if (getsockname(fd_stream, (sockaddr*)&sa_local, &len) != 0)
        {
            error("getsockname() failed");
        }

        fd_packet = ip_connect(sa_local, sa_remote, false);
        if (fd_packet < 0)
        {
            error("UDP connection failed");
        }
    }

    if (fd_stream >= 0 && fd_packet >= 0) info("Connected! Local port: %d", ntohs(sa_local.sin_port));
}

ClientSocket::~ClientSocket()
{
    if (fd_stream >= 0)
    {
        close(fd_stream);
    }
    if (fd_packet >= 0)
    {
        close(fd_packet);
    }
}

bool ClientSocket::connected() const
{
    return fd_stream >= 0 && fd_packet >= 0;
}

void ClientSocket::write(void const *buf, size_t len, bool reliable)
{
    if (len > 4094)
    {
        error("ClientSocket::write(): packet too large");
        return;
    }

    if (reliable)
    {
        unsigned char packet[4096];
        packet[0] = len>>8;
        packet[1] = len&255;
        memcpy(packet + 2, buf, len);
        if (send(fd_stream, packet, len + 2, 0) != (ssize_t)len + 2)
        {
            error("reliable send() failed");
        }
    }
    else
    if (g_packetloss == 0 || rand() > RAND_MAX*g_packetloss)
    {

        if (send(fd_packet, buf, len, 0) != (ssize_t)len)
        {
            warn("unreliable send() failed");
        }
    }
}

ssize_t ClientSocket::read(void *buf, size_t buf_len)
{
    timeval tv = { 0, 0 };

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd_stream, &readfds);
    FD_SET(fd_packet, &readfds);

    /* nfds must be set to the highest numbered socket + 1 */
    int nfds = 1 + (fd_stream > fd_packet ? fd_stream : fd_packet);
    int res = select(nfds, &readfds, NULL, NULL, &tv);
    if (res < 0)
    {
        error("select() failed");
        return -1;
    }

    if (res == 0) return 0;

    if (FD_ISSET(fd_stream, &readfds))
    {
        ssize_t len = recv(fd_stream, stream_buf + stream_pos, sizeof(stream_buf) - stream_pos, 0);
        if (len < 0) return len;
        if (len > 0)
        {
            stream_pos += len;
            if (stream_pos >= 2)
            {
                size_t len = 256*stream_buf[0] + stream_buf[1];
                if (len > sizeof(stream_buf) - 2)
                {
                    error("ClientSocket::read(): packet too large");
                }
                else
                if (len < 1)
                {
                    error("ClientSocket::read(): packet too small");
                }
                else
                if (stream_pos >= len + 2)
                {
                    if (len > buf_len) fatal("ClientSocket::read(): buffer too small");
                    memcpy(buf, stream_buf + 2, len);
                    memmove(stream_buf, stream_buf + len + 2, stream_pos - (len + 2));
                    stream_pos -= len + 2;
                    return len;
                }
            }
        }
    }

    if (FD_ISSET(fd_packet, &readfds))
    {
        return recv(fd_packet, buf, buf_len, 0);
    }

    return 0;
}
