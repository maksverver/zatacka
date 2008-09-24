#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#define PROTOCOL_VERSION (1)

/* reliable client->server */
#define MRCS_HELO      (0)
#define MRCS_QUIT      (1)

/* reliable server->client */
#define MRSC_MESG     (64)
#define MRSC_DISC     (65)
#define MRSC_STRT     (66)
#define MRSC_SCOR     (67)

/* unreliable client->server */
#define MUCS_MOVE    (128)

/* unreliable server->client */
#define MUSC_MOVE    (192)

#endif /* ndef PROTOCOL_H_INCLUDED */
