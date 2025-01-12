import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:http/http.dart';

class BluetoothPage extends StatefulWidget {
  const BluetoothPage({super.key});

  @override
  State<StatefulWidget> createState() => _BluetoothPageState();
}

class BluetoothDevice {
  final String name;
  final String address;
  final bool isPaired;
  final bool isConnected;

  BluetoothDevice(this.name, this.address, this.isPaired, this.isConnected);

  factory BluetoothDevice.fromJson(dynamic json) {
    final obj = json as Map<String, dynamic>? ?? {};
    return BluetoothDevice(obj['Name'] ?? "No name", obj['Address'],
        obj['Paired'] == 1, obj['Connected'] == 1);
  }
}

class _BluetoothPageState extends State<BluetoothPage> {
  static const _baseUri = 'http://10.42.0.1:9000';
  bool _isScanning = false;
  late Timer _refreshTimer;
  List<BluetoothDevice> _devices = [];

  Future<void> _refreshState() async {
    final resp = await get(Uri.parse("$_baseUri/bluetooth/is_scanning"));
    final devices = await get(Uri.parse("$_baseUri/bluetooth/devices"));
    setState(() {
      _isScanning = resp.body == '1';
      _devices = (jsonDecode(devices.body) as List<dynamic>)
          .map((d) => BluetoothDevice.fromJson(d))
          .toList();
    });
  }

  Future<void> _scan() async {
    await post(Uri.parse("$_baseUri/bluetooth/scan"));
  }

  Future<void> _stopScan() async {
    await post(Uri.parse("$_baseUri/bluetooth/stop_scan"));
  }

  Future<void> _connect(BluetoothDevice device) async {
    await post(Uri.parse("$_baseUri/bluetooth/connect/${device.address}"));
  }

  Future<void> _disconnect(BluetoothDevice device) async {
    await post(Uri.parse("$_baseUri/bluetooth/disconnect/${device.address}"));
  }

  Future<void> _pair(BluetoothDevice device) async {
    await post(Uri.parse("$_baseUri/bluetooth/pair/${device.address}"));
  }

  Future<void> _unpair(BluetoothDevice device) async {
    await post(Uri.parse("$_baseUri/bluetooth/unpair/${device.address}"));
  }

  void _timerCallback(Timer timer) {
    _refreshState();
  }

  @override
  void initState() {
    super.initState();
    _refreshTimer = Timer.periodic(const Duration(seconds: 1), _timerCallback);
    _refreshState();
  }

  @override
  void dispose() {
    _refreshTimer.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
        appBar: AppBar(
          backgroundColor: Colors.blue,
          title: const Text('Bluetooth'),
        ),
        floatingActionButton: FloatingActionButton(
            onPressed: _isScanning ? _stopScan : _scan, child:  Text(_isScanning ? 'Stop' : 'Scan')),
        body: SingleChildScrollView(
            child: Center(
                child: SizedBox(
                    width: 600,
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        const Text("Available devices:"),
                        if (_isScanning)
                          const Row(mainAxisSize: MainAxisSize.min, children: [
                            CircularProgressIndicator(),
                            SizedBox(width: 20),
                            Text("Scanning...")
                          ]),
                        _devices.isEmpty
                            ? const Text("No devices found")
                            : Column(
                                children: _devices
                                    .map((device) => ListTile(
                                        title: Text(device.name),
                                        subtitle: Text(device.address),
                                        leading: device.isConnected
                                            ? const Icon(
                                                Icons.bluetooth_connected)
                                            : const Icon(Icons.bluetooth),
                                        trailing: Row(
                                            mainAxisSize: MainAxisSize.min,
                                          children: [
                                          if (device.isConnected)
                                            ElevatedButton(
                                              onPressed: () =>
                                                  _disconnect(device),
                                              child: const Text("Disconnect"),
                                            )
                                          else
                                            ElevatedButton(
                                              onPressed: () => _connect(device),
                                              child: const Text("Connect"),
                                            ),
                                          if (device.isPaired)
                                            ElevatedButton(
                                              onPressed: () => _unpair(device),
                                              child: const Text("Unpair"),
                                            )
                                          else
                                            ElevatedButton(
                                              onPressed: () => _pair(device),
                                              child: const Text("Pair"),
                                            )
                                        ])))
                                    .toList(),
                              ),
                      ],
                    )))));
  }
}
