#ifndef UI_H
#define UI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

int ui_init(void);
void ui_process(void);
void ui_update_track_info(const char *artists, const char *album, const char *title);
void ui_led_on(void);
void ui_led_off(void);
void ui_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // UI_H
