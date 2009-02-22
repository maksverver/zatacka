#ifndef CLIENT_SOCKET_H_INCLUDED
#define CLIENT_SOCKET_H_INCLUDED

#include <stdlib.h>

#ifdef _MSC_VER
typedef int ssize_t;
#else
#include <unistd.h>
#endif

struct sockaddr_in;

class ClientSocket
{
public:
    ClientSocket(const char *host, int port, bool reliable_only = false);
    ~ClientSocket();
    bool connected() const;
    bool reliable_only() const { return fd_packet < 0; }

    /* Streaming (reliable) messaging */
    void write(void const *buf, size_t len, bool reliable);
    ssize_t read(void *buf, size_t len);

protected:
    /* reads the next available packet from the stream buffer (if any) */
    ssize_t next_stream_packet(void *buf, size_t buf_len);

private:
    int fd_stream;
    int fd_packet;

    unsigned char stream_buf[4096];
    size_t stream_pos;
};

#endif /* ndef CLIENT_SOCKET_H_INCLUDED */
