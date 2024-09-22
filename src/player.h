#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class SpotifyCommand
{
public:
  enum CommandType
  {
    PLAY,
    PLAY_PAUSE,
    NEXT,
    PREVIOUS,
  };
  const CommandType typ;
  const std::string payload;

  SpotifyCommand(CommandType t) : typ(t)
  {
  }

  SpotifyCommand(CommandType t, const std::string &p) : typ(t), payload(p)
  {
  }
};

struct SpotifyPlayerStatus
{
  enum Status
  {
    DISCONNECTED,
    STOPPED,
    PLAYING,
    PAUSED,
  };
  Status status;
  std::string title;
  std::string artist;
  std::string album;
};

class SpotifyPlayer
{
public:
  SpotifyPlayer();
  ~SpotifyPlayer();

  SpotifyPlayerStatus getStatus();
  void pushCommand(const SpotifyCommand &cmd);

private:
  void threadEntry();
  void processCommand(const SpotifyCommand &cmd);
  void getMetadata();
  void getPlaybackStatus();

  class SpotifyPlayerImpl;
  std::unique_ptr<SpotifyPlayerImpl> mImpl;
  bool mRunning;
  SpotifyPlayerStatus mStatus;
  std::queue<SpotifyCommand> mCommandQueue;
  std::mutex mMutex;
  std::condition_variable mCond;
  std::thread mThread;
};
