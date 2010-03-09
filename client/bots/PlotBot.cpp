/* A bot that just plots the game field. */

#include <common/Colors.h>
#include <client/PlayerController.h>
#include <FL/Fl_Double_Window.H>
#include <FL/fl_draw.H>
#include <math.h>
#include <vector>

struct Point2f
{
    float x, y;
};

struct Quadrilateral
{
    Fl_Color color;
    Point2f vertex[4];
};

class Window : public Fl_Double_Window
{
public:
    Window(const char *title) : Fl_Double_Window(400, 400, title)
    {
        size_range(100, 100);
    };

    void draw()
    {
        // Clear background
        fl_color(FL_BLACK);
        fl_rectf(0, 0, w(), h());

        // Draw quadrilaterals
        fl_push_matrix();
        fl_translate(0, h());
        fl_scale((float)w()/FIELD_SIZE, -(float)h()/FIELD_SIZE);
        for ( std::vector<Quadrilateral>::const_iterator it = quads.begin();
              it != quads.end(); ++it )
        {
            fl_color(it->color);
            fl_begin_polygon();
            fl_vertex(it->vertex[0].x, it->vertex[0].y);
            fl_vertex(it->vertex[1].x, it->vertex[1].y);
            fl_vertex(it->vertex[2].x, it->vertex[2].y);
            fl_vertex(it->vertex[3].x, it->vertex[3].y);
            fl_end_polygon();
        }
        fl_pop_matrix();
    }

    void clear()
    {
        quads.clear();
        redraw();
    }

    void add(const Quadrilateral &quad)
    {
        quads.push_back(quad);
        redraw();
    }

private:
    std::vector<Quadrilateral> quads;
};

class PlotBot : public PlayerController
{
public:
    PlotBot() : window(new Window("Plot Demo"))
    {
        window->show();
    }

    ~PlotBot()
    {
        delete window;
    }

    void restart(const GameParameters &gp)
    {
        (void)gp;  // unused
        window->clear();
    }

    Move move( int timestamp, const Player *players,
               int player_index, const Field &field )
    {
        (void)timestamp;     // unused
        (void)players;       // unused
        (void)player_index;  // unused
        (void)field;         // unused
        return MOVE_FORWARD;
    }

    void watch_player( int player_index, int timestamp, Move move,
        double move_rate, double turn_rate, bool solid,
        const Position &p, const Position &q )
    {
        (void)player_index;  // unused
        (void)timestamp;     // unused
        (void)move;          // unused
        (void)move_rate;     // unused
        (void)turn_rate;     // unused

        if ((p.x != q.x || p.y != q.y) && solid)
        {
            const float th = LINE_WIDTH;
            float dx1 = -sin(p.a), dy1 = cos(p.a);
            float dx2 = -sin(q.a), dy2 = cos(q.a);

            const RGB &rgb = g_colors[player_index%NUM_COLORS];
            Quadrilateral quad = {
                fl_rgb_color(rgb.r, rgb.g, rgb.b),
                { { FIELD_SIZE*p.x + 0.5f - 0.5f*th*dx1,
                    FIELD_SIZE*p.y + 0.5f - 0.5f*th*dy1 },
                  { FIELD_SIZE*p.x + 0.5f + 0.5f*th*dx1,
                    FIELD_SIZE*p.y + 0.5f + 0.5f*th*dy1 },
                  { FIELD_SIZE*q.x + 0.5f + 0.5f*th*dx2,
                    FIELD_SIZE*q.y + 0.5f + 0.5f*th*dy2 },
                  { FIELD_SIZE*q.x + 0.5f - 0.5f*th*dx2,
                    FIELD_SIZE*q.y + 0.5f - 0.5f*th*dy2 } } };
            window->add(quad);
        }
    }

private:
    Window *window;
};

/* This function is called to created instances of our bot class. */
extern "C" PlayerController *create_bot() {
  return new PlotBot;
}
