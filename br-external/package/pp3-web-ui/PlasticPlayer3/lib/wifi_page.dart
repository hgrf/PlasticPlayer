import 'dart:async';
import 'dart:convert';

import 'package:convert/convert.dart';
import 'package:flutter/material.dart';
import 'package:http/http.dart';

class WifiPage extends StatefulWidget {
  const WifiPage({super.key});

  @override
  State<StatefulWidget> createState() => _WifiPageState();
}

// TODO: use code generation to generate these classes
class WifiNetwork {
  final String id;
  final String ssid;

  WifiNetwork(this.id, this.ssid);

  @factory
  static WifiNetwork fromApiResult(dynamic result) {
    final id = result[0] as String;
    final ssid =
        String.fromCharCodes(hex.decode(id.split('/').last.split('_').first));
    return WifiNetwork(id, ssid);
  }
}

class WifiState {
  final String state;
  final WifiNetwork? connectedNetwork;

  WifiState(this.state, this.connectedNetwork);

  @factory
  static WifiState fromApiResult(dynamic result) {
    final state = result['state'] as String;
    final connectedNetwork = result['connectedNetwork'] == null
        ? null
        : WifiNetwork.fromApiResult([result['connectedNetwork']]);
    return WifiState(state, connectedNetwork);
  }
}

class _WifiPageState extends State<WifiPage> {
  static const _baseUri = 'http://10.42.0.1:9000';
  WifiState _wifiState = WifiState('unknown', null);
  bool _isScanning = false;
  List<WifiNetwork> _scanResults = [];
  List<WifiNetwork> _knownNetworks = [];
  late Timer _refreshTimer;

  Future<void> _getKnownNetworks() async {
    final resp = await get(Uri.parse("$_baseUri/wifi/known_networks"));
    _knownNetworks = (jsonDecode(resp.body) as List<dynamic>)
        .map((e) => WifiNetwork.fromApiResult(e))
        .toList();
  }

  Future<void> _refreshState() async {
    var resp = await get(Uri.parse("$_baseUri/wifi/state"));
    setState(() {
      _wifiState = WifiState.fromApiResult(jsonDecode(resp.body));
    });

    resp = await get(Uri.parse("$_baseUri/wifi/is_scanning"));
    setState(() {
      _isScanning = resp.body == '1';
    });

    resp = await get(Uri.parse("$_baseUri/wifi/scan_results"));
    setState(() {
      _scanResults = (jsonDecode(resp.body) as List<dynamic>)
          .map((e) => WifiNetwork.fromApiResult(e))
          .toList();
    });
  }

  Future<void> _scan() async {
    await post(Uri.parse('$_baseUri/wifi/scan'));
    setState(() {});
  }

  void _timerCallback(Timer timer) {
    _refreshState();
  }

  @override
  void initState() {
    super.initState();
    _refreshTimer = Timer.periodic(const Duration(seconds: 1), _timerCallback);
    _getKnownNetworks();
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
          title: const Text('Wifi'),
        ),
        body: Center(
            child: SizedBox(
                width: 600,
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Text('Wifi state: ${_wifiState.state}'),
                    if (_wifiState.state == 'connected' ||
                        _wifiState.state == 'connecting') ...[
                      Text("to ${_wifiState.connectedNetwork?.ssid ?? "none"}"),
                      ElevatedButton(
                          onPressed: () =>
                              post(Uri.parse('$_baseUri/wifi/disconnect')),
                          child: const Text('Disconnect'))
                    ],
                    const Text("Known networks:"),
                    ..._knownNetworks.map((network) => ListTile(
                        leading: !_scanResults.any((e) => e.id == network.id)
                            ? const Icon(Icons.wifi_off)
                            : const Icon(Icons.wifi),
                        trailing: ElevatedButton(
                            child: const Text("Forget"),
                            onPressed: () async {
                              await post(Uri.parse('$_baseUri/wifi/forget'),
                                  headers: {"Content-Type": "application/json"},
                                  body: jsonEncode({"id": network.id}));
                              _getKnownNetworks();
                            }),
                        title: Text(network.ssid),
                        onTap: () => !_scanResults
                                .any((e) => e.id == network.id)
                            ? null
                            : post(Uri.parse('$_baseUri/wifi/connect'),
                                headers: {"Content-Type": "application/json"},
                                body: jsonEncode({"id": network.id})))),
                    const Text("Available networks:"),
                    if (_isScanning)
                      const Row(mainAxisSize: MainAxisSize.min, children: [
                        CircularProgressIndicator(),
                        SizedBox(width: 20),
                        Text("Scanning...")
                      ]),
                    ..._scanResults
                        .where((e) => !_knownNetworks.any((n) => e.id == n.id))
                        .map((network) => ListTile(
                            leading: const Icon(Icons.wifi),
                            title: Text(network.ssid),
                            onTap: () async {
                              final passphrase = await showDialog<String>(
                                  context: context,
                                  builder: (context) {
                                    final controller = TextEditingController();
                                    return AlertDialog(
                                      title: const Text('Enter passphrase'),
                                      content: TextField(
                                        controller: controller,
                                      ),
                                      actions: <Widget>[
                                        TextButton(
                                          onPressed: () {
                                            Navigator.of(context)
                                                .pop(controller.text);
                                          },
                                          child: const Text('OK'),
                                        ),
                                      ],
                                    );
                                  });
                              if (passphrase != null) {
                                await post(Uri.parse('$_baseUri/wifi/register'),
                                    headers: {
                                      "Content-Type": "application/json"
                                    },
                                    body: jsonEncode({
                                      "id": network.id,
                                      "passphrase": passphrase
                                    }));
                                _getKnownNetworks();
                              }
                            })),
                    ElevatedButton(
                        onPressed: _isScanning ? null : _scan,
                        child: const Text('Scan')),
                  ],
                ))));
  }
}
