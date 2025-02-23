#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <gpiod.h>
#include <menu.h>

#include "bt.h"
#include "icons.h"
#include "librespot.h"
#include "wifistatus.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define GPIO_DEVICE_NAME "/dev/gpiochip0"
#define GPIO_DEVICE_LINE_BTN_LEFT 26
#define GPIO_DEVICE_LINE_BTN_RIGHT 3
#define DEBOUNCE_PERIOD_MS 200

#define GPIO_DEVICE_LINE_LED 4

static unsigned int g_line_offsets[] = { GPIO_DEVICE_LINE_BTN_LEFT, GPIO_DEVICE_LINE_BTN_RIGHT };
static unsigned long g_last_ts[ARRAY_SIZE(g_line_offsets)];
static unsigned long g_last_ts_all;
static struct gpiod_chip *gs_chip;
static struct gpiod_line_bulk gs_lines;
static struct gpiod_line_bulk gs_evt_lines;
static struct gpiod_line *gs_led_line;

#define WINDOW_WIDTH 16
#define SCROLL_PADDING 5
#define MENU_TIMEOUT_MS 3000
typedef enum {
    CURRENT_MENU_NONE,
    CURRENT_MENU_MAIN,
    CURRENT_MENU_BT,
} current_menu_t;
static current_menu_t g_current_menu = CURRENT_MENU_NONE;
static int g_fd;
static FILE *g_file;
static SCREEN *g_scr;
static MENU *g_menu;
static MENU *g_menu_bt;
static WINDOW *g_menu_win;
static WINDOW *g_status_win;
static ITEM **g_menu_items;
static unsigned int g_bt_devices_count;
static bt_device_t *g_bt_devices;
static ITEM **g_menu_bt_items;

pthread_mutex_t g_mutex;
static char *g_artists;
static char *g_album;
static char *g_title;

struct menu_item {
    const char *name;
    void (*callback)(void);
};

static const char *no_description = "";

static unsigned wifi_status_to_icon(wifi_status_t status) {
    switch(status) {
        case WIFI_STATUS_CONNECTED:
            return ICON_WIFI_CONNECTED;
        case WIFI_STATUS_CONNECTING:
            return ICON_WIFI_CONNECTING;
        case WIFI_STATUS_DISCONNECTED:
        default:
            return ICON_WIFI_DISCONNECTED;
    }
}

static unsigned librespot_status_to_icon(librespot_status_t status) {
    switch(status) {
        case LIBRESPOT_STATUS_PLAYING:
            return ICON_PLAY;
        case LIBRESPOT_STATUS_PAUSED:
            return ICON_PAUSE;
        case LIBRESPOT_STATUS_UNAVAILABLE:
        default:
            return ICON_WARNING;
    }
}

static void menu_action_play_pause(void) {
    printf("Menu action: play/pause\n");
    librespot_send_cmd("play_pause\n", false);
    /* hide menu */
    g_last_ts_all = 0;
}

static void menu_action_next(void) {
    printf("Menu action: next\n");
    librespot_send_cmd("next\n", false);
    /* hide menu */
    g_last_ts_all = 0;
}

static void menu_action_prev(void) {
    printf("Menu action: previous\n");
    librespot_send_cmd("previous\n", false);
    /* hide menu */
    g_last_ts_all = 0;
}

static void menu_bt_init(void) {
    // TODO: what if device name is too long?
    int i;
    g_bt_devices_count = bt_device_list_get(&g_bt_devices);
    g_menu_bt_items = (ITEM **)calloc(g_bt_devices_count, sizeof(ITEM *));
    for(i = 0; i < g_bt_devices_count; i++) {
        g_menu_bt_items[i] = new_item(g_bt_devices[i].name, no_description);
        set_item_userptr(g_menu_bt_items[i], (void *) &g_bt_devices[i]);
    }
	g_menu_bt = new_menu((ITEM **)g_menu_bt_items);
    set_menu_win(g_menu_bt, g_menu_win);
    set_menu_sub(g_menu_bt, derwin(g_menu_win, 6, 14, 1, 1));
    set_menu_mark(g_menu_bt, ">");
}

static void menu_bt_deinit(void) {
    int i;
    unpost_menu(g_menu_bt);
    free_menu(g_menu_bt);
    for(i = 0; i < g_bt_devices_count; i++)
        free_item(g_menu_bt_items[i]);
    bt_device_list_free(g_bt_devices, g_bt_devices_count);
}

static void menu_action_bt(void) {
    // TODO: append button to go back
    // TODO: append button for scanning
    printf("Menu action: bluetooth\n");
    unpost_menu(g_menu);
    menu_bt_init();
    post_menu(g_menu_bt);
    wrefresh(g_menu_win);
    g_current_menu = CURRENT_MENU_BT;
}

static void menu_action_power_off(void) {
    printf("Menu action: power off\n");
    if (system("sudo halt") < 0) {
        perror("failed to halt system");
    }
}

static const struct menu_item choices[] = {
    { "Play/Pause", menu_action_play_pause },
    { "Next", menu_action_next },
    { "Previous", menu_action_prev },
    { "Bluetooth", menu_action_bt },
    { "Power off", menu_action_power_off },
};

static int btn_led_init(void) {
    gs_chip = gpiod_chip_open(GPIO_DEVICE_NAME);
    if (gs_chip == NULL)
    {
        perror("gpio: open failed.\n");
        return -1;
    }
    
    if (gpiod_chip_get_lines(gs_chip, g_line_offsets, ARRAY_SIZE(g_line_offsets), &gs_lines) < 0) 
    {
        perror("gpio: get lines failed.\n");
        gpiod_chip_close(gs_chip);
        return -1;
    }

    gs_led_line = gpiod_chip_get_line(gs_chip, GPIO_DEVICE_LINE_LED);
    if (gs_led_line == NULL)
    {
        perror("gpio: get line failed.\n");
        gpiod_chip_close(gs_chip);
        return -1;
    }

    if (gpiod_line_request_output(gs_led_line, "led", 0) < 0) {
        perror("gpio: request output failed.\n");
        gpiod_chip_close(gs_chip);
        return -1;
    }

    if (gpiod_line_request_bulk_falling_edge_events_flags(&gs_lines, "gpiointerrupt", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0)
    {
        perror("gpio: set edge events failed.\n");
        gpiod_chip_close(gs_chip);
        return -1;
    }

    return 0;
}

static void btn_led_deinit(void) {
    gpiod_chip_close(gs_chip);
}

static int ui_menu_init(void) {
    int i;

    g_fd = open("/dev/tty1", O_RDWR);
    if (g_fd < 0)
    {
        perror("open tty1 failed.\n");
        return -1;
    }

    g_file = fdopen(g_fd, "w+");
    if (g_file == NULL)
    {
        close(g_fd);
        perror("fdopen failed.\n");
        return -1;
    }

    g_scr = newterm("vt100", g_file, g_file);
    if (g_scr == NULL)
    {
        fclose(g_file);
        close(g_fd);
        perror("newterm failed.\n");
        return -1;
    }

	start_color();
    cbreak();
    noecho();

	/* Create items */
    g_menu_items = (ITEM **)calloc(ARRAY_SIZE(choices) + 1, sizeof(ITEM *));
    for(i = 0; i < ARRAY_SIZE(choices); i++) {
        g_menu_items[i] = new_item(choices[i].name, no_description);
        set_item_userptr(g_menu_items[i], (void *) choices[i].callback);
    }
    g_menu_items[i] = NULL;

	/* Crate menu */
	g_menu = new_menu((ITEM **)g_menu_items);

	/* Create the window to be associated with the menu */
    g_menu_win = newwin(8, WINDOW_WIDTH, 0, 0);
     
	/* Set main window and sub window */
    set_menu_win(g_menu, g_menu_win);
    set_menu_sub(g_menu, derwin(g_menu_win, 6, 14, 1, 1));

	/* Set menu mark */
    set_menu_mark(g_menu, ">");

    return 0;
}

static void menu_deinit(void) {
    int i;

    unpost_menu(g_menu);
    free_menu(g_menu);
    for(i = 0; i < ARRAY_SIZE(choices); ++i)
        free_item(g_menu_items[i]);
	endwin();

    delscreen(g_scr);
    fclose(g_file);
    close(g_fd);
}

int ui_init(void) {
    pthread_mutex_init(&g_mutex, NULL);

    if (btn_led_init() < 0)
        return -1;

    if (icons_init() < 0) {
        btn_led_deinit();
        return -1;
    }

    if (ui_menu_init() < 0) {
        icons_deinit();
        btn_led_deinit();
        return -1;
    }

    g_status_win = newwin(4, WINDOW_WIDTH, 4, 0);

    return 0;
}

static unsigned long millis(void) {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
}

static void print_scrolling(int row, const char *text)
{
    unsigned int len, scroll_amp;
    int scroll_pos;
    time_t s;
    struct timespec spec;

    if (text == NULL)
        return;

    len = strlen(text);
    if (len <= WINDOW_WIDTH) {
        mvwprintw(g_status_win, row, 0, "%s", text);
        return;
    }

    scroll_amp = len - WINDOW_WIDTH + 2 * SCROLL_PADDING;
    scroll_pos = (millis() / 200) % (2 * scroll_amp) - scroll_amp;
    scroll_pos = abs(scroll_pos);
    scroll_pos -= SCROLL_PADDING;
    if (scroll_pos < 0)
        scroll_pos = 0;
    if (scroll_pos > len - WINDOW_WIDTH)
        scroll_pos = len - WINDOW_WIDTH;
    mvwprintw(g_status_win, row, 0, "%.16s", &text[scroll_pos]);
}

void ui_process(void) {
    int i, j, res, conn_res;
    int line_idx = -1;
    unsigned long ts;
    struct gpiod_line_event event;
    ITEM *cur;
    void (*p)(void);
    bt_device_t *bt_device;
    struct timespec timeout = { 0, 200000000 };
    wifi_status_t wifi_status;
    librespot_status_t librespot_status;

    res = gpiod_line_event_wait_bulk(&gs_lines, &timeout, &gs_evt_lines);
    if (res == 1)
    {
        for (i = 0; i < gs_evt_lines.num_lines; i++) {
            for (j = 0; j < gs_lines.num_lines; j++) {
                if (gs_evt_lines.lines[i] == gs_lines.lines[j]) {
                    line_idx = j;
                    break;
                }
            }

            if (line_idx < 0)
                continue;

            if (gpiod_line_event_read(gs_evt_lines.lines[i], &event) != 0)
                continue;

            if (event.event_type != GPIOD_LINE_EVENT_FALLING_EDGE)
                continue;

            ts = event.ts.tv_sec * 1000 + event.ts.tv_nsec / 1000000;
            if (ts < g_last_ts[line_idx] + DEBOUNCE_PERIOD_MS)
                continue;
            g_last_ts[line_idx] = ts;
            g_last_ts_all = millis();

            if (g_current_menu == CURRENT_MENU_MAIN) {
                if (g_line_offsets[line_idx] == GPIO_DEVICE_LINE_BTN_LEFT) {
                    if (current_item(g_menu) == g_menu_items[ARRAY_SIZE(choices) - 1])
                        menu_driver(g_menu, REQ_FIRST_ITEM);
                    else
                        menu_driver(g_menu, REQ_DOWN_ITEM);
                    wrefresh(g_menu_win);
                } else if (g_line_offsets[line_idx] == GPIO_DEVICE_LINE_BTN_RIGHT) {
                    cur = current_item(g_menu);
                    p = (void (*)()) item_userptr(cur);
                    p();
                }
            } else if (g_current_menu == CURRENT_MENU_BT) {
                if (g_line_offsets[line_idx] == GPIO_DEVICE_LINE_BTN_LEFT) {
                    if (current_item(g_menu_bt) == g_menu_bt_items[g_bt_devices_count - 1])
                        menu_driver(g_menu_bt, REQ_FIRST_ITEM);
                    else
                        menu_driver(g_menu_bt, REQ_DOWN_ITEM);
                    wrefresh(g_menu_win);
                } else if (g_line_offsets[line_idx] == GPIO_DEVICE_LINE_BTN_RIGHT) {
                    cur = current_item(g_menu_bt);
                    bt_device = (bt_device_t *) item_userptr(cur);
                    printf("Connecting to %s\n", bt_device->id);
                    werase(g_menu_win);
                    mvwprintw(g_menu_win, 0, 0, "Connecting to");
                    mvwprintw(g_menu_win, 1, 0, "%s", bt_device->name);
                    wrefresh(g_menu_win);
                    conn_res = bt_device_connect(bt_device->id);
                    if (conn_res != 0) {
                        werase(g_menu_win);
                        mvwprintw(g_menu_win, 0, 0, "Connection");
                        mvwprintw(g_menu_win, 1, 0, "failed");
                        wrefresh(g_menu_win);
                    } else {
                        werase(g_menu_win);
                        mvwprintw(g_menu_win, 0, 0, "Connected to");
                        mvwprintw(g_menu_win, 1, 0, "%s", bt_device->name);
                        wrefresh(g_menu_win);
                    }
                    sleep(2);
                    /* hide menu in next iteration */
                    g_last_ts_all = 0;
                }
            } else {
                clearok(g_menu_win, TRUE);
                box(g_menu_win, 0, 0);
                post_menu(g_menu);
                wrefresh(g_menu_win);

                g_current_menu = CURRENT_MENU_MAIN;
            }
        }
    } else if (res == 0 && millis() > g_last_ts_all + MENU_TIMEOUT_MS) {
        /* timeout occured, show status screen */
        if (g_current_menu == CURRENT_MENU_BT) {
            menu_bt_deinit();
        }
        librespot_status = librespot_get_status();
        icons_put(0, 0, librespot_status_to_icon(librespot_status));
        icons_put(75, 0, bt_is_connected() ? ICON_BLUETOOTH_CONNECTED : ICON_BLUETOOTH_DISCONNECTED);
        wifi_status = get_wifi_status();
        icons_put(100, 0, wifi_status_to_icon(wifi_status));
        icons_sync();
        
        werase(g_status_win);
        pthread_mutex_lock(&g_mutex);
        print_scrolling(0, g_artists);
        print_scrolling(1, g_album);
        print_scrolling(2, g_title);
        wrefresh(g_status_win);
        pthread_mutex_unlock(&g_mutex);

        g_current_menu = CURRENT_MENU_NONE;
    }
}

void ui_update_track_info(const char *artists, const char *album, const char *title) {
    pthread_mutex_lock(&g_mutex);
    if (g_artists != NULL) {
        free(g_artists);
        g_artists = NULL;
    }
    if (g_album != NULL) {
        free(g_album);
        g_album = NULL;
    }
    if (g_title != NULL) {
        free(g_title);
        g_title = NULL;
    }
    if (artists != NULL)
        g_artists = strdup(artists);
    if (album != NULL)
        g_album = strdup(album);
    if (title != NULL)
        g_title = strdup(title);
    pthread_mutex_unlock(&g_mutex);
}

void ui_led_on(void) {
    if (gpiod_line_set_value(gs_led_line, 1) < 0) {
        perror("gpio: set value failed.\n");
    }
}

void ui_led_off(void) {
    if (gpiod_line_set_value(gs_led_line, 0) < 0) {
        perror("gpio: set value failed.\n");
    }
}

void ui_deinit(void) {
    icons_deinit();
    menu_deinit();
    btn_led_deinit();
    pthread_mutex_destroy(&g_mutex);

    if (g_artists != NULL) {
        free(g_artists);
        g_artists = NULL;
    }
    if (g_album != NULL) {
        free(g_album);
        g_album = NULL;
    }
    if (g_title != NULL) {
        free(g_title);
        g_title = NULL;
    }
}
