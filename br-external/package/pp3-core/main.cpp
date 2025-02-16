#include <algorithm>
#include <iostream>
#include <signal.h>
#include <thread>

#include <driver_ntag21x_basic.h>
#include <driver_mfrc522_basic.h>
#include <ndef-lite/message.hpp>

#include "librespot.h"
#include "ui.h"
#include "wifistatus.h"

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

static bool g_is_running = true;
static mfrc522_handle_t *g_mfrc522_handle;
static char stdout_buffer[1024];
static char stderr_buffer[1024];

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
        ntag21x_interface_debug_print("ntag21x: read failed: %d\n", res);
        return 1;
    }

    /* output */
    ntag21x_interface_debug_print("ntag21x: read page %d: ", page);
    for (i = 0; i < 4; i++) {
        ntag21x_interface_debug_print("0x%02X ", data[i]);
    }
    for (i = 0; i < 4; i++) {
        if (data[i] < 0x20 || data[i] > 0x7E) {
            ntag21x_interface_debug_print(".");
        } else {
            ntag21x_interface_debug_print("%c", data[i]);
        }
    }
    ntag21x_interface_debug_print("\n");

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
 * @return     NDEF message
 */
static NDEFMessage search_and_read_tag(uint8_t serial[7]) {
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
        ntag21x_interface_debug_print("ntag21x: search failed: %d\n", res);
        if (mfrc522_get_version(g_mfrc522_handle, &reader_id, &version) != 0) {
            ntag21x_interface_debug_print("ntag21x: failed to get version\n");
        } else if (reader_id != 0x09) {
            ntag21x_interface_debug_print("ntag21x: invalid reader ID: 0x%02X\n", reader_id);
        }
        if (mfrc522_get_error(g_mfrc522_handle, &res) != 0) {
            ntag21x_interface_debug_print("ntag21x: failed to get error register\n");
        } else {
            ntag21x_interface_debug_print("ntag21x: error register: 0x%02X\n", res);
        }
        return NDEFMessage();
    }

    ntag21x_interface_debug_print("ntag21x: serial number is ");
    for (i = 1; i < 8; i++) {
        ntag21x_interface_debug_print("%02x ", id[i]);
    }
    ntag21x_interface_debug_print("\n");

    memcpy(serial, id + 1, 7);

    // read message length from page 4, byte 0 should be 0x03, byte 1 is the length
    // c.f. https://github.com/TheNitek/NDEF/blob/f72cf58a705ead36c1014d091da9dcb71670cdb7/src/MifareUltralight.cpp#L127
    res = read_page(NDEF_START_PAGE, data);
    if (res != 0) {
        ntag21x_interface_debug_print("ntag21x: read failed: %d\n", res);
        return NDEFMessage();
    }

    page = NDEF_START_PAGE;
    i = 0;
    if (data[i] == TLV_TAG_FIELD_LOCK_CTRL) {
        // the value of this TLV consists of 3 bytes, so the next TLV starts on page 5 byte 1
        page++;
        i = 1;
        res = read_page(page, data);
        if (res != 0) {
            ntag21x_interface_debug_print("ntag21x: read failed: %d\n", res);
            return NDEFMessage();
        }
    }

    if (data[i] != TLV_TAG_FIELD_NDEF) {
        ntag21x_interface_debug_print("ntag21x: invalid NDEF message start byte: 0x%02X\n", data[0]);
        return NDEFMessage();
    }

    i++;
    len = data[i];
    page_count = (len + i + 1) / PAGE_SIZE + ((len + i + 1) % PAGE_SIZE ? 1 : 0);
    page++;
    res = read_pages(page, page_count - 1, data + PAGE_SIZE);
    if (res != 0) {
        ntag21x_interface_debug_print("ntag21x: read failed: %d\n", res);
        return NDEFMessage();
    }

    try {
        auto msg = NDEFMessage::from_bytes(std::vector<uint8_t>(data + i + 1, data + i + 1 + len), 0);
        return msg;
    } catch (const std::exception& e) {
        ntag21x_interface_debug_print("ntag21x: failed to parse NDEF message: %s\n", e.what());
        return NDEFMessage();
    }
}

static std::string g_current_uri;
static unsigned int g_tag_remove_counter;

static void load_uri(const std::string& uri) {
    const std::string base_uri = "https://open.spotify.com/";
    if (uri.substr(0, base_uri.length()) != base_uri) {
        std::cout << "URI does not start with " << base_uri << std::endl;
        return;
    }

    if (uri == g_current_uri) {
        return;
    }
    g_current_uri = uri;

    ui_led_on();
    std::string cmd = "load spotify:" + uri.substr(base_uri.length());
    std::replace(cmd.begin(), cmd.end(), '/', ':');
    std::cout << "cmd: " << cmd << std::endl;
    librespot_send_cmd(cmd.c_str(), false);
}

static void tag_reader_thread_entry(void) {
    uint8_t prev_serial[7] = {0};
    uint8_t serial[7];
    while(g_is_running) {
        if (ntag21x_basic_get_serial_number(serial) == 0) {
            printf("Serial number: ");
            for (int i = 0; i < sizeof(serial); i++) {
                printf("%02x", serial[i]);
            }
            printf("\n");
            if (memcmp(prev_serial, serial, sizeof(prev_serial)) == 0) {
                printf("Tag has not changed.\n");
                usleep(200000);
                continue;
            }
            memcpy(prev_serial, serial, sizeof(prev_serial));
        } else {
            printf("Failed to read serial number.\n");
            memset(prev_serial, 0, sizeof(prev_serial));
        }

        NDEFMessage msg = search_and_read_tag(prev_serial);
        if (msg.is_valid()) {
            std::cout << "NDEF message is valid and has " << msg.record_count() << " records" << std::endl;
            g_tag_remove_counter = 0;
            if (msg.record_count() > 0) {
                // c.f. https://github.com/hgrf/NDEF/commit/4c3133f6830fbe595c937db7a17c7a65eb487a82
                // c.f. https://github.com/hgrf/NDEF/commit/e035efd38d2deb2f6b94301faa5937c59c1dba61
                // c.f. https://www.oreilly.com/library/view/beginning-nfc/9781449324094/ch04.html
                // c.f. https://www.oreilly.com/library/view/beginning-nfc/9781449324094/apa.html
                // c.f. https://berlin.ccc.de/~starbug/felica/NFCForum-SmartPoster_RTD_1.0.pdf
                const auto &rec = msg.record(0);
                if (rec.type().name() == "U") {
                    auto uri = msg.record(0).get_uri_protocol() + msg.record(0).get_uri();
                    std::cout << "Record 1 URI: " << uri << std::endl;
                    load_uri(uri);
                } else if (rec.type().name() == "Sp") {
                    auto sp_msg = NDEFMessage::from_bytes(rec.payload(), 0);
                    if (sp_msg.is_valid()) {
                        std::cout << "Record 1 Smart Poster is valid and has " << sp_msg.record_count() << " records" << std::endl;
                        if (sp_msg.record_count() > 0) {
                            const auto &sp_rec = sp_msg.record(0);
                            if (sp_rec.type().name() == "U") {
                                auto uri = sp_msg.record(0).get_uri_protocol() + sp_msg.record(0).get_uri();
                                std::cout << "Record 1 Smart Poster URI: " << uri << std::endl;
                                load_uri(uri);
                            } else {
                                std::cout << "Record 1 Smart Poster type: " << sp_rec.type().name() << std::endl;
                            }
                        }
                    }
                } else {
                    std::cout << "Record 1 type: " << rec.type().name() << std::endl;
                }
            }
        } else {
            std::cout << "NDEF message is invalid" << std::endl;
            g_tag_remove_counter++;
            if (g_tag_remove_counter >= 3) {
                ui_led_off();
                g_current_uri.clear();
                librespot_send_cmd("pause", false);
            }
        }
    }
}

static void signal_handler(int signum) {
    g_is_running = false;
}

int main(int argc, char *argv[]) {
    uint8_t res;

    signal(SIGINT, signal_handler);

    setvbuf(stdout, stdout_buffer, _IOLBF, sizeof(stdout_buffer));
    setvbuf(stderr, stderr_buffer, _IOLBF, sizeof(stderr_buffer));

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

    res = ui_init();
    if (res != 0) {
        (void)ntag21x_basic_deinit();
        return 1;
    }

    res = librespot_init();
    if (res != 0) {
        ui_deinit();
        (void)ntag21x_basic_deinit();
        return 1;
    }

    std::thread tag_reader_thread(tag_reader_thread_entry);

    while(g_is_running) {
        ui_process();
    }

    tag_reader_thread.join();

    librespot_deinit();

    ui_deinit();

    (void)ntag21x_basic_deinit();
    
    return 0;
}
