#line 1 "/Users/holger/PlasticPlayer/src/screens/status.cpp"
#include "status.h"

#include <Arduino.h>

#include <ssd1306.h>

StatusScreen::StatusScreen(SpotifyPlayer &player)
    : mPlayer(player), mPreviousStatus({SpotifyPlayerStatus::DISCONNECTED, "", "", ""})
{
}

void StatusScreen::show(bool forceRefresh)
{
    auto status = mPlayer.getStatus();
    if (status.status == mPreviousStatus.status && status.title == mPreviousStatus.title && !forceRefresh)
    {
        return;
    }

    if (status.status == SpotifyPlayerStatus::DISCONNECTED)
    {
        ssd1306_clearScreen();
        ssd1306_printFixed(0, 0, "Not connected to", STYLE_NORMAL);
        ssd1306_printFixed(0, 10, "Spotify", STYLE_NORMAL);
        return;
    }

    ssd1306_clearScreen();
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

    ssd1306_printFixed(0, 20, status.artist.c_str(), STYLE_NORMAL);
    ssd1306_printFixed(0, 30, status.title.c_str(), STYLE_NORMAL);

    mPreviousStatus = status;
}
