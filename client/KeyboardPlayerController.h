#ifndef KEYBOARD_PLAYER_CONTROLLER_H_INCLUDED
#define KEYBOARD_PLAYER_CONTROLLER_H_INCLUDED

#include "PlayerController.h"

class KeyboardPlayerController : public PlayerController
{
public:
    KeyboardPlayerController(int key_left, int key_right);

    void restart(const GameParameters &gp);

    Move move( int timestamp, const Player *players,
               int player_index, const Field *field );

private:
    int key_left, key_right;
};

#endif /* ndef KEYBOARD_PLAYER_CONTROLLER_H_INCLUDED */
