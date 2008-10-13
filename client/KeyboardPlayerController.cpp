#include "KeyboardPlayerController.h"
#include "common.h"

KeyboardPlayerController::KeyboardPlayerController(int key_left, int key_right)
    : key_left(key_left), key_right(key_right)
{
}

void KeyboardPlayerController::restart(const GameParameters &gp)
{
    (void)gp;
}

Move KeyboardPlayerController::move( int timestamp, const Player *players,
               int player_index, const Field *field )
{
    (void)timestamp;
    (void)players;
    (void)player_index;
    (void)field;

    /* FIXME: mouse buttons don't work this way on X */
    bool left  = Fl::event_key(key_left);
    bool right = Fl::event_key(key_right);

    if (left && !right) return MOVE_TURN_LEFT;
    if (right && !left) return MOVE_TURN_RIGHT;
    return MOVE_FORWARD;
}
