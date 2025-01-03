// credit https://blog.linumiz.com/archives/16537

#include "wifistatus.h"

#include <gio/gio.h>
#include <stdio.h>

wifi_status_t get_wifi_status(void)
{
    wifi_status_t ret = WIFI_STATUS_ERROR;
    GDBusConnection *connection;
    GError *error = NULL;
    GVariant *result;
    GVariant *value;
    const char *state;
    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error != NULL) {
        g_printerr("Error connecting to system bus: %s\n", error->message);
        g_error_free(error);
        return ret;
    }
    result = g_dbus_connection_call_sync(
            connection,
            "net.connman.iwd",                      /* Bus name */
            "/net/connman/iwd/0/3",                 /* Object path of the adapter */
            "org.freedesktop.DBus.Properties",      /* Interface name */
            "Get",                                  /* Method name */
            g_variant_new("(ss)", "net.connman.iwd.Station", "State"), /* Parameters */
            G_VARIANT_TYPE("(v)"),                  /* Expected return type */
            G_DBUS_CALL_FLAGS_NONE,
            -1,                                     /* Default timeout */
            NULL,                                   /* GCancellable */
            &error
            );
    if (error != NULL) {
        g_printerr("Error calling Get method: %s\n", error->message);
        g_error_free(error);
        g_object_unref(connection);
        return ret;
    }
    g_variant_get(result, "(v)", &value);
    g_variant_get(value, "s", &state);
    if (g_strcmp0(state, "connected") == 0) {
        ret = WIFI_STATUS_CONNECTED;
    } else if (g_strcmp0(state, "disconnected") == 0) {
        ret = WIFI_STATUS_DISCONNECTED;
    } else if (g_strcmp0(state, "connecting") == 0) {
        ret = WIFI_STATUS_CONNECTING;
    }
    g_variant_unref(value);
    g_variant_unref(result);
    g_object_unref(connection);

    return ret;
}
