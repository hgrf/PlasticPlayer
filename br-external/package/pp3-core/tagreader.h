#ifndef TAGREADER_H
#define TAGREADER_H

#include <stdint.h>

int tag_reader_init(void (*ndef_msg_cb)(uint8_t *msg, unsigned int len), void (*tag_removed_cb)(void));
void tag_reader_deinit(void);

#endif // TAGREADER_H
