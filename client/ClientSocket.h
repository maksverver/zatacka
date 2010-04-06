#ifndef CLIENT_SOCKET_H_INCLUDED
#define CLIENT_SOCKET_H_INCLUDED

#include <stdlib.h>

#ifdef _MSC_VER
typedef int ssize_t;
#else
#include <unistd.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
const SOCKET INVALID_SOCKET = -1;
#endif

struct sockaddr_in;
 
class ClientSocket
{
public:
    ClientSocket(const char *host, int port, bool reliable_only = false);
    ~ClientSocket();
    bool connected() const;
    bool reliable_only() const { return fd_packet == INVALID_SOCKET; }

    /* Streaming (reliable) messaging */
    void write(void const *buf, size_t len, bool reliable);
    ssize_t read(void *buf, size_t len);

    /* Functions to collect network traffic statistics: */
    void clear_stats();
    size_t get_bytes_sent() { return bytes_sent; }
    size_t get_bytes_received() { return bytes_received; }
    size_t get_packets_sent() { return packets_sent; }
    size_t get_packets_received() { return packets_received; }

    /* Enables/disables nodelay mode (disables/enables Nagle's algorithm) */
    bool set_nodelay(bool nodelay);

protected:
    /* reads the next available packet from the stream buffer (if any) */
    ssize_t next_stream_packet(void *buf, size_t buf_len);

private:
    SOCKET fd_stream;
    SOCKET fd_packet;

    unsigned char stream_buf[4096];
    size_t stream_pos;

    size_t bytes_sent, bytes_received, packets_sent, packets_received;
};

#endif /* ndef CLIENT_SOCKET_H_INCLUDED */
