#ifndef MAIN_GAME_VIEW_H_INCLUDED
#define MAIN_GAME_VIEW_H_INCLUDED

#include "common.h"
#include "GameView.h"
#include <string>

/* Defined in zatacka.cpp, called by MainGameView */
void send_chat_message(const std::string &msg);

/* Defined in zatacka.cpp, called by MainGameView */
bool is_control_key(int key);

struct ChatLine
{
    double time;
    Fl_Color color;
    std::string text;
};

class MainGameView : public GameView
{
public:
    MainGameView(int x, int y, int w, int h);
    ~MainGameView();

    void appendMessage(const std::string &text, Fl_Color col = FL_WHITE);
    int handleKeyPress(int key);
    void updateTime(double t);

protected:
    void damageChatText();
    void draw();

private:
    bool typing;
    std::string chat_text;
    std::vector<ChatLine> chat_lines;
};

#endif /* ndef MAIN_GAME_VIEW_H_INCLUDED */
