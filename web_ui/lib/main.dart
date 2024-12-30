import 'dart:async';
import 'dart:convert';

import 'package:convert/convert.dart';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:http/http.dart';
import 'package:http_parser/http_parser.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flutter Demo',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        useMaterial3: true,
      ),
      home: const MyHomePage(title: 'Flutter Demo Home Page'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
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

class ProgressMultipartRequest extends MultipartRequest {
  final void Function(int bytes, int totalBytes) onProgress;

  ProgressMultipartRequest(
    super.method,
    super.url, {
    required this.onProgress,
  });

  @override
  ByteStream finalize() {
    final byteStream = super.finalize();

    final total = contentLength;
    int bytes = 0;

    final t = StreamTransformer.fromHandlers(
      handleData: (List<int> data, EventSink<List<int>> sink) {
        bytes += data.length;
        onProgress(bytes, total);
        if (total >= bytes) {
          sink.add(data);
        }
      },
    );
    final stream = byteStream.transform(t);
    return ByteStream(stream);
  }
}

class _MyHomePageState extends State<MyHomePage> {
  static const _baseUri = 'http://10.42.0.1:9000';
  WifiState _wifiState = WifiState('unknown', null);
  bool _isScanning = false;
  List<WifiNetwork> _scanResults = [];
  List<WifiNetwork> _knownNetworks = [];
  late Timer _refreshTimer;
  List<dynamic> _firmwareStatus = [];
  FilePickerStatus? _filePickerStatus;
  int? _progressBytes;
  int? _totalBytes;

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

  Future<void> _getFirmwareStatus() async {
    final resp = await get(Uri.parse("$_baseUri/firmware/status"));
    setState(() {
      _firmwareStatus = jsonDecode(resp.body);
    });
  }

  Future<void> _getKnownNetworks() async {
    final resp = await get(Uri.parse("$_baseUri/wifi/known_networks"));
    _knownNetworks = (jsonDecode(resp.body) as List<dynamic>)
        .map((e) => WifiNetwork.fromApiResult(e))
        .toList();
  }

  void _timerCallback(Timer timer) {
    _refreshState();
  }

  @override
  void initState() {
    super.initState();
    _refreshTimer = Timer.periodic(const Duration(seconds: 1), _timerCallback);
    _getFirmwareStatus();
    _getKnownNetworks();
    _refreshState();
  }

  @override
  void dispose() {
    _refreshTimer.cancel();
    super.dispose();
  }

  Future<void> _scan() async {
    await post(Uri.parse('$_baseUri/wifi/scan'));
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        title: Text(widget.title),
      ),
      body: Center(
        child: SizedBox(
            width: 600,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: <Widget>[
                const Text("RAUC slots"),
                ..._firmwareStatus
                    .map((e) => ListTile(
                          leading: e[1]["state"] == "booted"
                              ? const Icon(Icons.check)
                              : const SizedBox(width: 0),
                          title: Text(e[0].toString()),
                          subtitle: Text(e[1].toString()),
                        ))
                    ,
                ElevatedButton(
                  child: const Text("Upload firmware update"),
                  onPressed: () async {
                    final result = await FilePicker.platform.pickFiles(
                        type: FileType.custom,
                        allowedExtensions: ["raucb"],
                        onFileLoading: (status) {
                          setState(() {
                            _filePickerStatus = status;
                          });
                        });
                    if (result == null) {
                      return;
                    }
                    if (!context.mounted) {
                      return;
                    }
                    showDialog(
                        context: context,
                        builder: (context) => AlertDialog(
                              title:
                                  Text("Uploading ${result.files.single.name}"),
                              content: Column(
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  LinearProgressIndicator(
                                    value: _progressBytes! / _totalBytes!,
                                  ),
                                  Text(
                                      "Progress: ${_progressBytes! ~/ 1024 ~/ 1024} / ${_totalBytes! ~/ 1024 ~/ 1024} MB"),
                                ],
                              ),
                            ));
                    var request = ProgressMultipartRequest(
                      "POST",
                      Uri.parse("$_baseUri/firmware/upload"),
                      onProgress: (bytes, totalBytes) {
                        setState(() {
                          _progressBytes = bytes;
                          _totalBytes = totalBytes;
                        });
                      },
                    );
                    request.files.add(MultipartFile.fromBytes(
                        'file', result.files.single.bytes!,
                        filename: result.files.single.name,
                        contentType: MediaType('application', 'octet-stream')));
                    await request.send();
                  },
                ),
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
                    onTap: () => !_scanResults.any((e) => e.id == network.id)
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
                                headers: {"Content-Type": "application/json"},
                                body: jsonEncode({
                                  "id": network.id,
                                  "passphrase": passphrase
                                }));
                            _getKnownNetworks();
                          }
                        })),
              ],
            )),
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: _isScanning ? null : _scan,
        tooltip: 'Scan',
        child: const Text('Scan'),
      ),
    );
  }
}
