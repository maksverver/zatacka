#include "ClientSocket.h"
#include "Debug.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#include <stdio.h>
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
    : fd_stream(-1), fd_packet(-1)
{
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
    fd_packet = ip_connect(sa_local, sa_remote, false);

    if (fd_stream < 0) error("TCP connection failed");
    if (fd_packet < 0) error("UDP connection failed");
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
    if (send(reliable ? fd_stream : fd_packet, buf, len, 0) != (ssize_t)len)
    {
        error("%s send() failed", reliable ? "reliable" : "unreliable");
    }
}

ssize_t ClientSocket::read(void *buf, size_t len)
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

    int s = FD_ISSET(fd_stream, &readfds) ? fd_stream : fd_packet;
    printf("%d %d\n", FD_ISSET(fd_stream, &readfds), FD_ISSET(fd_packet, &readfds));
    return recv(s, buf, len, 0);
}
