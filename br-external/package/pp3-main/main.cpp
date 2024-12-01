#include <iostream>

#include <driver_ntag21x_basic.h>
#include <ndef-lite/message.hpp>

#define NDEF_START_PAGE 4
#define NDEF_START_OFFSET 2
#define PAGE_SIZE 4

/**
 * @brief      read page and print data
 * @param[in]  page is the read page
 * @param[out] *data points to a data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 */
int read_page(uint8_t page, uint8_t data[4]) {
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
int read_pages(uint8_t start_page, uint8_t page_count, uint8_t *data) {
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
NDEFMessage search_and_read_tag() {
    uint8_t res;
    uint8_t i;
    uint8_t id[8];
    uint8_t data[496];
    uint8_t len;
    uint8_t page_count;
    ntag21x_capability_container_t type_s;

    res = ntag21x_basic_search(&type_s, id, 50);
    if (res != 0) {
        ntag21x_interface_debug_print("ntag21x: search failed: %d\n", res);
        return NDEFMessage();
    }
    if (type_s != NTAG21X_CAPABILITY_CONTAINER_496_BYTE_NTAG215) {
        ntag21x_interface_debug_print("ntag21x: unsupported card.\n");
        return NDEFMessage();
    }

    ntag21x_interface_debug_print("ntag21x: id is ");
    for (i = 0; i < 8; i++) {
        ntag21x_interface_debug_print("0x%02X ", id[i]);
    }
    ntag21x_interface_debug_print("\n");

    // read message length from page 4, byte 0 should be 0x03, byte 1 is the length
    // c.f. https://github.com/TheNitek/NDEF/blob/f72cf58a705ead36c1014d091da9dcb71670cdb7/src/MifareUltralight.cpp#L127
    res = read_page(NDEF_START_PAGE, data);
    if (res != 0) {
        ntag21x_interface_debug_print("ntag21x: read failed: %d\n", res);
        return NDEFMessage();
    }
    if (data[0] != 0x03) {
        ntag21x_interface_debug_print("ntag21x: invalid NDEF message start byte: 0x%02X\n", data[0]);
        return NDEFMessage();
    }

    len = data[1];
    page_count = len / PAGE_SIZE + (len % PAGE_SIZE ? 1 : 0);
    res = read_pages(NDEF_START_PAGE + 1, page_count - 1, data + PAGE_SIZE);
    if (res != 0) {
        ntag21x_interface_debug_print("ntag21x: read failed: %d\n", res);
        return NDEFMessage();
    }

    try {
        auto msg = NDEFMessage::from_bytes(std::vector<uint8_t>(data + NDEF_START_OFFSET, data + NDEF_START_OFFSET + len), 0);
        return msg;
    } catch (const std::exception& e) {
        ntag21x_interface_debug_print("ntag21x: failed to parse NDEF message: %s\n", e.what());
        return NDEFMessage();
    }
}

int main(int argc, char *argv[]) {
    uint8_t res;

    res = ntag21x_basic_init();
    if (res != 0) {
        return 1;
    }

    for(;;) {
        NDEFMessage msg = search_and_read_tag();
        if (msg.is_valid()) {
            std::cout << "NDEF message is valid and has " << msg.record_count() << " records" << std::endl;
            if (msg.record_count() > 0) {
                std::cout << "Record 1 URI: " << msg.record(0).get_uri() << std::endl;
            }
        } else {
            std::cout << "NDEF message is invalid" << std::endl;
        }
    }
    
    (void)ntag21x_basic_deinit();
    
    return 0;
}
