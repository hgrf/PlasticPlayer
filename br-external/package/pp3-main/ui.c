#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <gpiod.h>
#include <menu.h>

#include "librespot.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define GPIO_DEVICE_NAME "/dev/gpiochip0"
#define GPIO_DEVICE_LINE_BTN_LEFT 26
#define GPIO_DEVICE_LINE_BTN_RIGHT 23
#define DEBOUNCE_PERIOD_MS 200

static unsigned int g_line_offsets[] = { GPIO_DEVICE_LINE_BTN_LEFT, GPIO_DEVICE_LINE_BTN_RIGHT };
static unsigned long g_last_ts[ARRAY_SIZE(g_line_offsets)];
static struct gpiod_chip *gs_chip;
static struct gpiod_line_bulk gs_lines;
static struct gpiod_line_bulk gs_evt_lines;

static int g_fd;
static FILE *g_file;
static SCREEN *g_scr;
static MENU *g_menu;
static WINDOW *g_menu_win;
static ITEM **g_menu_items;

struct menu_item {
    const char *name;
    void (*callback)(void);
};

static const char *no_description = "";

static void menu_action_play_pause(void) {
    printf("Menu action: play/pause\n");
    librespot_send_cmd("play_pause\n");
}

static void menu_action_next(void) {
    printf("Menu action: next\n");
    librespot_send_cmd("next\n");
}

static void menu_action_prev(void) {
    printf("Menu action: previous\n");
    librespot_send_cmd("previous\n");
}

static void menu_action_bt(void) {
    printf("Menu action: bluetooth\n");
}

static const struct menu_item choices[] = {
    { "Play/Pause", menu_action_play_pause },
    { "Next", menu_action_next },
    { "Previous", menu_action_prev },
    { "Bluetooth", menu_action_bt },
};

static int btn_init(void) {
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

    if (gpiod_line_request_bulk_falling_edge_events_flags(&gs_lines, "gpiointerrupt", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0)
    {
        perror("gpio: set edge events failed.\n");
        gpiod_chip_close(gs_chip);
        return -1;
    }

    return 0;
}

static void btn_deinit(void) {
    gpiod_chip_close(gs_chip);
}

static int menu_init(void) {
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
    g_menu_items = (ITEM **)calloc(ARRAY_SIZE(choices), sizeof(ITEM *));
    for(i = 0; i < ARRAY_SIZE(choices); i++) {
        g_menu_items[i] = new_item(choices[i].name, no_description);
        set_item_userptr(g_menu_items[i], (void *) choices[i].callback);
    }

	/* Crate menu */
	g_menu = new_menu((ITEM **)g_menu_items);

	/* Create the window to be associated with the menu */
    g_menu_win = newwin(8, 16, 0, 0);
     
	/* Set main window and sub window */
    set_menu_win(g_menu, g_menu_win);
    set_menu_sub(g_menu, derwin(g_menu_win, 6, 14, 1, 1));

	/* Set menu mark */
    set_menu_mark(g_menu, ">");

	/* Print a border around the main window */
    box(g_menu_win, 0, 0);
	refresh();
        
	/* Post the menu */
	post_menu(g_menu);
	wrefresh(g_menu_win);

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
    if (btn_init() < 0)
        return -1;

    if (menu_init() < 0) {
        btn_deinit();
        return -1;
    }

    return 0;
}

void ui_process(void) {
    int i, j, res;
    int line_idx = -1;
    unsigned long ts;
    struct gpiod_line_event event;
    ITEM *cur;
    void (*p)(void);

    res = gpiod_line_event_wait_bulk(&gs_lines, NULL, &gs_evt_lines);
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
        }
    }
}

void ui_deinit(void) {
    menu_deinit();    
    btn_deinit();
}
