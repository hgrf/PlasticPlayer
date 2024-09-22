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

    // TODO: de facto this never happens at the moment because the encapsulating loop continues when mPlayer is NULL
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
    case SpotifyCommand::STOP:
        g_print("processing stop command\n");
        g_object_get(mImpl->mPlayer, "can-play", &can_play, NULL);
        if (!can_play)
        {
            g_debug("can-play is false, skipping");
            break;
        }
        playerctl_player_stop(mImpl->mPlayer, &error);
        break;
    }

    if (error)
    {
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
    }
}

void SpotifyPlayer::getMetadata()
{
    GError *error = NULL;
    gchar *artist = playerctl_player_get_artist(mImpl->mPlayer, &error);
    if (error)
    {
        g_printerr("Failed to get artist: %s\n", error->message);
        g_error_free(error);
        return;
    }

    gchar *album = playerctl_player_get_album(mImpl->mPlayer, &error);
    if (error)
    {
        g_printerr("Failed to get album: %s\n", error->message);
        g_error_free(error);
        g_free(artist);
        return;
    }

    gchar *title = playerctl_player_get_title(mImpl->mPlayer, &error);
    if (error)
    {
        g_printerr("Failed to get title: %s\n", error->message);
        g_error_free(error);
        g_free(artist);
        g_free(album);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStatus.artist = artist ? artist : "";
        mStatus.album = album ? album : "";
        mStatus.title = title ? title : "";
    }

    g_free(artist);
    g_free(album);
    g_free(title);
}

void SpotifyPlayer::getPlaybackStatus()
{
    PlayerctlPlaybackStatus status = PLAYERCTL_PLAYBACK_STATUS_STOPPED;
    g_object_get(mImpl->mPlayer, "playback-status", &status, NULL);

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
        default:
            mStatus.status = SpotifyPlayerStatus::DISCONNECTED;
            break;
        }
    }
}

void SpotifyPlayer::threadEntry()
{
    GError *error = NULL;

    while (mRunning)
    {
        if (mImpl->mManager == NULL)
        {
            mImpl->mManager = playerctl_player_manager_new(&error);
            if (mImpl->mManager == NULL)
            {
                g_printerr("Failed to create player manager: %s\n", error->message
                                                                        ? error->message
                                                                        : "unknown error");
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                // TODO: make sure command queue is emptied
                continue;
            }
        }

        GList *players = NULL;
        g_object_get(mImpl->mManager, "player-names", &players, NULL);
        if (players == NULL)
        {
            g_printerr("No players found\n");
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStatus.status = SpotifyPlayerStatus::DISCONNECTED;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            g_object_unref(mImpl->mManager);
            mImpl->mManager = NULL;
            // TODO: make sure command queue is emptied
            continue;
        }

        PlayerctlPlayerName *name = static_cast<PlayerctlPlayerName *>(players->data);
        mImpl->mPlayer = playerctl_player_new_from_name(name, &error);
        if (error)
        {
            g_printerr("Failed to create player: %s\n", error->message);
            g_error_free(error);
            g_object_unref(mImpl->mManager);
            mImpl->mManager = NULL;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStatus.status = SpotifyPlayerStatus::DISCONNECTED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            // TODO: make sure command queue is emptied
            continue;
        }

        getPlaybackStatus();
        getMetadata();
        g_print("Player: %s status: %d title: %s\n", name->name, mStatus.status, mStatus.title.c_str());

        {
            std::unique_lock<std::mutex> lock(mMutex);
            if (!mCond.wait_for(lock, std::chrono::milliseconds(100), [this]
                                { return mRunning && !mCommandQueue.empty(); }))
            {
                g_object_unref(mImpl->mPlayer);
                continue;
            }
            SpotifyCommand cmd = mCommandQueue.front();
            mCommandQueue.pop();
            lock.unlock();
            processCommand(cmd);
        }

        g_object_unref(mImpl->mPlayer);
    }

    if (mImpl->mManager)
    {
        g_object_unref(mImpl->mManager);
    }
}
