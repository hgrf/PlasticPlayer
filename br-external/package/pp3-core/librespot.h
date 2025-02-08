#ifndef LIBRESPOT_H
#define LIBRESPOT_H

#include <stdbool.h>

typedef enum {
    LIBRESPOT_STATUS_UNAVAILABLE = 0,
    LIBRESPOT_STATUS_PAUSED,
    LIBRESPOT_STATUS_PLAYING,
} librespot_status_t;

int librespot_init(void);
int librespot_send_cmd(const char *cmd, bool with_response);
librespot_status_t librespot_get_status(void);
void librespot_deinit(void);

#endif // LIBRESPOT_H
