#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#define PROTOCOL_VERSION (1)

typedef enum Message
{
    /* reliable client->server */
    MRCS_HELO =   0,
    MRCS_QUIT =   1,
    MRCS_CHAT =   2,

    /* reliable server->client */
    MRSC_MESG =  64,
    MRSC_DISC =  65,
    MRSC_STRT =  66,
    MRSC_SCOR =  67,

    /* unreliable client->server */
    MUCS_MOVE = 128,

    /* unreliable server->client */
    MUSC_MOVE = 192
} Message;

/* Move commands.
   MOVE_NONE and MOVE_DEAD are only sent by the server.*/
typedef enum Move
{
    MOVE_NONE       = 0,
    MOVE_FORWARD    = 1,
    MOVE_TURN_LEFT  = 2,
    MOVE_TURN_RIGHT = 3,
    MOVE_DEAD       = 4
} Move;

#endif /* ndef PROTOCOL_H_INCLUDED */
