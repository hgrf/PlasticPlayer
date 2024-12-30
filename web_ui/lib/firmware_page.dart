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

class _FirmwarePageState extends State<FirmwarePage> {
  static const _baseUri = 'http://10.42.0.1:9000';
  List<dynamic> _firmwareStatus = [];
  FilePickerStatus? _filePickerStatus;
  int? _progressBytes;
  int? _totalBytes;

  Future<void> _getFirmwareStatus() async {
    final resp = await get(Uri.parse("$_baseUri/firmware/status"));
    setState(() {
      _firmwareStatus = jsonDecode(resp.body);
    });
  }

  @override
  void initState() {
    super.initState();
    _getFirmwareStatus();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Colors.blue,
        title: const Text("Firmware"),
      ),
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
              ],
            )),
      ),
    );
  }
}
