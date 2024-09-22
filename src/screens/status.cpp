#line 1 "/Users/holger/PlasticPlayer/src/screens/status.cpp"
#include "status.h"

#include <Arduino.h>

#include <ssd1306.h>

#define SSD1306_MAX_CHARS 20
#define SSD1306_SCROLL_PADDING 5

void ssd1306_printScrolling(int y, const std::string& text, EFontStyle style)
{
    if (text.length() <= SSD1306_MAX_CHARS)
    {
        ssd1306_printFixed(0, y, text.c_str(), style);
        return;
    }

    int scrollPos = (millis() / 200) % (text.length() - SSD1306_MAX_CHARS + 2 * SSD1306_SCROLL_PADDING);
    scrollPos -= SSD1306_SCROLL_PADDING;
    if (scrollPos < 0)
    {
        scrollPos = 0;
    }
    if (scrollPos > text.length() - SSD1306_MAX_CHARS)
    {
        scrollPos = text.length() - SSD1306_MAX_CHARS;
    }
    ssd1306_printFixed(0, y, text.substr(scrollPos, SSD1306_MAX_CHARS).c_str(), style);
}

StatusScreen::StatusScreen(SpotifyPlayer &player)
    : mPlayer(player), mPreviousStatus({SpotifyPlayerStatus::DISCONNECTED, "", "", ""})
{
}

void StatusScreen::show(bool forceRefresh)
{
    auto status = mPlayer.getStatus();
    if (status.status != mPreviousStatus.status || status.title != mPreviousStatus.title || forceRefresh)
    {
        ssd1306_clearScreen();
    }

    if (status.status == SpotifyPlayerStatus::DISCONNECTED)
    {
        ssd1306_printFixed(0, 0, "Not connected to", STYLE_NORMAL);
        ssd1306_printFixed(0, 10, "Spotify", STYLE_NORMAL);
        return;
    }

    switch (status.status)
    {
    case SpotifyPlayerStatus::PLAYING:
        ssd1306_printFixed(0, 0, "Playing", STYLE_NORMAL);
        break;
    case SpotifyPlayerStatus::PAUSED:
        ssd1306_printFixed(0, 0, "Paused", STYLE_NORMAL);
        break;
    case SpotifyPlayerStatus::STOPPED:
        ssd1306_printFixed(0, 0, "Stopped", STYLE_NORMAL);
        break;
    default:
        ssd1306_printFixed(0, 0, "Unknown status", STYLE_NORMAL);
        break;
    }

    ssd1306_printScrolling(20, status.artist, STYLE_NORMAL);
    ssd1306_printScrolling(30, status.album, STYLE_NORMAL);
    ssd1306_printScrolling(40, status.title, STYLE_NORMAL);

    mPreviousStatus = status;
}
