#ifndef KEY_WINDOW_H_INCLUDED
#define KEY_WINDOW_H_INCLUDED

#include "common.h"

/* 102 keys on the keyboard, 8 buttons on the mouse.
   Note that not all keys work on all platforms. */
#define NUM_KEYS (102 + 8)
extern const int key_codes[NUM_KEYS];
extern const char *key_labels[NUM_KEYS];

class KeyWindow : public Fl_Window
{
public:
    KeyWindow(int width, int height, const char *title);
    ~KeyWindow();

    int key();

protected:
    int handle(int type);

private:
    int m_key;
};

#endif /* ndef KEY_WINDOW_H_INCLUDED */
