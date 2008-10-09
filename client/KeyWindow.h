#ifndef KEY_WINDOW_H_INCLUDED
#define KEY_WINDOW_H_INCLUDED

#include "common.h"

class KeyWindow : public Fl_Window
{
public:
    KeyWindow(int width, int height, const char *title);
    ~KeyWindow();

    int key();
    const char *key_label();

protected:
    int handle(int type);

private:
    int m_key_index;
};

#endif /* ndef KEY_WINDOW_H_INCLUDED */
