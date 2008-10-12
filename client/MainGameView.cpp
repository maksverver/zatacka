#include "MainGameView.h"

static char shift(char c)
{
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    switch (c)
    {
    case '`': return '~';
    case '1': return '!';
    case '2': return '@';
    case '3': return '#';
    case '4': return '$';
    case '5': return '%';
    case '6': return '^';
    case '7': return '&';
    case '8': return '*';
    case '9': return '(';
    case '0': return ')';
    case '-': return '_';
    case '=': return '+';
    case '\\': return '|';
    case '[': return '{';
    case ']': return '}';
    case ';': return ':';
    case '\'': return '"';
    case ',': return '<';
    case '.': return '>';
    case '/': return '?';
    default: return c;
    }
}

MainGameView::MainGameView(int x, int y, int w, int h, bool antialiasing)
    : GameView(x, y, w, h, antialiasing), typing(false)
{
}

MainGameView::~MainGameView()
{
}

void MainGameView::appendMessage(const std::string &text, Fl_Color col)
{
    if (!chat_lines.empty() && chat_lines.back().text == text)
    {
        /* Don't duplicate messages */
        chat_lines.back().time = time_now();
        return;
    }

    ChatLine cl;
    cl.time  = time_now();
    cl.color = col;
    cl.text  = text;
    chat_lines.push_back(cl);

    /* Only show the few latest messages */
    while (chat_lines.size() > 5) chat_lines.erase(chat_lines.begin());

    damageChatText();
}

int MainGameView::handleKeyPress(int key)
{
    if ( key != FL_Enter && key != FL_Escape && key != FL_BackSpace
         && (key < 32 || key > 126) )
    {
        /* Don't handle this key */
        return 0;
    }

    /* Enter starts chat (or sends current message) */
    if (key == FL_Enter)
    {
        if (!typing)
        {
            typing = true;
        }
        else
        {
            if (!chat_text.empty())
            {
                send_chat_message(chat_text);
                chat_text.clear();
            }
            typing = false;
        }
    }
    else
    if (key == FL_Escape)
    {
        typing = false;
        chat_text.clear();
    }
    else
    if (key == FL_BackSpace)
    {
        if (!chat_text.empty())
        {
            chat_text.erase(chat_text.end() - 1);
        }
        else
        {
            typing = false;
        }
    }
    else
    {
        /* Start typing, except if the key pressed is also a control key. */
        if (!is_control_key(key)) typing = true;

        if (typing && key >= 32 && key < 126)
        {
            char c = Fl::event_state(FL_SHIFT) ? shift(key) : key;
            if (chat_text.size() < 100) chat_text += c;
        }
    }
    damageChatText();
    return 1;
}

void MainGameView::updateTime(double t)
{
    std::vector<ChatLine>::iterator i = chat_lines.begin();

    /* Remove chat lines after 8 seconds */
    while (i != chat_lines.end() && i->time < t - 8) ++i;
    if (i == chat_lines.begin()) return;
    chat_lines.erase(chat_lines.begin(), i);
    damageChatText();
}

void MainGameView::setWarmup(bool value)
{
    if (value == warmup) return;
    warmup = value;
    damage(1);
}

void MainGameView::damageChatText()
{
    int height = 8 + 6*16;
    damage(1, x(), y() + h() - height, w(), height);
}

void MainGameView::draw()
{
    GameView::draw();

    /* Draw chat lines */
    fl_font(FL_HELVETICA, 14);
    int x = this->x() + 8, y = this->y() + h() - 8;
    if (typing)
    {
        fl_color(FL_WHITE);
        fl_draw(("> " + chat_text).c_str(), x, y);
        y -= 16;
    }
    for (int n = (int)chat_lines.size() - 1; n >= 0; --n)
    {
        fl_color(chat_lines[n].color);
        fl_draw(chat_lines[n].text.c_str(), x, y);
        y -= 16;
    }

    if (getSprites() == 0)
    {
        /* Display waiting screen */
        fl_font(FL_HELVETICA, 24);
        fl_color(FL_WHITE);
        fl_draw( "Succesfully connected to server!\n"
                 "Please wait for the next round to start.",
                 this->x(), this->y(), w(), h(), FL_ALIGN_CENTER );
    }
    else
    if (warmup)
    {
        fl_font(FL_HELVETICA, 24);
        fl_color(FL_WHITE);
        fl_draw( "Prepare for the next round!\n"
                "Press movement keys to play.",
                    this->x(), this->y(), w(), h(), FL_ALIGN_CENTER );
    }
}
