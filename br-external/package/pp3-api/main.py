import dbus
import os
import subprocess

from fastapi import FastAPI, File, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

app = FastAPI()

# TODO: use nginx to proxy requests to the API
origins = ["*"]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# for python examples look at:
# c.f. https://git.kernel.org/pub/scm/network/wireless/iwd.git/tree/test/list-known-networks

# to inspect a D-Bus object, use the following command:
# sudo busctl introspect net.connman.iwd /net/connman/iwd/0/3 net.connman.iwd.Station
# sudo busctl introspect net.connman.iwd /net/connman/iwd/0/3/5346525f37343838_psk net.connman.iwd.Network

class WifiNetwork(BaseModel):
    id: str
    passphrase: str = ""


class WifiState(BaseModel):
    state: str
    connectedNetwork: str

class FirmwareFile(BaseModel):
    filename: str


bus = dbus.SystemBus()
manager = dbus.Interface(bus.get_object('net.connman.iwd', '/'),
                         'org.freedesktop.DBus.ObjectManager')
devpath = "/net/connman/iwd/0/3"
device = dbus.Interface(bus.get_object("net.connman.iwd", devpath),
                                "net.connman.iwd.Station")

installer = dbus.Interface(bus.get_object("de.pengutronix.rauc", '/'),
                            "de.pengutronix.rauc.Installer")

adapter = dbus.Interface(bus.get_object("org.bluez", "/org/bluez/hci0"), "org.bluez.Adapter1")
bt_manager = dbus.Interface(bus.get_object("org.bluez", "/"), "org.freedesktop.DBus.ObjectManager")


@app.get("/wifi/state", response_model=WifiState)
async def wifi_get_state():
    state = device.Get("net.connman.iwd.Station", "State", dbus_interface=dbus.PROPERTIES_IFACE)
    try:
        connectedNetwork = device.Get("net.connman.iwd.Station", "ConnectedNetwork", dbus_interface=dbus.PROPERTIES_IFACE)
    except dbus.exceptions.DBusException:
        connectedNetwork = ""
    return WifiState(state=state, connectedNetwork=connectedNetwork)

@app.get("/wifi/known_networks")
async def wifi_get_known_networks():
    networks = []
    for path, interfaces in manager.GetManagedObjects().items():
        if 'net.connman.iwd.KnownNetwork' not in interfaces:
            continue
        network = interfaces['net.connman.iwd.KnownNetwork']
        networks.append([path.split("/")[-1], network])
    return networks

def iwd_file_from_network(network: WifiNetwork):
    id = network.id.rstrip("_psk")
    ssid = bytes.fromhex(id).decode("utf-8")
    useText = True
    for c in ssid:
        if not c.isalnum() and c not in ["-", "_"]:
            useText = False
    if useText:
        id = ssid
    else:
        id = "=" + id
    return f"/data/iwd/{id}.psk"

@app.post("/wifi/forget")
async def wifi_forget(network: WifiNetwork):
    # NOTE: due to limited access rights of the iwd service, we need
    #       to remove the file manually
    # id = "/net/connman/iwd/" + network.id
    # network = dbus.Interface(bus.get_object("net.connman.iwd", id),
    #                             "net.connman.iwd.KnownNetwork")
    # network.Forget()
    os.remove(iwd_file_from_network(network))
    return ""

@app.post("/wifi/register")
async def wifi_register(network: WifiNetwork):
    # NOTE: this could be done in a more thorough way using an agent, but
    # that requires additional dependencies (GLib mainloop)
    # https://git.kernel.org/pub/scm/network/wireless/iwd.git/tree/test/simple-agent
    with open(iwd_file_from_network(network), "w") as f:
        f.write("[Security]\n")
        f.write(f"Passphrase={network.passphrase}\n")
    return ""

# TODO: how to handle dbus response timeout? (it blocks everything...)
@app.post("/wifi/connect")
async def wifi_connect(network: WifiNetwork):
    id = devpath + "/" + network.id
    print("Connecting to", id)
    network = dbus.Interface(bus.get_object("net.connman.iwd", id),
                                "net.connman.iwd.Network")
    network.Connect()
    return ""

@app.post("/wifi/disconnect")
async def wifi_disconnect():
    try:
        device.Disconnect()
    except dbus.exceptions.DBusException:
        # TODO: handle exception (operation failed - when connecting)
        pass
    return ""

@app.post("/wifi/scan")
async def wifi_scan():
    try:
        device.Scan()
    except dbus.exceptions.DBusException:
        # TODO: handle exception (operation in progress)
        pass
    return ""

@app.get("/wifi/is_scanning")
async def wifi_is_scanning():
    return device.Get("net.connman.iwd.Station", "Scanning", dbus_interface=dbus.PROPERTIES_IFACE)

@app.get("/wifi/scan_results")
async def wifi_get_scan_results():
    devices = []
    for d in device.GetOrderedNetworks():
        devices.append([d[0].split("/")[-1], d[1]])
    return devices

@app.post("/bluetooth/scan")
async def bluetooth_scan():
    # c.f. https://github.com/bluez/bluez/blob/5.79/lib/uuid.h#L33
    adapter.SetDiscoveryFilter({"UUIDs": ["0000110b-0000-1000-8000-00805f9b34fb"]})
    adapter.StartDiscovery()
    return ""

@app.post("/bluetooth/stop_scan")
async def bluetooth_stop_scan():
    adapter.StopDiscovery()
    return ""

@app.post("/bluetooth/pair/{mac}")
async def bluetooth_pair(mac: str):
    mac = mac.replace(":", "_")
    device = dbus.Interface(bus.get_object("org.bluez", f"/org/bluez/hci0/dev_{mac}"), "org.bluez.Device1")
    device.Set("org.bluez.Device1", "Trusted", True, dbus_interface=dbus.PROPERTIES_IFACE)
    device.Pair()
    return ""

@app.post("/bluetooth/unpair/{mac}")
async def bluetooth_unpair(mac: str):
    mac = mac.replace(":", "_")
    adapter.RemoveDevice(f"/org/bluez/hci0/dev_{mac}")
    return ""

@app.post("/bluetooth/connect/{mac}")
async def bluetooth_connect(mac: str):
    mac = mac.replace(":", "_")
    device = dbus.Interface(bus.get_object("org.bluez", f"/org/bluez/hci0/dev_{mac}"), "org.bluez.Device1")
    device.Connect()
    return ""

@app.post("/bluetooth/disconnect/{mac}")
async def bluetooth_disconnect(mac: str):
    mac = mac.replace(":", "_")
    device = dbus.Interface(bus.get_object("org.bluez", f"/org/bluez/hci0/dev_{mac}"), "org.bluez.Device1")
    device.Disconnect()
    return ""

@app.get("/bluetooth/is_scanning")
async def bluetooth_is_scanning():
    return adapter.Get("org.bluez.Adapter1", "Discovering", dbus_interface=dbus.PROPERTIES_IFACE)

@app.get("/bluetooth/devices")
async def bluetooth_get_devices():
    objs = bt_manager.GetManagedObjects()
    devices = []
    for path, interfaces in objs.items():
        if "org.bluez.Device1" in interfaces:
            device = interfaces["org.bluez.Device1"]
            devices.append(device)
    return devices

@app.get("/firmware/status")
async def firmware_get_status():
    return installer.GetSlotStatus()

@app.post("/firmware/upload")
async def firmware_upload(file: UploadFile):
    with open("/upload/" + file.filename, "wb") as f:
        f.write(file.file.read())
    return ""

@app.post("/firmware/install")
async def firmware_install(file: FirmwareFile):
    path = "/upload/" + file.filename
    installer.InstallBundle(path, [])
    return ""

@app.get("/firmware/install_progress")
async def firmware_install_progress():
    return installer.Get("de.pengutronix.rauc.Installer", "Progress", dbus_interface=dbus.PROPERTIES_IFACE)

@app.post("/reboot")
async def reboot():
    subprocess.run(["sudo", "reboot"])
    return ""
