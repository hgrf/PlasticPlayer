#pragma once

#include "../button.h"
#include "../player.h"

class MenuScreen
{
public:
    MenuScreen(SpotifyPlayer &player, Button &buttonNext, Button &buttonSelect);
    ~MenuScreen() = default;

    void show();
    void select();
    void next();
    void previous();
    void process();
    bool isVisible() const;

private:
    bool mVisible;
    uint64_t mLastEvent;
    SpotifyPlayer &mPlayer;
    Button &mButtonNext;
    Button &mButtonSelect;
};
