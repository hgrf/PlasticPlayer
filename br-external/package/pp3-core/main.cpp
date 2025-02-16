#include <iostream>
#include <signal.h>

#include "librespot.h"
#include "tagreader.h"
#include "ui.h"
#include "wifistatus.h"

static bool g_is_running = true;
static char stdout_buffer[1024];
static char stderr_buffer[1024];

static void signal_handler(int signum) {
    g_is_running = false;
}

int main(int argc, char *argv[]) {
    int res;

    signal(SIGINT, signal_handler);

    setvbuf(stdout, stdout_buffer, _IOLBF, sizeof(stdout_buffer));
    setvbuf(stderr, stderr_buffer, _IOLBF, sizeof(stderr_buffer));

    res = tag_reader_init();
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
