#pragma once

#include "../player.h"

class StatusScreen
{
public:
    StatusScreen(SpotifyPlayer &player);

    void show(bool forceRefresh = false);

private:
    SpotifyPlayer &mPlayer;
    SpotifyPlayerStatus mPreviousStatus;
};
