#ifndef SEMI_RANDOM_H_INCLUDED
#define SEMI_RANDOM_H_INCLUDED

#include <client/PlayerController.h>

class SimpleSearch : public PlayerController
{
public:
    SimpleSearch();

    void restart(const GameParameters &gp);

    Move move( int timestamp, const Player *players,
               int player_index, const Field &field );

protected:
    int search(Position pos, int depth, Move *move_out);

protected:
    const Field *field;
    GameParameters gp;
    Move moves[3];
};

#endif /* ndef SEMI_RANDOM_H_INCLUDED */
