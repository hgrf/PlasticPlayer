import 'dart:async';

import 'package:dio/dio.dart';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:http_parser/http_parser.dart';
import 'package:intl/intl.dart';

final dio = Dio();

class FirmwarePage extends StatefulWidget {
  const FirmwarePage({super.key});

  @override
  State<FirmwarePage> createState() => _FirmwarePageState();
}

class _State {
  Widget display() {
    return const SizedBox(width: 0, height: 0);
  }
}

class _InitialState extends _State {
  @override
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
        const Text("Uploading...",
            style: TextStyle(fontWeight: FontWeight.bold)),
        const SizedBox(height: 5),
        LinearProgressIndicator(
          value: currentBytes / totalBytes,
        ),
        const SizedBox(height: 5),
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
        const Text("Installing...",
            style: TextStyle(fontWeight: FontWeight.bold)),
        const SizedBox(height: 5),
        LinearProgressIndicator(
          value: progress / 100,
        ),
        const SizedBox(height: 5),
        Text("Progress: $progress%"),
        Text(message),
      ],
    );
  }
}

class _FirmwarePageState extends State<FirmwarePage> {
  static const _baseUri = 'http://10.42.0.1:9000';
  List<dynamic> _firmwareStatus = [];
  late Timer _refreshTimer;
  _State _state = _InitialState();

  Future<void> _getFirmwareStatus() async {
    final resp = await dio.get("$_baseUri/firmware/status");
    setState(() => _firmwareStatus = resp.data);
  }

  Future<void> _refreshInstallingState() async {
    var resp = await dio.get("$_baseUri/firmware/install_progress");
    setState(() {
      _state = _InstallingState.fromApiResponse(resp.data);
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
    );
    if (result == null) {
      return;
    }
    final formData = FormData.fromMap({
      'file': MultipartFile.fromBytes(result.files.single.bytes!,
          filename: result.files.single.name,
          contentType: MediaType('application', 'octet-stream'))
    });
    await dio.post(
      "$_baseUri/firmware/upload",
      data: formData,
      onSendProgress: (sent, total) {
        setState(() {
          _state = _UploadingState(currentBytes: sent, totalBytes: total);
        });
      },
    );
    _state = _InstallingState(progress: 0, message: "Starting installation");
    await dio.post("$_baseUri/firmware/install",
        data: {"filename": result.files.single.name});
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

  Widget _raucSlotToSubtitle(dynamic slot) {
    final installed = DateTime.parse(slot["installed.timestamp"]).toLocal();
    final installedStr = DateFormat('yyyy-MM-dd HH:mm:ss').format(installed);
    return Text(
        "Version: ${slot["bundle.version"]}  Installed: $installedStr  ${(slot["boot-status"] != null) ? "Boot status: ${slot["boot-status"]}" : ""}");
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
                const Text("Firmware slots:",
                    style:
                        TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                ..._firmwareStatus.map((e) => ListTile(
                      title: Text(e[0].toString() +
                          (e[1]["state"] == "booted" ? " (active)" : "")),
                      subtitle: _raucSlotToSubtitle(e[1]),
                    )),
                const SizedBox(height: 10),
                if (_state is _InitialState)
                  ElevatedButton(
                    child: const Text("Upload firmware update"),
                    onPressed: () => _uploadFirmware(),
                  )
                else
                  _state.display(),
                const SizedBox(height: 10),
                if (_state is _InstallingState &&
                    (_state as _InstallingState).progress == 100)
                  ElevatedButton(
                    child: const Text("Reboot"),
                    onPressed: () async {
                      await dio.post("$_baseUri/reboot");
                    },
                  ),
              ],
            )),
      ),
    );
  }
}
