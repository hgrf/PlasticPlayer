#include <algorithm>
#include <cstdint>
#include <iostream>
#include <signal.h>

#include <ndef-lite/message.hpp>

#include "librespot.h"
#include "tagreader.h"
#include "ui.h"
#include "wifistatus.h"

static bool g_is_running = true;
static char stdout_buffer[1024];
static char stderr_buffer[1024];
static std::string g_current_uri;

static void signal_handler(int signum) {
    g_is_running = false;
}

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

static void ndef_msg_cb(uint8_t *raw_msg, unsigned int len) {
    NDEFMessage msg;
    try {
        msg = NDEFMessage::from_bytes(std::vector<uint8_t>(raw_msg, raw_msg + len), 0);
    } catch (const std::exception& e) {
        std::cout << "failed to parse NDEF message: " << e.what() << std::endl;
        return;
    }

    std::cout << "NDEF message has " << msg.record_count() << " records" << std::endl;
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
}

static void tag_removed_cb(void) {
    ui_led_off();
    g_current_uri.clear();
    librespot_send_cmd("pause", false);
}

int main(int argc, char *argv[]) {
    int res;

    signal(SIGINT, signal_handler);

    setvbuf(stdout, stdout_buffer, _IOLBF, sizeof(stdout_buffer));
    setvbuf(stderr, stderr_buffer, _IOLBF, sizeof(stderr_buffer));

    res = tag_reader_init(ndef_msg_cb, tag_removed_cb);
    if (res != 0) {
        return 1;
    }

    res = ui_init();
    if (res != 0) {
        tag_reader_deinit();
        return 1;
    }

    res = librespot_init();
    if (res != 0) {
        ui_deinit();
        tag_reader_deinit();
        return 1;
    }

    while(g_is_running) {
        ui_process();
    }

    librespot_deinit();

    ui_deinit();

    tag_reader_deinit();
    
    return 0;
}
