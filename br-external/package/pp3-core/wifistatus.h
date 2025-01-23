#ifndef WIFISTATUS_H
#define WIFISTATUS_H

typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_ERROR,
} wifi_status_t;

wifi_status_t get_wifi_status(void);

#endif // WIFISTATUS_H
