import 'dart:async';
import 'dart:convert';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:http/http.dart';
import 'package:http_parser/http_parser.dart';

class FirmwarePage extends StatefulWidget {
  const FirmwarePage({super.key});

  @override
  State<FirmwarePage> createState() => _FirmwarePageState();
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

class _State {
  Widget display() {
    return const SizedBox(width: 0, height: 0);
  }
}

class _UploadingState extends _State {
  final int currentBytes;
  final int totalBytes;

  _UploadingState({required this.currentBytes, required this.totalBytes});

  @override
  Widget display() {
    return Column(
      children: [
        const Text("Uploading..."),
        LinearProgressIndicator(
          value: currentBytes / totalBytes,
        ),
        Text(
            "Progress: ${currentBytes ~/ 1024 ~/ 1024} / ${totalBytes ~/ 1024 ~/ 1024} MB"),
      ],
    );
  }
}

class _InstallingState extends _State {
  final int progress;
  final String message;

  _InstallingState({required this.progress, required this.message});

  @factory
  static _InstallingState fromApiResponse(dynamic data) {
    return _InstallingState(
      progress: data[0],
      message: data[1],
    );
  }

  @override
  Widget display() {
    return Column(
      children: [
        const Text("Installing..."),
        LinearProgressIndicator(
          value: progress / 100,
        ),
        Text("Progress: $progress%"),
        Text(message),
      ],
    );
  }
}

class _FirmwarePageState extends State<FirmwarePage> {
  static const _baseUri = 'http://10.42.0.1:9000';
  List<dynamic> _firmwareStatus = [];
  FilePickerStatus? _filePickerStatus;
  late Timer _refreshTimer;
  _State _state = _State();

  Future<void> _getFirmwareStatus() async {
    final resp = await get(Uri.parse("$_baseUri/firmware/status"));
    setState(() {
      _firmwareStatus = jsonDecode(resp.body);
    });
  }

  Future<void> _refreshInstallingState() async {
    var resp = await get(Uri.parse("$_baseUri/firmware/install_progress"));
    setState(() {
      _state = _InstallingState.fromApiResponse(jsonDecode(resp.body));
    });
  }

  void _timerCallback(Timer timer) {
    if (_state is _InstallingState) {
      _refreshInstallingState();
    }
  }

  Future<void> _uploadFirmware() async {
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
    var request = ProgressMultipartRequest(
      "POST",
      Uri.parse("$_baseUri/firmware/upload"),
      onProgress: (bytes, totalBytes) {
        setState(() {
          _state = _UploadingState(currentBytes: bytes, totalBytes: totalBytes);
        });
      },
    );
    request.files.add(MultipartFile.fromBytes(
        'file', result.files.single.bytes!,
        filename: result.files.single.name,
        contentType: MediaType('application', 'octet-stream')));
    await request.send();
    _state = _InstallingState(progress: 0, message: "Starting installation");
    await post(Uri.parse("$_baseUri/firmware/install"),
        headers: {"Content-Type": "application/json"},
        body: jsonEncode({
          "filename": result.files.single.name,
        }));
  }

  @override
  void initState() {
    super.initState();
    _refreshTimer = Timer.periodic(const Duration(seconds: 1), _timerCallback);
    _getFirmwareStatus();
  }

  @override
  void dispose() {
    _refreshTimer.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Firmware")),
      body: Center(
        child: SizedBox(
            width: 600,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: <Widget>[
                const Text("RAUC slots"),
                ..._firmwareStatus.map((e) => ListTile(
                      leading: e[1]["state"] == "booted"
                          ? const Icon(Icons.check)
                          : const SizedBox(width: 0),
                      title: Text(e[0].toString()),
                      subtitle: Text(e[1].toString()),
                    )),
                ElevatedButton(
                  child: const Text("Upload firmware update"),
                  onPressed: () => _uploadFirmware(),
                ),
                _state.display(),
                if (_state is _InstallingState &&
                    (_state as _InstallingState).progress == 100)
                  ElevatedButton(
                    child: const Text("Reboot"),
                    onPressed: () async {
                      await post(Uri.parse("$_baseUri/reboot"));
                    },
                  ),
              ],
            )),
      ),
    );
  }
}
