#ifndef BT_H
#define BT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct {
    char *id;   //< library-internal identifier (in this case DBus object path)
    char *name; //< user-friendly name
    char *mac;  //< MAC address
    bool connected;
} bt_device_t;

/**
 * @brief      Get a list of Bluetooth devices. The caller is responsible for
 *             freeing the memory allocated for the device list.
 * @param[out] devices points to a pointer to the device list
 * @return     number of devices, or -1 on error
 */
int bt_device_list_get(bt_device_t **devices);

/**
 * @brief      Free a list of Bluetooth devices.
 * @param[in]  devices points to the device list
 * @param[in]  count is the number of devices
 */
void bt_device_list_free(bt_device_t *devices, int count);

/**
 * @brief      Connect to a Bluetooth device.
 * @param[in]  id is the device identifier
 * @return     status code (0 on success, -1 on error)
 */
int bt_device_connect(const char *id);

/**
 * @brief      Checks if we are connected to at least one bluetooth device.
 * @return     bool
 */
bool bt_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // BT_H
