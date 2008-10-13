#include <client/PlayerController.h>
#include <math.h>

/* Bot definition goes here.
   It must be derived from PlayerController, and implement the restart()
   and move() methods. */
class DemoBot : public PlayerController
{
public:
    DemoBot();
    ~DemoBot();

    void restart(const GameParameters &gp);

    Move move( int timestamp, const Player *players,
               int player_index, const Field &field );

private:
    int warmup, period;
};

/* This function is called to created instances of our bot class. */
extern "C" PlayerController *create_bot() { return new DemoBot(); }

/* Constructor; bot initialization goes here */
DemoBot::DemoBot()
{
}

/* Destructor; clean up all allocated resources for this bot here. */
DemoBot::~DemoBot()
{
}

/* A new game starts: clean up old game stat and reinitialize */
void DemoBot::restart(const GameParameters &gp)
{
    warmup = gp.warmup;
    period = (int)(0.75*2.0*M_PI/gp.turn_rate);
}

/* Select a move to make at the given timestamp. */
Move DemoBot::move( int timestamp, const Player *players,
                    int player_index, const Field &field )
{
    (void)field;          /* unused */

    if (timestamp < warmup)
    {
        /* Turn towards center of field */
        const Player &pl = players[player_index];
        double a = atan2(0.5 - pl.y, 0.5 - pl.x);
        double b = pl.a - a;
        while (b < -M_PI) b += 2.0*M_PI;
        while (b > +M_PI) b -= 2.0*M_PI;
        if (b < -0.1) return MOVE_TURN_LEFT;
        if (b > +0.1) return MOVE_TURN_RIGHT;
        return MOVE_FORWARD;
    }
    else
    {
        int t = timestamp - warmup + period/2;
        return (t/period)%2 ? MOVE_TURN_LEFT : MOVE_TURN_RIGHT;
    }
}
