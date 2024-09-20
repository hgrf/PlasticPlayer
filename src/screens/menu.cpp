#include "menu.h"

#include <ssd1306.h>

static SAppMenu menu;

static const char *menuItems[] = {
    "Play/Pause",
    "Next",
    "Previous",
    "Bluetooth",
};

MenuScreen::MenuScreen(SpotifyPlayer &player, Button &buttonNext, Button &buttonSelect)
    : mVisible(false), mLastEvent(0), mPlayer(player), mButtonNext(buttonNext), mButtonSelect(buttonSelect)
{
    ssd1306_createMenu(&menu, (const char **)menuItems, sizeof(menuItems) / sizeof(char *));
}

void MenuScreen::show()
{
    ssd1306_clearScreen();
    ssd1306_showMenu(&menu);
}

void MenuScreen::next()
{
    ssd1306_menuDown(&menu);
    ssd1306_updateMenu(&menu);
}

void MenuScreen::previous()
{
    ssd1306_menuUp(&menu);
    ssd1306_updateMenu(&menu);
}

void MenuScreen::select()
{
    switch (menu.selection)
    {
    case 0:
        mPlayer.process(SpotifyCommand(SpotifyCommand::PLAY_PAUSE));
        break;
    case 1:
        mPlayer.process(SpotifyCommand(SpotifyCommand::NEXT));
        break;
    case 2:
        mPlayer.process(SpotifyCommand(SpotifyCommand::PREVIOUS));
        break;
    case 3:
        break;
    }
}

void MenuScreen::process()
{
    if (mButtonNext.hasEvent() && mButtonNext.waitForEvent() == Button::PRESS)
    {
        mLastEvent = millis();
        if (!mVisible)
        {
            mVisible = true;
            show();
        }
        else
        {
            next();
        }
    }

    if (mButtonSelect.hasEvent() && mButtonSelect.waitForEvent() == Button::PRESS)
    {
        mLastEvent = millis();
        if (!mVisible)
        {
            mVisible = true;
            show();
        }
        else
        {
            select();
            mVisible = false;
        }
    }

    if (mVisible && millis() - mLastEvent > 3000)
    {
        mVisible = false;
    }
}

bool MenuScreen::isVisible() const
{
    return mVisible;
}