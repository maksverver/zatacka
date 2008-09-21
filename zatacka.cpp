#include "common.h"
#include "Debug.h"
#include "GameView.h"
#include "ClientSocket.h"
#include "Protocol.h"

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

static void handle_MESG(unsigned char *buf, size_t len)
{
    /* TODO */
    (void)buf;
    (void)len;
    info("MESG received (TODO!)");
}

static void handle_DISC(unsigned char *buf, size_t len)
{
    info("Disconnected by server; reason:");
    fwrite(buf + 1, 1, len - 1, stdout);
    fputc('\n', stdout);
    exit(0);
}

static void handle_STRT(unsigned char *buf, size_t len)
{
    /* TODO */
    (void)buf;
    (void)len;
    info("STRT received (TODO!)");
}


static void handle_packet(unsigned char *buf, size_t len)
{
    info("packet type %d of length %d received", (int)buf[0], len);
    hex_dump(buf, len);
    switch ((int)buf[0])
    {
    case MRSC_MESG: return handle_MESG(buf, len);
    case MRSC_DISC: return handle_DISC(buf, len);
    case MRSC_STRT: return handle_STRT(buf, len);
    default: error("invalid message type");
    }
}

void callback(void *arg)
{
    /* Read network input */
    unsigned char buf[4096];
    ssize_t len;
    while ((len = g_cs->read(buf, sizeof(buf) - 1)) > 0)
    {
        handle_packet(buf, (size_t)len);
    }
    if (len < 0) error("socket read failed");

    /* Write network output */
    /* g_cs->write("Tick!", 5, false); */

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

static void disconnect()
{
    unsigned char msg[1] = { MRCS_QUIT };
    if (g_cs != NULL) g_cs->write(msg, sizeof(msg), true);
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

    /* Send hello message */
    g_cs->write("\x00\x04Maks", 6, true);

    /* FIXME: make resolution and framerate configurable */
    int tmp;
    Fl_Window *window = new Fl_Window(800,600);
    g_gv = new GameView(1, 0,0,800,600);
    window->end();
    window->show(argc, argv);
    /* FIXME: should wait for window to be visible */
    Fl::add_timeout(0.5, callback, &tmp);
    int res = Fl::run();
    disconnect();
    return res;
}
