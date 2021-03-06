#include "ClientSocket.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef _MSC_VER
typedef int ssize_t;
#else
#include <unistd.h>
#endif

#ifndef WIN32
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/tcp.h>
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
static double g_packetloss;

static int ip_connect(sockaddr_in &sa_local, sockaddr_in &sa_remote, bool reliable)
{
    SOCKET s = socket(PF_INET, reliable ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET)
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

ClientSocket::ClientSocket(const char *hostname, int port, bool reliable_only)
    : fd_stream(INVALID_SOCKET), fd_packet(INVALID_SOCKET), stream_pos(0)
{
    clear_stats();

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
    if (fd_stream == INVALID_SOCKET)
    {
        error("TCP connection failed");
        return;
    }

    if (!reliable_only)
    {
        socklen_t len = sizeof(sa_local);
        if (getsockname(fd_stream, (sockaddr*)&sa_local, &len) != 0)
        {
            error("getsockname() failed");
            return;
        }

        fd_packet = ip_connect(sa_local, sa_remote, false);
        if (fd_packet == INVALID_SOCKET)
        {
            error("UDP connection failed");
            return;
        }
    }

    info( "Connected! Local port: %d (%s)", ntohs(sa_local.sin_port),
          reliable_only ? "TCP only" : "TCP+UDP" );
}

ClientSocket::~ClientSocket()
{
    if (fd_stream != INVALID_SOCKET)
    {
        close(fd_stream);
    }
    if (fd_packet != INVALID_SOCKET)
    {
        close(fd_packet);
    }
}

bool ClientSocket::connected() const
{
    return fd_stream != INVALID_SOCKET;
}

void ClientSocket::write(void const *buf, size_t len, bool reliable)
{
    ++packets_sent;
    bytes_sent += len;

    if (len > 4094)
    {
        error("ClientSocket::write(): packet too large");
        return;
    }

    if (reliable || fd_packet == INVALID_SOCKET)
    {
        unsigned char packet[4096];
        packet[0] = len>>8;
        packet[1] = len&255;
        memcpy(packet + 2, buf, len);
        if (send(fd_stream, packet, len + 2, 0) != (ssize_t)len + 2)
        {
            warn("reliable send() failed");
        }
    }
    else
    {
        /* Randomly drop outgoing packet */
        if (g_packetloss != 0 && rand() < RAND_MAX*g_packetloss) return;

        if (send(fd_packet, buf, len, 0) != (ssize_t)len)
        {
            warn("unreliable send() failed");
        }
    }
}

ssize_t ClientSocket::next_stream_packet(void *buf, size_t buf_len)
{
    if (stream_pos < 2) return 0;

    size_t len = 256*stream_buf[0] + stream_buf[1];
    if (len > sizeof(stream_buf) - 2)
    {
        error("ClientSocket::read(): packet too large");
        return -1;
    }

    if (len < 1)
    {
        error("ClientSocket::read(): packet too small");
        return -1;
    }

    if (stream_pos < len + 2)
    {
        /* Packet not yet complete. */
        return 0;
    }

    /* Decode packet */
    if (len > buf_len) fatal("ClientSocket::read(): buffer too small");
    memcpy(buf, stream_buf + 2, len);
    memmove(stream_buf, stream_buf + len + 2, stream_pos - (len + 2));
    stream_pos -= len + 2;
    return len;
}

ssize_t ClientSocket::read(void *buf, size_t buf_len)
{
    /* First, check if we have a buffered packet available */
    {
        ssize_t res = next_stream_packet(buf, buf_len);
        if (res != 0) return res;
    }

    /* Try to read a packet from a socket */
    timeval tv = { 0, 0 };

    fd_set readfds;
    FD_ZERO(&readfds);
    int nfds = 0;  /* must be set to the highest numbered socket + 1 */
    if (fd_stream != INVALID_SOCKET)
    {
        FD_SET(fd_stream, &readfds);
        if ((int)fd_stream >= nfds) nfds = fd_stream + 1;
    }
    if (fd_packet != INVALID_SOCKET)
    {
        FD_SET(fd_packet, &readfds);
        if ((int)fd_packet >= nfds) nfds = fd_packet + 1;
    }
    int ready = select(nfds, &readfds, NULL, NULL, &tv);
    if (ready < 0)
    {
        error("select() failed");
        return -1;
    }
    if (ready == 0) return 0;

    if (fd_stream != INVALID_SOCKET && FD_ISSET(fd_stream, &readfds))
    {
        ssize_t len = recv(fd_stream, stream_buf + stream_pos,
                                      sizeof(stream_buf) - stream_pos, 0);
        if (len < 0)
        {
            error("recv() failed  %d", stream_pos);
            return -1;
        }
        stream_pos += len;
        ssize_t res = next_stream_packet(buf, buf_len);
        if (res != 0)
        {
            ++packets_received;
            bytes_received += res;
            return res;
        }
    }

    if (fd_packet != INVALID_SOCKET && FD_ISSET(fd_packet, &readfds))
    {
        ssize_t res = recv(fd_packet, buf, buf_len, 0);

        /* Randomly drop incoming packet */
        if (g_packetloss != 0 && rand() < RAND_MAX*g_packetloss) res = 0;

        if (res != 0)
        {
            ++packets_received;
            bytes_received += res;
            return res;
        }
    }

    return 0;
}

void ClientSocket::clear_stats()
{
    bytes_sent       = 0;
    bytes_received   = 0;
    packets_sent     = 0;
    packets_received = 0;
}

bool ClientSocket::set_nodelay(bool nodelay)
{
#ifdef WIN32
    BOOL v = nodelay ? TRUE : FALSE;
#else
    int v = nodelay ? 1 : 0;
#endif
    return setsockopt( fd_stream, IPPROTO_TCP, TCP_NODELAY,
                       (char*)&v, sizeof(v) ) == 0;
}
