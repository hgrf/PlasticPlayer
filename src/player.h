#ifndef PLAYER_H
#define PLAYER_H

#include <Arduino.h>

#include <playerctl/playerctl.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <playerctl/playerctl-common.h>
#include <playerctl/playerctl-formatter.h>
#ifdef __cplusplus
}

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
  const String payload;

  SpotifyCommand(CommandType t) : typ(t)
  {
  }

  SpotifyCommand(CommandType t, const String &p) : typ(t), payload(p)
  {
  }
};

class SpotifyPlayer
{
public:
  void begin()
  {
    GError *error = NULL;
    mManager = playerctl_player_manager_new(&error);
    if (error != NULL)
    {
      g_printerr("Could not connect to players: %s\n", error->message);
    }
  }
  void process(const SpotifyCommand &cmd)
  {
    GError *error = NULL;
    GList *available_players = NULL;
    gboolean can_control = FALSE;
    gboolean can_play = FALSE;
    gboolean can_go_next = FALSE;

    if (mManager == NULL)
    {
      g_printerr("Manager not initialized\n");
      return;
    }

    g_object_get(mManager, "player-names", &available_players, NULL);
    GList *l = NULL;
    for (l = available_players; l != NULL; l = l->next)
    {
      PlayerctlPlayerName *name = static_cast<PlayerctlPlayerName *>(l->data);
      printf("found player: %s\n", name->instance);

      PlayerctlPlayer *player = playerctl_player_new_from_name(name, &error);
      if (error != NULL)
      {
        g_printerr("Could not connect to player: %s\n", error->message);
        break;
      }

      switch (cmd.typ)
      {
      case SpotifyCommand::PLAY:
        g_object_get(player, "can-control", &can_control, NULL);
        if (!can_control)
        {
          g_printerr("%s: player cannot control", name->instance);
          break;
        }
        printf("open %s\n", cmd.payload.c_str());
        playerctl_player_open(player, (gchar *)cmd.payload.c_str(), &error);
        break;
      case SpotifyCommand::PLAY_PAUSE:
        g_object_get(player, "can-play", &can_play, NULL);
        if (!can_play)
        {
          g_debug("%s: can-play is false, skipping", name->instance);
          break;
        }
        playerctl_player_play_pause(player, &error);
        break;
      case SpotifyCommand::NEXT:
        g_object_get(player, "can-go-next", &can_go_next, NULL);
        if (!can_go_next)
        {
          g_debug("%s: player cannot go next", name->instance);
          break;
        }
        playerctl_player_next(player, &error);
        break;
      case SpotifyCommand::PREVIOUS:
          g_object_get(player, "can-go-previous", &can_go_next, NULL);
        if (!can_go_next)
        {
          g_debug("%s: player cannot go previous", name->instance);
          break;
        }
         playerctl_player_previous(player, &error);
        break;
      }

      if (error)
      {
        g_printerr("Error: %s\n", error->message);
      }

      g_object_unref(player);

      // TODO: loop not necessary
      break;
    }
  }

private:
  PlayerctlPlayerManager *mManager = NULL;
};

#endif

#endif // PLAYER_H
