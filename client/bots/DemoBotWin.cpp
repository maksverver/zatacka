/* A demo bot with a separte window to display debug data. */

#include <client/PlayerController.h>
#include <math.h>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

/* Debug window showing last move the bot made. */
class Window : public Fl_Window
{
public:
    Window(Move *move, const char *title)
        : Fl_Window(100, 100, title), move(move) { };

    void draw()
    {
	// Clear background
	fl_color(FL_BLACK);
        fl_rectf(0, 0, w(), h());

	// Draw an arrow
        fl_push_matrix();
        fl_translate(0.5*w(), 0.5*h());
        fl_scale(0.4*w(), 0.4*h());
        if (*move == MOVE_TURN_RIGHT) fl_rotate(-90);
        else if (*move == MOVE_TURN_LEFT) fl_rotate(90);
        fl_color(FL_RED);
        fl_begin_complex_polygon();
        fl_vertex( 0.0, -1.0);
        fl_vertex( 1.0,  0.0);
        fl_vertex( 0.5,  0.0);
        fl_vertex( 0.5,  1.0);
        fl_vertex(-0.5,  1.0);
        fl_vertex(-0.5,  0.0);
        fl_vertex(-1.0,  0.0);
        fl_end_complex_polygon();
        fl_pop_matrix();
    }

private:
    Move *move;
};

/* The main bot class. Similar to DemoBot. */
class DemoBotWin : public PlayerController
{
public:
    DemoBotWin();
    ~DemoBotWin();

    void restart(const GameParameters &gp);

    Move move( int timestamp, const Player *players,
               int player_index, const Field &field );

private:
    Move last_move;
    int warmup, period;
    Window *window;
};

/* This function is called to created instances of our bot class. */
extern "C" PlayerController *create_bot() {
  return new DemoBotWin;
}

/* Construct a new player: */
DemoBotWin::DemoBotWin() : window(new Window(&last_move, "Demo"))
{
    window->show();
}

/* Clean up player resources: */
DemoBotWin::~DemoBotWin()
{
    delete window;
}

/* A new game starts: clean up old game stat and reinitialize */
void DemoBotWin::restart(const GameParameters &gp)
{
    last_move = MOVE_FORWARD;
    warmup    = gp.warmup;
    period    = (int)(0.75*2.0*M_PI/gp.turn_rate);
    if (window) window->redraw();
}

/* Select a move to make at the given timestamp. */
Move DemoBotWin::move( int timestamp, const Player *players,
                       int player_index, const Field &field )
{
    (void)field;          /* unused */

    /* Join game (even if we start out in the right direction) */
    if (timestamp == 0) return MOVE_TURN_LEFT;

    if (timestamp < warmup)
    {
        /* Turn towards center of field */
        const Position &pos = players[player_index].pos;
        double a = atan2(0.5 - pos.y, 0.5 - pos.x);
        double b = pos.a - a;
        while (b < -M_PI) b += 2.0*M_PI;
        while (b > +M_PI) b -= 2.0*M_PI;
        if (b < -0.1) last_move = MOVE_TURN_LEFT;
        else
        if (b > +0.1) last_move = MOVE_TURN_RIGHT;
        else
            last_move = MOVE_FORWARD;
    }
    else
    {
        int t = timestamp - warmup + period/2;
        last_move = (t/period)%2 ? MOVE_TURN_LEFT : MOVE_TURN_RIGHT;
    }
    if (window) window->redraw();
    return last_move;
}
