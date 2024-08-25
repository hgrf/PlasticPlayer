#include <SPI.h>
#include <MFRC522.h>
#include <NfcAdapter.h>
#include <ssd1306.h>

#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>
#include <thread>

#define RST_PIN 25
#define SS_PIN 8

// TODO: debug left pin hardware
#define BUTTON_LEFT_PIN 26
#define BUTTON_RIGHT_PIN 17

extern "C"
{
  /* The manager of all the players we connect to */

#include <playerctl/playerctl.h>
#include <playerctl/playerctl-common.h>
#include <playerctl/playerctl-formatter.h>
#include <playerctl/playerctl-player-private.h>

  void playercmd_open(PlayerctlPlayer *player, const gchar *uri,
                      GError **error)
  {
    GError *tmp_error = NULL;
    gchar *instance = pctl_player_get_instance(player);

    gboolean can_control = FALSE;
    g_object_get(player, "can-control", &can_control, NULL);

    if (!can_control)
    {
      g_printerr("%s: player cannot control", instance);
      return;
    }

    if (uri)
    {
      gchar *full_uri = NULL;

      // it may be some other scheme, just pass the uri directly
      full_uri = g_strdup(uri);

      // TODO: need the string replacement
      // TODO: only do this once from the main thread for each new tag
      // TODO: show player state on display
      // TODO: show when player unavailable ("Please connect PlasticPlayer3 with Spotify")
      playerctl_player_open(player, (gchar *)uri, &tmp_error);

      g_free(full_uri);

      if (tmp_error != NULL)
      {
        g_propagate_error(error, tmp_error);
        return;
      }
    }

    return;
  }

  void playercmd_play_pause(PlayerctlPlayer *player, GError **error)
  {
    GError *tmp_error = NULL;
    gchar *instance = pctl_player_get_instance(player);

    gboolean can_play = FALSE;
    g_object_get(player, "can-play", &can_play, NULL);

    if (!can_play)
    {
      g_debug("%s: can-play is false, skipping", instance);
      return;
    }

    playerctl_player_play_pause(player, &tmp_error);
    if (tmp_error)
    {
      g_propagate_error(error, tmp_error);
      return;
    }
    return;
  }

  void playercmd_next(PlayerctlPlayer *player, GError **error)
  {
    GError *tmp_error = NULL;
    gchar *instance = pctl_player_get_instance(player);

    gboolean can_go_next = FALSE;
    g_object_get(player, "can-go-next", &can_go_next, NULL);

    if (!can_go_next)
    {
      g_debug("%s: player cannot go next", instance);
      return;
    }

    playerctl_player_next(player, &tmp_error);
    if (tmp_error)
    {
      g_propagate_error(error, tmp_error);
      return;
    }
    return;
  }

  void playercmd_previous(PlayerctlPlayer *player, GError **error)
  {
    GError *tmp_error = NULL;
    gchar *instance = pctl_player_get_instance(player);

    gboolean can_go_next = FALSE;
    g_object_get(player, "can-go-previous", &can_go_next, NULL);

    if (!can_go_next)
    {
      g_debug("%s: player cannot go previous", instance);
      return;
    }

    playerctl_player_previous(player, &tmp_error);
    if (tmp_error)
    {
      g_propagate_error(error, tmp_error);
      return;
    }
    return;
  }
}

class Button;
static std::list<Button *> sButtons;

class Button
{
public:
  enum ButtonEvent
  {
    PRESS,
    RELEASE
  };

  Button(int pin) : mPin(pin)
  {
  }

  void begin()
  {
    pinMode(mPin, INPUT_PULLUP);
    mLastState = digitalRead(mPin);
    sButtons.push_back(this);
    attachInterrupt(mPin, isr, CHANGE);
  }

  void process()
  {
    int reading = digitalRead(mPin);

    if (reading == mLastState)
    {
      return;
    }

    if ((millis() - mLastDebounceTime) > DEBOUNCE_DELAY)
    {
      if (reading != mState)
      {
        mState = reading;

        std::lock_guard<std::mutex> lock(mMutex);
        mQueue.push(reading == LOW ? PRESS : RELEASE);
        mCondition.notify_one();
      }
    }

    // NOTE: this means that the first state change out of the debounce period is taken into account,
    //       it should be the last one
    mLastDebounceTime = millis();
    mLastState = reading;
  }

  ButtonEvent waitForEvent(void)
  {
    std::unique_lock<std::mutex> lock(mMutex);
    while (mQueue.empty())
    {
      mCondition.wait(lock);
    }
    ButtonEvent val = mQueue.front();
    mQueue.pop();
    return val;
  }

  bool hasEvent(void)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return !mQueue.empty();
  }

private:
  static void isr()
  {
    for (auto button : sButtons)
    {
      button->process();
    }
  }

  std::mutex mMutex;
  std::condition_variable mCondition;
  std::queue<ButtonEvent> mQueue;
  int mPin;
  int mState = HIGH;
  int mLastState = HIGH;
  unsigned long mLastDebounceTime = 0;
  static const unsigned long DEBOUNCE_DELAY = 50;
};

Button btnLeft(BUTTON_LEFT_PIN);
Button btnRight(BUTTON_RIGHT_PIN);

MFRC522_SPI spiDevice = MFRC522_SPI(SS_PIN, RST_PIN);
MFRC522 mfrc522 = MFRC522(&spiDevice);
NfcAdapter nfc = NfcAdapter(&mfrc522);

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

void nfcThreadEntry()
{
  String lastUid;

  SPI.begin();
  mfrc522.PCD_Init();
  nfc.begin();

  while (1)
  {
    if (nfc.tagPresent())
    {
      NfcTag tag = nfc.read();
      String uid = tag.getUidString();
      if (uid != lastUid)
      {
        Console.print("New tag detected: ");
        Console.println(uid);

        if (tag.hasNdefMessage())
        {
          NdefMessage message = tag.getNdefMessage();
          NdefRecord record = message.getRecord(0);
          String uri = record.asUri();
          if (uri.startsWith("https://open.spotify.com/"))
          {
            uri.replace("https://open.spotify.com/", "spotify:");
            uri.replace("/", ":");
            spotifyProcess(SpotifyCommand(SpotifyCommand::PLAY, uri));
          }
        }
      }

      lastUid = uid;
    }
  }
}

void spotifyProcess(class SpotifyCommand cmd)
{
  PlayerctlPlayerManager *manager = NULL;
  GError *error = NULL;
  GList *available_players = NULL;

  manager = playerctl_player_manager_new(&error);
  if (error != NULL)
  {
    g_printerr("Could not connect to players: %s\n", error->message);
  }

  g_object_get(manager, "player-names", &available_players, NULL);
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
      printf("open %s\n", cmd.payload.c_str());
      playercmd_open(player, (gchar *)cmd.payload.c_str(), &error);
      break;
    case SpotifyCommand::PLAY_PAUSE:
      playercmd_play_pause(player, &error);
      break;
    case SpotifyCommand::NEXT:
      playercmd_next(player, &error);
      break;
    case SpotifyCommand::PREVIOUS:
      playercmd_previous(player, &error);
      break;
    }

    g_object_unref(player);

    // TODO: loop not necessary
    break;
  }
}

SAppMenu menu;

char *menuItems[] =
    {
        "Play/Pause",
        "Next",
        "Previous",
        "Bluetooth",
};

void setup()
{
  // c.f. https://github.com/me-no-dev/RasPiArduino/issues/101
  // NOTE: this stuff all dates back to 2019, is there no more recent option?
  btnLeft.begin();
  btnRight.begin();

  // c.f. https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
  // p. 75
  // for pin 17: GPIO_PUP_PDN_CNTRL_REG1 (offset 0xe8), bits 3:2
  // for pin 26: GPIO_PUP_PDN_CNTRL_REG1 (offset 0xe8), bits 21:20
  // 0b01 = pull-up
  // TODO: this should be part of bcm2835_registers.h and wiring_digital.c should be adapted
  BCM2835_REG(bcmreg_gpio, 0x00e8) = (BCM2835_REG(bcmreg_gpio, 0x00e8) & ~(0b11 << 2)) | (0b01 << 2);
  BCM2835_REG(bcmreg_gpio, 0x00e8) = (BCM2835_REG(bcmreg_gpio, 0x00e8) & ~(0b11 << 20)) | (0b01 << 20);

  ssd1306_128x64_i2c_init();
  ssd1306_flipVertical(1);
  ssd1306_flipHorizontal(1);
  ssd1306_setFixedFont(ssd1306xled_font6x8);
  ssd1306_clearScreen();

  ssd1306_createMenu(&menu, (const char **)menuItems, sizeof(menuItems) / sizeof(char *));
  ssd1306_showMenu(&menu);

  std::thread nfcThread(nfcThreadEntry);
  nfcThread.detach();
}

void loop()
{
  String uri;

  if (btnLeft.hasEvent() && btnLeft.waitForEvent() == Button::PRESS)
  {
    ssd1306_menuDown(&menu);
    ssd1306_updateMenu(&menu);
  }

  if (btnRight.hasEvent() && btnRight.waitForEvent() == Button::PRESS)
  {
    switch (menu.selection)
    {
    case 0:
      spotifyProcess(SpotifyCommand(SpotifyCommand::PLAY_PAUSE));
      break;
    case 1:
      spotifyProcess(SpotifyCommand(SpotifyCommand::NEXT));
      break;
    case 2:
      spotifyProcess(SpotifyCommand(SpotifyCommand::PREVIOUS));
      break;
    case 3:
      break;
    }
  }
}
