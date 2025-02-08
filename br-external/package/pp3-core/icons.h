#ifndef ICONS_H
#define ICONS_H

#define ICON_WIFI_CONNECTED 0xe63e
#define ICON_WIFI_CONNECTING 0xe4d9
#define ICON_WIFI_DISCONNECTED 0xe648
#define ICON_BLUETOOTH_CONNECTED 0xe1a8
#define ICON_BLUETOOTH_DISCONNECTED 0xe1a9
#define ICON_PLAY 0xe037
#define ICON_PAUSE 0xe034
#define ICON_WARNING 0xe002

int icons_init(void);
void icons_put(int x, int y, unsigned icon);
void icons_clear(void);
void icons_deinit(void);

#endif // ICONS_H
