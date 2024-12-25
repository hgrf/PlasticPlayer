#ifndef LIBRESPOT_H
#define LIBRESPOT_H

int librespot_init(void);
int librespot_send_cmd(const char *cmd);
void librespot_deinit(void);

#endif // LIBRESPOT_H
