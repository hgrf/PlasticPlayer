import dbus

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


bus = dbus.SystemBus()
manager = dbus.Interface(bus.get_object('net.connman.iwd', '/'),
                         'org.freedesktop.DBus.ObjectManager')
devpath = "/net/connman/iwd/0/3"
device = dbus.Interface(bus.get_object("net.connman.iwd", devpath),
                                "net.connman.iwd.Station")


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

@app.post("/wifi/forget")
async def wifi_forget(network: WifiNetwork):
    id = "/net/connman/iwd/" + network.id
    network = dbus.Interface(bus.get_object("net.connman.iwd", id),
                                "net.connman.iwd.KnownNetwork")
    network.Forget()
    return ""

@app.post("/wifi/register")
async def wifi_register(network: WifiNetwork):
    # NOTE: this could be done in a more thorough way using an agent, but
    # that requires additional dependencies (GLib mainloop)
    # https://git.kernel.org/pub/scm/network/wireless/iwd.git/tree/test/simple-agent
    id = network.id.rstrip("_psk")
    with open(f"/var/lib/iwd/={id}.psk", "w") as f:
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

@app.get("/firmware/status")
async def firmware_get_status():
    installer = dbus.Interface(bus.get_object("de.pengutronix.rauc", '/'),
                                "de.pengutronix.rauc.Installer")
    return installer.GetSlotStatus()

@app.post("/firmware/upload")
async def firmware_upload(file: UploadFile):
    with open("/upload/" + file.filename, "wb") as f:
        f.write(file.file.read())
    return ""
