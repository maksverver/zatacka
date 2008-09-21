#include "Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

const int max_packet_len = 4096;

struct Client
{
    /* Set to true only if this client is currently in use */
    bool            in_use;

    /* Remote address (TCP only) */
    sockaddr_in     sa_remote;

    /* Streaming data */
    int             fd_stream;
    unsigned char   buf[max_packet_len];
    size_t          buf_pos;
};

/* Globals */
static const int max_clients = 64;
static int g_fd_listen; /* Stream data listening socket */
static int g_fd_packet; /* Packet data socket */
static Client clients[max_clients];

static void handle_packet( int c, unsigned char *buf, size_t len,
                           bool reliable )
{
    info( "%s packet of length %d received from client #%d",
          reliable ? "reliable" : "unreliable", len, c );
}

static void disconnect(int c, const char *reason)
{
    info( "disconnecting client %d at %s:%d (reason: %s)",
          c, inet_ntoa(clients[c].sa_remote.sin_addr),
          ntohs(clients[c].sa_remote.sin_port), reason );
    close(clients[c].fd_stream);
    clients[c].in_use = false;
}

static int run()
{
    for (;;)
    {
        /* Prepare file selectors to select on */
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_fd_listen, &readfds);
        FD_SET(g_fd_packet, &readfds);
        int max_fd = g_fd_listen > g_fd_packet ? g_fd_listen : g_fd_packet;
        for (int n = 0; n < max_clients; ++n)
        {
            if (!clients[n].in_use) continue;
            FD_SET(clients[n].fd_stream, &readfds);
            if (clients[n].fd_stream > max_fd) max_fd = clients[n].fd_stream;
        }

        /* Select readable file descriptors (wait infinitely) */
        int ready = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ready == 0)
        {
            fatal("select() time-out; shouldn't happen");
        }
        if (ready < 0)
        {
            fatal("select() failed");
        }

        /* Accept new connections */
        if (FD_ISSET(g_fd_listen, &readfds))
        {
            sockaddr_in sa;
            socklen_t sa_len = sizeof(sa);
            int fd = accept(g_fd_listen, (sockaddr*)&sa, &sa_len);
            if (fd < 0)
            {
                error("accept() failed");
            }
            else
            if (sa_len != sizeof(sa) || sa.sin_family != AF_INET)
            {
                error("accepted connection from unsupported remote address");
                close(fd);
            }
            else
            {
                int n = 0;
                while (n < max_clients && clients[n].in_use) ++n;
                if (n == max_clients)
                {
                    warn( "No client slot free; rejecting connection from %s:%d",
                          inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
                    close(fd);
                }
                else
                {
                    /* Initialize new client slot */
                    memset(&clients[n], 0, sizeof(clients[n]));
                    clients[n].in_use    = true;
                    clients[n].sa_remote = sa;
                    clients[n].fd_stream = fd;
                    info( "Accepted client from %s:%d in slot #%d",
                          inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), n );
                }
            }
        }

        /* Accept incoming packets */
        if (FD_ISSET(g_fd_packet, &readfds))
        {
            sockaddr_in sa;
            socklen_t sa_len = sizeof(sa);
            unsigned char buf[max_packet_len - 2];
            ssize_t buf_len;

            buf_len = recvfrom( g_fd_packet, buf, sizeof(buf), 0,
                                (sockaddr*)&sa, &sa_len );
            if (buf_len < 0)
            {
                error("recvfrom() failed");
            }
            else
            if (sa_len != sizeof(sa) || sa.sin_family != AF_INET)
            {
                error("received packet from unsupported remote address");
            }
            else
            if (buf_len < 1)
            {
                error( "received invalid packet from %s:%d",
                       inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
            }
            else
            {
                int c = -1;
                for (int n = 0; n < max_clients; ++n)
                {
                    if ( clients[n].in_use &&
                         clients[n].sa_remote.sin_addr.s_addr ==
                            sa.sin_addr.s_addr &&
                         clients[n].sa_remote.sin_port == sa.sin_port )
                    {
                        c = n;
                        break;
                    }
                }
                if (c == -1)
                {
                    warn ( "packet from %s:%d ignored (not registered)",
                        inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
                }
                else
                {
                    handle_packet(c, buf, buf_len, false);
                }
            }
        }

        /* Accept incoming stream packets */
        for (int n = 0; n < max_clients; ++n)
        {
            if (!clients[n].in_use) continue;
            if (!FD_ISSET(clients[n].fd_stream, &readfds)) continue;

            ssize_t read = recv(
                clients[n].fd_stream, clients[n].buf + clients[n].buf_pos,
                sizeof(clients[n].buf) - clients[n].buf_pos, 0 );
            if (read <= 0)
            {
                disconnect(n, read == 0 ? "EOF reached" : "recv() failed");
            }
            else
            {
                clients[n].buf_pos += read;
                if (clients[n].buf_pos >= 2)
                {
                    size_t len = 256*clients[n].buf[0] + clients[n].buf[1];
                    if (len > max_packet_len - 2)
                    {
                        disconnect(n, "packet too large");
                    }
                    if (clients[n].buf_pos >= len)
                    {
                        handle_packet(n, clients[n].buf, len, true);
                        memmove( clients[n].buf, clients[n].buf + len,
                                 clients[n].buf_pos - len );
                        clients[n].buf_pos -= len;
                    }
                }
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    time_reset();

    (void)argc;
    (void)argv;

    sockaddr_in sa_local;
    sa_local.sin_family = AF_INET;
    sa_local.sin_port   = htons(12321);
    sa_local.sin_addr.s_addr = INADDR_ANY;

    /* Create TCP listening socket */
    g_fd_listen = socket(PF_INET, SOCK_STREAM, 0);
    if (g_fd_listen < 0)
    {
        fatal("Could not create TCP socket: socket() failed");
    }
    if (bind(g_fd_listen, (sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        fatal("Could not create TCP socket: bind() failed");
    }
    if (listen(g_fd_listen, 4) != 0)
    {
        fatal("Could not create TCP socket: listen() failed");
    }

    /* Create UDP datagram socket */
    g_fd_packet = socket(PF_INET, SOCK_DGRAM, 0);
    if (g_fd_packet < 0)
    {
        fatal("Could not create UDP socket: socket() failed");
    }
    if (bind(g_fd_packet, (sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        fatal("Could not create UDP socket: bind() failed");
    }

    /* Main loop */
    return run();
}
