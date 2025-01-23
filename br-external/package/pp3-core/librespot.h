#ifndef LIBRESPOT_H
#define LIBRESPOT_H

#include <stdbool.h>

int librespot_init(void);
int librespot_send_cmd(const char *cmd, bool with_response);
void librespot_deinit(void);

#endif // LIBRESPOT_H
