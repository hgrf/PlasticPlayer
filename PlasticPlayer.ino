#include <SPI.h>
#include <MFRC522.h>
#include <NfcAdapter.h>
#include <ssd1306.h>

#include "src/button.h"
#include "src/player.h"

#define RST_PIN 25
#define SS_PIN 8

// TODO: debug left pin hardware
#define BUTTON_LEFT_PIN 26
#define BUTTON_RIGHT_PIN 17

Button btnLeft(BUTTON_LEFT_PIN);
Button btnRight(BUTTON_RIGHT_PIN);

MFRC522_SPI spiDevice = MFRC522_SPI(SS_PIN, RST_PIN);
MFRC522 mfrc522 = MFRC522(&spiDevice);
NfcAdapter nfc = NfcAdapter(&mfrc522);

SpotifyPlayer player;

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
            player.process(SpotifyCommand(SpotifyCommand::PLAY, uri));
          }
        }
      }

      lastUid = uid;
    }
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
  player.begin();

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
      player.process(SpotifyCommand(SpotifyCommand::PLAY_PAUSE));
      break;
    case 1:
      player.process(SpotifyCommand(SpotifyCommand::NEXT));
      break;
    case 2:
      player.process(SpotifyCommand(SpotifyCommand::PREVIOUS));
      break;
    case 3:
      break;
    }
  }
}
