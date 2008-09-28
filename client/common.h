#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <Fl/Fl.H>
#include <Fl/Fl_Widget.H>
#include <Fl/Fl_Box.H>
#include <Fl/Fl_Check_Button.H>
#include <Fl/Fl_Choice.H>
#include <Fl/Fl_Input.H>
#include <Fl/Fl_Pack.H>
#include <FL/fl_ask.H>
#include <Fl/fl_draw.H>
#include <Fl/x.H>
#include <vector>

#include <common/Debug.h>
#include <common/Protocol.h>
#include <common/Time.h>
#include <common/Field.h>
#include <common/BMP.h>

/* Redefine fatal to a function that uses the GUI to display the error
   message to the user, instead of printing it on the console and aborting. */
#ifdef fatal
#undef fatal
#endif
__attribute__((noreturn)) void gui_fatal(const char *fmt, ...);
#define fatal gui_fatal

/* Define assert so it calls gui_fatal, which displays an error message instead
   of printing on to the console and aborting. */
#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) \
    ((bool)(expr) ? (void)0 : \
    (void)gui_fatal("assertion %s failed at %s:%d", #expr, __FILE__, __LINE__))
#endif



#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
