#include "tagreader.h"

#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include <driver_ntag21x_basic.h>
#include <driver_mfrc522_basic.h>

#include "log.h"
static const char *TAG = "nfc";

#define NDEF_START_PAGE 4
#define PAGE_SIZE 4

// c.f. https://blog.eletrogate.com/wp-content/uploads/2018/05/NFCForum-TS-Type-2-Tag_1.1.pdf
typedef enum {
    TLV_TAG_FIELD_NULL,
    TLV_TAG_FIELD_LOCK_CTRL,
    TLV_TAG_FIELD_MEM_CTRL,
    TLV_TAG_FIELD_NDEF,
    TLV_TAG_FIELD_PROPRIETARY = 0xfd,
    TLV_TAG_FIELD_TERMINATOR,
} tlv_tag_field_t;

static pthread_t g_thread;
static bool g_is_running = true;
static mfrc522_handle_t *g_mfrc522_handle;
void (*g_ndef_msg_cb)(uint8_t *msg, unsigned int len);
void (*g_tag_removed_cb)(void);
static int g_tag_remove_counter;

/**
 * @brief      read page and print data
 * @param[in]  page is the read page
 * @param[out] *data points to a data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 */
 static int read_page(uint8_t page, uint8_t data[4]) {
    uint8_t i;
    uint8_t res = ntag21x_basic_read(page, data);

    if (res != 0) {
        LOG_WARNING("read failed: %d", res);
        return 1;
    }

    /* output */
    printf("page 0x%02x: ", page);
    for (i = 0; i < 4; i++) {
        printf("0x%02X ", data[i]);
    }
    for (i = 0; i < 4; i++) {
        if (data[i] < 0x20 || data[i] > 0x7E) {
            printf(".");
        } else {
            printf("%c", data[i]);
        }
    }
    printf("\n");

    return 0;
}

/**
 * @brief      read pages and print data
 * @param[in]  start_page is the start page
 * @param[in]  page_count is the read page count
 * @param[out] *data points to a data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 */
 static int read_pages(uint8_t start_page, uint8_t page_count, uint8_t *data) {
    uint8_t res;
    uint8_t i;

    for (i = 0; i < page_count; i++) {
        res = read_page(start_page + i, data + i * PAGE_SIZE);
        if (res != 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief      search and read tag
 * @return     status code
 *             - 0 success
 *             - 1 failure
 */
 static int search_and_read_tag(uint8_t serial[7]) {
    uint8_t res;
    uint8_t i;
    uint8_t id[8];
    uint8_t data[496];
    uint8_t len;
    uint8_t page;
    uint8_t page_count;
    uint8_t reader_id;
    uint8_t version;
    ntag21x_capability_container_t type_s;

    res = ntag21x_basic_search(&type_s, id, 0);
    if (res != 0) {
        LOG_WARNING("search failed: %d", res);
        if (mfrc522_get_version(g_mfrc522_handle, &reader_id, &version) != 0) {
            LOG_ERROR("failed to get version");
        } else if (reader_id != 0x09) {
            LOG_ERROR("invalid reader ID: 0x%02X", reader_id);
        }
        if (mfrc522_get_error(g_mfrc522_handle, &res) != 0) {
            LOG_ERROR("failed to get error register");
        } else {
            LOG_WARNING("error register: 0x%02X", res);
        }
        return 1;
    }

    printf("serial number is ");
    for (i = 1; i < 8; i++) {
        printf("%02x ", id[i]);
    }
    printf("\n");

    memcpy(serial, id + 1, 7);

    // read message length from page 4, byte 0 should be 0x03, byte 1 is the length
    // c.f. https://github.com/TheNitek/NDEF/blob/f72cf58a705ead36c1014d091da9dcb71670cdb7/src/MifareUltralight.cpp#L127
    res = read_page(NDEF_START_PAGE, data);
    if (res != 0) {
        return 1;
    }

    page = NDEF_START_PAGE;
    i = 0;
    if (data[i] == TLV_TAG_FIELD_LOCK_CTRL) {
        // the value of this TLV consists of 3 bytes, so the next TLV starts on page 5 byte 1
        page++;
        i = 1;
        res = read_page(page, data);
        if (res != 0) {
            return 1;
        }
    }

    if (data[i] != TLV_TAG_FIELD_NDEF) {
        LOG_ERROR("invalid NDEF message start byte: 0x%02X", data[0]);
        return 1;
    }

    i++;
    len = data[i];
    page_count = (len + i + 1) / PAGE_SIZE + ((len + i + 1) % PAGE_SIZE ? 1 : 0);
    page++;
    res = read_pages(page, page_count - 1, data + PAGE_SIZE);
    if (res != 0) {
        return 1;
    }

    g_tag_remove_counter = 0;
    if (g_ndef_msg_cb != NULL) {
        g_ndef_msg_cb(data + i + 1, len);
    }

    return 0;
}

static void *tag_reader_thread_entry(void *) {
    int res;
    uint8_t prev_serial[7] = {0};
    uint8_t serial[7];
    while(g_is_running) {
        if (ntag21x_basic_get_serial_number(serial) == 0) {
            printf("serial number: ");
            for (int i = 0; i < sizeof(serial); i++) {
                printf("%02x", serial[i]);
            }
            printf("\n");
            if (memcmp(prev_serial, serial, sizeof(prev_serial)) == 0) {
                LOG_INFO("tag has not changed");
                usleep(200000);
                continue;
            }
            memcpy(prev_serial, serial, sizeof(prev_serial));
        } else {
            LOG_WARNING("failed to read serial number");
            memset(prev_serial, 0, sizeof(prev_serial));
        }

        res = search_and_read_tag(prev_serial);
        if (res != 0) {
            if (g_tag_remove_counter == 3 && g_tag_removed_cb) {
                g_tag_removed_cb();
            } else if (g_tag_remove_counter < 3) {
                g_tag_remove_counter++;
            }
        }
    }

    return NULL;
}

int tag_reader_init(void (*ndef_msg_cb)(uint8_t *msg, unsigned int len), void (*tag_removed_cb)(void)) {
    int res;

    g_ndef_msg_cb = ndef_msg_cb;
    g_tag_removed_cb = tag_removed_cb;

    res = ntag21x_basic_init();
    if (res != 0) {
        return 1;
    }

    g_mfrc522_handle = mfrc522_basic_get_handle();

    // default minimum reception level is 8 vs. collision level 4
    res = mfrc522_set_min_level(g_mfrc522_handle, 3);
    if (res != 0) {
        (void)ntag21x_basic_deinit();
        return 1;
    }
    res = mfrc522_set_collision_level(g_mfrc522_handle, 1);
    if (res != 0) {
        (void)ntag21x_basic_deinit();
        return 1;
    }

    res = pthread_create(&g_thread, NULL, tag_reader_thread_entry, NULL);
    if (res != 0) {
        (void)ntag21x_basic_deinit();
        return 1;
    }

    return 0;
}

void tag_reader_deinit(void) {
    g_is_running = false;

    pthread_join(g_thread, NULL);

    (void)ntag21x_basic_deinit();
}