#include "KeyWindow.h"

/* 102 keys on the keyboard, 8 buttons on the mouse.
   Note that not all keys work on all platforms. */
#define NUM_KEYS 102 + 8

static const int key_codes[NUM_KEYS] =
{
    /* Letter keys (26) */
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',

    /* Number keys (10) */
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',

    /* ASCII punctuation (11) */
    '`', '-', '=', '\\', '[', ']', ';', '\'', ',', '.', '/',

    /* Num-pad keys (15) */
    FL_KP + '1', FL_KP + '2', FL_KP + '3', FL_KP + '4', FL_KP + '5',
    FL_KP + '6', FL_KP + '7', FL_KP + '8', FL_KP + '9', FL_KP + '0',
    FL_KP + '.', FL_KP + '+', FL_KP + '-', FL_KP + '*', FL_KP + '/',

    /* Function keys (12) */
    FL_F +  1, FL_F +  2, FL_F +  3, FL_F +  4, FL_F +  5, FL_F +  6,
    FL_F +  7, FL_F +  8, FL_F +  9, FL_F + 10, FL_F + 11, FL_F + 12,

    /* Special keys (27) */
    FL_Escape, FL_BackSpace, FL_Tab, FL_Print,
    FL_Scroll_Lock, FL_Pause, FL_Insert, FL_Home,
    FL_Page_Up, FL_Delete, FL_End, FL_Page_Down,
    FL_Left, FL_Up, FL_Right, FL_Down,
    FL_Shift_L, FL_Shift_R, FL_Control_L, FL_Control_R,
    FL_Alt_L, FL_Alt_R, FL_Meta_L, FL_Meta_R,
    FL_Menu, FL_Caps_Lock, FL_Num_Lock, ' ',

    /* Mouse buttons (8) */
    FL_Button + 1, FL_Button + 2, FL_Button + 3, FL_Button + 4,
    FL_Button + 5, FL_Button + 6, FL_Button + 7, FL_Button + 8
};

static const char *key_labels[NUM_KEYS] =
{
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
    "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",

    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",

    "`", "-", "=", "\\", "[", "]", ";", "'", ",", ".", "/",

    "Num 1", "Num 2", "Num 3", "Num 4", "Num 5",
    "Num 6", "Num 7", "Num 8", "Num 9", "Num 0",
    "Num .", "Num +", "Num -", "Num *", "Num /",

    "F1", "F2", "F3", "F4", "F5", "F6",
    "F7", "F8", "F9", "F10", "F11", "F12",

    "Esc", "BckSp", "Tab", "PrScr",
    "ScrLk", "Brk", "Ins", "Home",
    "PgUp", "Del", "End", "PgDn",
    "@4->", "@8->", "@6->", "@2->",
    "LSh", "RSh", "LCtl", "RCtl",
    "LAlt", "RAlt", "LMeta", "RMeta",
    "Menu", "CpsLk", "NumLk", "Space",

    "MB 1", "MB 2", "MB 3", "MB 4",
    "MB 5", "MB 6", "MB 7", "MB 8"
};


KeyWindow::KeyWindow(int width, int height, const char *title)
    : Fl_Window(width, height, title), m_key_index(-1)
{
    set_modal();
}

KeyWindow::~KeyWindow()
{
}

int KeyWindow::handle(int type)
{
    if (type != FL_KEYDOWN && type != FL_PUSH) return 0;

    int key = Fl::event_key();

    for (int i = 0; i < NUM_KEYS; ++i)
    {
        if (key_codes[i] == key)
        {
            m_key_index = i;
            hide();
            return 1;
        }
    }
    printf("WARNING: key %d not found!\n", key);
    return 0;
}

int KeyWindow::key()
{
    return m_key_index >= 0 ? key_codes[m_key_index] : 0;
}

const char *KeyWindow::key_label()
{
    return m_key_index >= 0 ? key_labels[m_key_index] : NULL;
}
