#include "librespot.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#include "ui.h"

#define LIBRESPOT_PORT 42069
#define LIBRESPOT_EVENT_PORT 42070

static int g_server_fd;
static pthread_t g_thread_id;
static volatile bool g_is_running = true;
static char g_buffer[1024] = {0};

static void librespot_parse_event(char *data) {
    size_t len = strlen(data);
    char *event = NULL;
    char *album = NULL;
    char *artists = NULL;
    char *title = NULL;
    int cur_line_idx = 0;
    char *line;
    for (int i = 0; i < len; i++) {
        if (data[i] == '\n') {
            data[i] = '\0';
            cur_line_idx = i + 1;
        }

        if (cur_line_idx >= 0) {
            line = &data[cur_line_idx];
            if (strncmp(line, "PLAYER_EVENT=", sizeof("PLAYER_EVENT=") - 1) == 0) {
                event = &line[sizeof("PLAYER_EVENT=") - 1];
            } else if (strncmp(line, "ALBUM=", sizeof("ABLUM=") - 1) == 0) {
                album = &line[sizeof("ALBUM=") - 1];
            } else if (strncmp(line, "ARTISTS=", sizeof("ARTISTS=") - 1) == 0) {
                artists = &line[sizeof("ARTISTS=") - 1];
            } else if (strncmp(line, "NAME=", sizeof("NAME=") - 1) == 0) {
                title = &line[sizeof("NAME=") - 1];
            }
            cur_line_idx = -1;
        }
    }

    if (event != NULL && strcmp(event, "track_changed") == 0) {
        ui_update_track_info(artists, album, title);
    } else if (event != NULL && strcmp(event, "paused") == 0) {
        ui_update_player_status("Paused");
    } else if (event != NULL && strcmp(event, "playing") == 0) {
        ui_update_player_status("Playing");
    }
}

static void* librespot_thread_entry(void *) {
    int res, socket;
    ssize_t valread;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    struct timeval tv;
    fd_set rfds;
    while (g_is_running) {
        g_buffer[0] = '\0';
        FD_ZERO(&rfds);
        FD_SET(g_server_fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        res = select(g_server_fd + 1, &rfds, NULL, NULL, &tv);
        if (res == 0) {
            /* timeout */
            continue;
        } else if (res < 0) {
            perror("select");
            continue;
        }
        if ((socket = accept(g_server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
            perror("accept");
            continue;
        }
        valread = read(socket, g_buffer, sizeof(g_buffer) - 1);
        if (valread < 0) {
            perror("read");
            close(socket);
            continue;
        }
        g_buffer[valread] = '\0';
        librespot_parse_event(g_buffer);
        close(socket);
    }

    return NULL;
}

int librespot_init(void) {
    struct sockaddr_in address;
    int opt = 1;

    if ((g_server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        return -1;
    }

    if (setsockopt(g_server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        close(g_server_fd);
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(LIBRESPOT_EVENT_PORT);

    if (bind(g_server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(g_server_fd);
        return -1;
    }
    if (listen(g_server_fd, 3) < 0) {
        perror("listen");
        close(g_server_fd);
        return -1;
    }

    pthread_create(&g_thread_id, NULL, librespot_thread_entry, NULL);

    return 0;
}

int librespot_send_cmd(const char *cmd) {
    int status, valread, client_fd;
    struct sockaddr_in serv_addr;
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(LIBRESPOT_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/address not supported\n");
        close(client_fd);
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        printf("Connection failed\n");
        close(client_fd);
        return -1;
    }
    send(client_fd, cmd, strlen(cmd), 0);

    close(client_fd);
    return 0;
}

void librespot_deinit(void) {
    g_is_running = false;
    pthread_join(g_thread_id, NULL);
    close(g_server_fd);
}
