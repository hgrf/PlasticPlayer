#include "bt.h"

#include <gio/gio.h>

static unsigned int devices_iter(GVariant *managed_objects, bt_device_t *devices) {
    unsigned int ret = 0;
    GVariantIter object_iter;
    gchar *object_path;
    GVariant *interfaces;
    GVariantIter interfaces_iter;
    gchar *interface_name;
    GVariant *interface_properties;
    GVariantIter properties_iter;
    gchar *property_name;
    GVariant *property_value;

    g_variant_iter_init(&object_iter, managed_objects);
    while (g_variant_iter_next(&object_iter, "{o@a{sa{sv}}}", &object_path, &interfaces)) {
        g_variant_iter_init(&interfaces_iter, interfaces);
        while (g_variant_iter_next(&interfaces_iter, "{s@a{sv}}", &interface_name, &interface_properties)) {
            if (strcmp(interface_name, "org.bluez.Device1") == 0) {
                if (devices != NULL) {
                    g_variant_iter_init(&properties_iter, interface_properties);
                    while (g_variant_iter_next(&properties_iter, "{sv}", &property_name, &property_value)) {
                        devices[ret].id = object_path;
                        if (strcmp(property_name, "Name") == 0) {
                            g_variant_get(property_value, "s", &devices[ret].name);
                        }
                        if (strcmp(property_name, "Address") == 0) {
                            g_variant_get(property_value, "s", &devices[ret].mac);
                        }
                        g_variant_unref(property_value);
                        g_free(property_name);
                    }
                }
                ret++;
            }
            g_variant_unref(interface_properties);
            g_free(interface_name);
        }
        g_variant_unref(interfaces);
        if (devices == NULL) {
            g_free(object_path);
        }
    }

    return ret;
}

int bt_device_list_get(bt_device_t **devices) {
    GDBusConnection *connection;
    GError *error = NULL;
    GVariant *result;
    GVariant *managed_objects;
    int ret;
    bt_device_t *devs;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error != NULL) {
        g_printerr("Error connecting to system bus: %s\n", error->message);
        g_error_free(error);
        return -1;
    }
    result = g_dbus_connection_call_sync(
            connection,
            "org.bluez",                            /* Bus name */
            "/",
            "org.freedesktop.DBus.ObjectManager",   /* Interface name */
            "GetManagedObjects",                    /* Method name */
            NULL,                                   /* Parameters */
            G_VARIANT_TYPE("(a{oa{sa{sv}}})"),      /* Expected return type */
            G_DBUS_CALL_FLAGS_NONE,
            -1,                                     /* Default timeout */
            NULL,                                   /* GCancellable */
            &error
            );
    if (error != NULL) {
        g_printerr("Error calling GetManagedObjects method: %s\n", error->message);
        g_error_free(error);
        g_object_unref(connection);
        return -1;
    }
    managed_objects = g_variant_get_child_value(result, 0);

    /* first, get the number of devices */
    ret = devices_iter(managed_objects, NULL);
    if (ret > 0) {
        devs = (bt_device_t *) calloc(ret, sizeof(bt_device_t));
        if (devs == NULL) {
            g_printerr("Error allocating memory\n");
            g_variant_unref(managed_objects);
            g_variant_unref(result);
            g_object_unref(connection);
            return -1;
        }
        devices_iter(managed_objects, devs);
        *devices = devs;
    } else {
        *devices = NULL;
    }

    g_variant_unref(managed_objects);
    g_variant_unref(result);
    g_object_unref(connection);

    return ret;
}

void bt_device_list_free(bt_device_t *devices, int count) {
    int i;

    for (i = 0; i < count; i++) {
        g_free(devices[i].id);
        g_free(devices[i].name);
        g_free(devices[i].mac);
    }
    free(devices);
}

int bt_device_connect(const char *id) {
    GDBusConnection *connection;
    GError *error = NULL;
    GVariant *result;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error != NULL) {
        g_printerr("Error connecting to system bus: %s\n", error->message);
        g_error_free(error);
        return -1;
    }
    result = g_dbus_connection_call_sync(
            connection,
            "org.bluez",                            /* Bus name */
            id,                                     /* Object path */
            "org.bluez.Device1",                    /* Interface name */
            "Connect",                              /* Method name */
            NULL,                                   /* Parameters */
            NULL,                                   /* Expected return type */
            G_DBUS_CALL_FLAGS_NONE,
            -1,                                     /* Default timeout */
            NULL,                                   /* GCancellable */
            &error
            );
    if (error != NULL) {
        g_printerr("Error calling Connect method: %s\n", error->message);
        g_error_free(error);
        return -1;
    }
    g_object_unref(connection);

    return 0;
}
