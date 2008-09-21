#include "Debug.h"
#include "common.h"
#include "GameView.h"
#include "ClientSocket.h"

#define HZ 4

/* Global variables*/
ClientSocket *g_cs;     /* client connection to the server */
GameView *g_gv;         /* graphical game view */
int g_data_rate;        /* moves per second (currently also display FPS) */
int g_turn_rate;        /* turn rate (2pi/g_turn_rate radians per turn) */
int g_move_rate;        /* move rate (g_move_rate*screen_size/1000 per turn) */
int g_backlog;          /* nubmer of moves to cache and send/receive */

double x = 100, y = 100;
double a = 0;


void callback(void *arg)
{
    /* Read network input */
    char buf[2048];
    ssize_t len;

    len = g_cs->read(buf, sizeof(buf) - 1);
    if (len > 0)
    {
        buf[len] = 0;
        info("received: [%s]\n", buf);
    }
    if (len < 0)
    {
        error("socket read failed");
    }

    /* Write network output */
    g_cs->write("Tick!", 5, false);

    if (Fl::event_key(FL_Left) && !Fl::event_key(FL_Right))
    {
        a -= 5;
        if (a < 0) a += 360;
    }

    if (Fl::event_key(FL_Right) && !Fl::event_key(FL_Left))
    {
        a += 5;
        if (a >= 360) a -= 360;
    }

    int &i = *(int*)arg;
    printf("%s\n", ++i%2 ? "tick" : "tock");
    if (i%100 < 90)
    {
        g_gv->plot(round(x), round(y), FL_YELLOW);
    }
    g_gv->setSprite(0, round(x), round(y), -a, FL_YELLOW);
    x += 3*cos(M_PI/180*a);
    y += 3*sin(M_PI/180*a);
    Fl::repeat_timeout(1.0/HZ, callback, arg);
}

int main(int argc, char *argv[])
{
    time_reset();

    /* Connect to the server */
    g_cs = new ClientSocket("heaven", 12321);
    if (!g_cs->connected())
    {
        error("Couldn't connect to server.");
        return 1;
    }
    g_cs->write("Hello world!\r\n", 14, true);

    /* FIXME: make resolution and framerate configurable */
    int tmp;
    Fl_Window *window = new Fl_Window(800,600);
    g_gv = new GameView(1, 0,0,800,600);
    window->end();
    window->show(argc, argv);
    /* FIXME: should wait for window to be visible */
    Fl::add_timeout(0.5, callback, &tmp);
    return Fl::run();
}
