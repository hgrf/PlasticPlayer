#include "player.h"

#include <playerctl/playerctl.h>

#define RETRY_DELAY_MS 1000

extern "C"
{
#include <playerctl/playerctl-common.h>
#include <playerctl/playerctl-formatter.h>
}

class SpotifyPlayer::SpotifyPlayerImpl
{
public:
    PlayerctlPlayerManager *mManager = NULL;
    PlayerctlPlayer *mPlayer = NULL;
};

SpotifyPlayer::SpotifyPlayer()
    : mImpl(new SpotifyPlayerImpl()), mRunning(true), mThread(&SpotifyPlayer::threadEntry, this)
{
}

SpotifyPlayer::~SpotifyPlayer()
{
    mRunning = false;
    mThread.join();
}

SpotifyPlayerStatus SpotifyPlayer::getStatus()
{
    std::lock_guard<std::mutex> lock(mMutex);
    SpotifyPlayerStatus status = mStatus;
    return status;
}

void SpotifyPlayer::pushCommand(const SpotifyCommand &cmd)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mCommandQueue.push(cmd);
    mCond.notify_one();
}

void SpotifyPlayer::processCommand(const SpotifyCommand &cmd)
{
    GError *error = NULL;
    gboolean can_control = FALSE;
    gboolean can_play = FALSE;
    gboolean can_go_next = FALSE;

    if (mImpl->mPlayer == NULL)
    {
        g_printerr("Not connected to player\n");
        return;
    }

    switch (cmd.typ)
    {
    case SpotifyCommand::PLAY:
        g_print("processing play command\n");
        g_object_get(mImpl->mPlayer, "can-control", &can_control, NULL);
        if (!can_control)
        {
            g_printerr("player cannot control");
            break;
        }
        printf("open %s\n", cmd.payload.c_str());
        playerctl_player_open(mImpl->mPlayer, (gchar *)cmd.payload.c_str(), &error);
        break;
    case SpotifyCommand::PLAY_PAUSE:
        g_print("processing play/pause command\n");

        g_object_get(mImpl->mPlayer, "can-play", &can_play, NULL);
        if (!can_play)
        {
            g_debug("can-play is false, skipping");
            break;
        }
        playerctl_player_play_pause(mImpl->mPlayer, &error);
        break;
    case SpotifyCommand::NEXT:
        g_print("processing next command\n");
        g_object_get(mImpl->mPlayer, "can-go-next", &can_go_next, NULL);
        if (!can_go_next)
        {
            g_debug("player cannot go next");
            break;
        }
        playerctl_player_next(mImpl->mPlayer, &error);
        break;
    case SpotifyCommand::PREVIOUS:
        g_print("processing previous command\n");
        g_object_get(mImpl->mPlayer, "can-go-previous", &can_go_next, NULL);
        if (!can_go_next)
        {
            g_debug("player cannot go previous");
            break;
        }
        playerctl_player_previous(mImpl->mPlayer, &error);
        break;
    }

    if (error)
    {
        g_printerr("Error: %s\n", error->message);
    }
}

void SpotifyPlayer::threadEntry()
{
    GError *error = NULL;
    mImpl->mManager = playerctl_player_manager_new(&error);
    if (mImpl->mManager == NULL)
    {
        g_printerr("Failed to create player manager: %s\n", error->message
                                                                ? error->message
                                                                : "unknown error");
        mRunning = false;
    }

    while (mRunning)
    {
        GList *players = NULL;
        g_object_get(mImpl->mManager, "player-names", &players, NULL);
        if (players == NULL)
        {
            g_printerr("No players found\n");
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStatus.status = SpotifyPlayerStatus::DISCONNECTED;
            }

            // TODO: try to reinit the manager after a while
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            continue;
        }

        PlayerctlPlayerName *name = static_cast<PlayerctlPlayerName *>(players->data);
        mImpl->mPlayer = playerctl_player_new_from_name(name, &error);
        if (mImpl->mPlayer == NULL)
        {
            g_printerr("Failed to create player: %s\n", error->message
                                                            ? error->message
                                                            : "unknown error");
            g_list_free(players);
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStatus.status = SpotifyPlayerStatus::DISCONNECTED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            continue;
        }

        PlayerctlPlaybackStatus status = PLAYERCTL_PLAYBACK_STATUS_STOPPED;
        g_object_get(mImpl->mPlayer, "playback-status", &status, NULL);
        gchar *artist = playerctl_player_get_artist(mImpl->mPlayer, &error);
        gchar *album = playerctl_player_get_album(mImpl->mPlayer, &error);
        gchar *title = playerctl_player_get_title(mImpl->mPlayer, &error);

        g_print("status: %d, artist: %s, album: %s, title: %s\n", status, artist, album, title);

        {
            std::lock_guard<std::mutex> lock(mMutex);
            switch (status)
            {
            case PLAYERCTL_PLAYBACK_STATUS_PLAYING:
                mStatus.status = SpotifyPlayerStatus::PLAYING;
                break;
            case PLAYERCTL_PLAYBACK_STATUS_PAUSED:
                mStatus.status = SpotifyPlayerStatus::PAUSED;
                break;
            case PLAYERCTL_PLAYBACK_STATUS_STOPPED:
                mStatus.status = SpotifyPlayerStatus::STOPPED;
                break;
            }
            mStatus.artist = artist;
            mStatus.album = album;
            mStatus.title = title;
        }

        g_free(artist);
        g_free(title);
        g_free(album);

        {
            std::unique_lock<std::mutex> lock(mMutex);
            if (mCond.wait_for(lock, std::chrono::milliseconds(100), [this]
                               { return mRunning && !mCommandQueue.empty(); }) == false)
            {
                continue;
            }
            SpotifyCommand cmd = mCommandQueue.front();
            mCommandQueue.pop();
            lock.unlock();
            processCommand(cmd);
        }

        g_object_unref(mImpl->mPlayer);
        g_list_free(players);
    }

    if (mImpl->mManager != NULL)
    {
        g_object_unref(mImpl->mManager);
    }
}
