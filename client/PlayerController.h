#ifndef PLAYER_CONTROLLER_H_INCLUDED
#define PLAYER_CONTROLLER_H_INCLUDED

#include <string>
#include <deque>
#include <common/Field.h>
#include "GameModel.h"

class PlayerController
{
public:
    PlayerController(bool human = false) : m_human(human) { };
    virtual ~PlayerController() { };

    /* Called before the start of each game. */
    virtual void restart(const GameParameters &gp) = 0;

    /* Determine a valid move and return it.

       `timestamp' is the timestamp for the move (starting from 0); values
       below gp.warmup are in the warmup period, during which the player cannot
       move forward (but must return MOVE_FORWARD if he decides not to turn).

       `players' is the list of players (an array of length gp.num_players).

       `player_index' is the index of the controlled player into `players'.

       `field' is the current game field; the player with index i is represented
       by value (i + 1), since zeroes indicate free space.

       `messages` is a list of chat messages received; first element is the
       client name (empty for server messages), second is the message string.

       Implementation should return a valid move (not MOVE_DEAD), and may call
       say() to broadcast chat messages.
    */
    virtual Move move( int timestamp, const Player *players,
                       int player_index, const Field &field ) = 0;

    /* Handle a chat message. */
    virtual void listen(std::string &name, std::string &text)
    {
        (void)name;
        (void)text;
    };

    /* Low-level callback to receive all incoming player move data. */
    virtual void watch_player( int player_index, int timestamp, Move move,
        double move_rate, double turn_rate, bool solid,
        const Position &old_pos, const Position &new_pos )
    {
        (void)player_index;
        (void)timestamp;
        (void)move;
        (void)move_rate;
        (void)turn_rate;
        (void)solid;
        (void)old_pos;
        (void)new_pos;
    }

    /* Call this method to queue an outgoing chat message */
    void say(const std::string &text)
    {
        m_messages.push_back(text);
    }

    /* Called by the application to retrieve outgoing messages */
    bool retrieve_message(std::string *text)
    {
        if (m_messages.empty()) return false;
        if (text != NULL) *text = m_messages.front();
        m_messages.pop_front();
        return true;
    }

    /* Returns whether this player is human */
    bool human() { return m_human; }

private:
    bool m_human;
    std::deque<std::string> m_messages;
};

#endif /* ndef PLAYER_CONTROLLER_H_INCLUDED */
