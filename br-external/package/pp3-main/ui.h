#ifndef UI_H
#define UI_H

int ui_init(void);
void ui_process(void);
void ui_update_player_status(const char *status);
void ui_update_track_info(const char *artists, const char *album, const char *title);
void ui_led_on(void);
void ui_led_off(void);
void ui_deinit(void);

#endif // UI_H
