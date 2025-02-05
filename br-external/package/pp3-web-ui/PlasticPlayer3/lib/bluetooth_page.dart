import 'dart:async';

import 'package:dio/dio.dart';
import 'package:flutter/material.dart';

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
  static const _baseUri = 'http://10.42.0.1:9000/';
  bool _isScanning = false;
  late Timer _refreshTimer;
  List<BluetoothDevice> _devices = [];
  final dio = Dio();

  Future<void> _refreshState() async {
    final resp = await dio.get("bluetooth/is_scanning");
    final devices = await dio.get("bluetooth/devices");
    setState(() {
      _isScanning = resp.data == 1;
      _devices = (devices.data as List<dynamic>)
          .map((d) => BluetoothDevice.fromJson(d))
          .toList();
    });
  }

  Future<void> _scan() async {
    await dio.post("bluetooth/scan");
  }

  Future<void> _stopScan() async {
    await dio.post("bluetooth/stop_scan");
  }

  Future<void> _connect(BluetoothDevice device) async {
    await dio.post("bluetooth/connect/${device.address}");
  }

  Future<void> _disconnect(BluetoothDevice device) async {
    await dio.post("bluetooth/disconnect/${device.address}");
  }

  Future<void> _pair(BluetoothDevice device) async {
    await dio.post("bluetooth/pair/${device.address}");
  }

  Future<void> _unpair(BluetoothDevice device) async {
    await dio.post("bluetooth/unpair/${device.address}");
  }

  void _timerCallback(Timer timer) {
    _refreshState();
  }

  @override
  void initState() {
    super.initState();

    dio.options.baseUrl = _baseUri;
    dio.interceptors.add(
      InterceptorsWrapper(
        onResponse: (response, handler) {
          if (response.statusCode != 200) {
            ScaffoldMessenger.of(context)
                .showSnackBar(SnackBar(content: Text("Error: $response")));
          } else {
            return handler.next(response);
          }
        },
        onError: (DioException error, ErrorInterceptorHandler handler) {
          ScaffoldMessenger.of(context)
              .showSnackBar(SnackBar(content: Text("Error: $error")));
        },
      ),
    );

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
        appBar: AppBar(title: const Text('Bluetooth')),
        floatingActionButton: FloatingActionButton(
            onPressed: _isScanning ? _stopScan : _scan,
            child: Text(_isScanning ? 'Stop' : 'Scan')),
        body: Center(
            child: SizedBox(
                width: 600,
                child: Column(
                  children: [
                    if (_isScanning)
                      const Padding(
                          padding: EdgeInsets.all(20),
                          child: Row(
                              mainAxisAlignment: MainAxisAlignment.center,
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                CircularProgressIndicator(),
                                SizedBox(width: 20),
                                Text("Scanning...")
                              ])),
                    _devices.isEmpty
                        ? const Padding(
                            padding: EdgeInsets.all(20),
                            child: Row(
                                mainAxisAlignment: MainAxisAlignment.center,
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  Icon(Icons.info),
                                  SizedBox(width: 10),
                                  Text("No devices found")
                                ]))
                        : Expanded(
                            child: ListView(
                            children: _devices
                                .map((device) => ListTile(
                                    title: Text(device.name),
                                    subtitle: Text(device.address),
                                    leading: device.isConnected
                                        ? const Icon(Icons.bluetooth_connected)
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
                                          const SizedBox(width: 10),
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
                          ))
                  ],
                ))));
  }
}
