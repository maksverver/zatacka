#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#define PROTOCOL_VERSION (2)

typedef enum Message
{
    /* reliable client->server */
    MRCS_FEAT =   0,
    MRCS_QUIT =   1,
    MRCS_JOIN =   2,
    MRCS_CHAT =   3,
    MRCS_STRT =   4,
    MRCS_MOVE =   5,

    /* reliable server->client */
    MRSC_FEAT =  64,
    MRSC_QUIT =  65,
    MRSC_CHAT =  66,
    MRSC_STRT =  67,
    MRSC_MOVE =  68,
    MRSC_SCOR =  69,
    MRSC_FFWD =  70

    /* unreliable client->server */
    /* reserved range 128-191 */

    /* unreliable server->client */
    /* reserved range 192-255 */
} Message;

/* Player movement commands. */
typedef enum Move
{
    MOVE_FORWARD    = 0,
    MOVE_TURN_LEFT  = 1,
    MOVE_TURN_RIGHT = 2,
    MOVE_DEAD       = 3
} Move;

/* Protocol features. Set in FEAT message. */
#define FEAT_NONE           (0)
#define FEAT_NODELAY        (1)
#define FEAT_BOTS           (2)
#define FEAT_UNRELIABLE     (4)
#define FEAT_ALL            (7)

/* Player flags. Set in HELO message. */
#define PLFL_NONE   (0)
#define PLFL_BOT    (1)
#define PLFL_ALL    (1)

#endif /* ndef PROTOCOL_H_INCLUDED */
