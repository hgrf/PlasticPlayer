import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import 'bluetooth_page.dart';
import 'firmware_page.dart';
import 'wifi_page.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp.router(
        title: "Plastic Player 3",
        theme: ThemeData(
            colorScheme: ColorScheme.fromSeed(seedColor: Colors.blueGrey),
            appBarTheme: const AppBarTheme(
                backgroundColor: Colors.blueGrey,
                foregroundColor: Colors.white)),
        routerConfig: _router);
  }
}

final _router = GoRouter(
  initialLocation: '/wifi',
  routes: [
    ShellRoute(
        builder: (context, state, child) => Scaffold(
                body: Row(
              children: [
                SizedBox(
                    width: 250,
                    child: ListView(
                      children: [
                        Image.asset('images/logo.png', width: 200, height: 100),
                        const Divider(),
                        ListTile(
                          title: const Text('Bluetooth'),
                          leading: const Icon(Icons.bluetooth),
                          selected: GoRouterState.of(context).fullPath ==
                              '/bluetooth',
                          selectedTileColor: Colors.grey[300],
                          onTap: () => context.go('/bluetooth'),
                        ),
                        ListTile(
                          title: const Text('Wifi'),
                          leading: const Icon(Icons.wifi),
                          selected:
                              GoRouterState.of(context).fullPath == '/wifi',
                          selectedTileColor: Colors.grey[300],
                          onTap: () => context.go('/wifi'),
                        ),
                        ListTile(
                          title: const Text('Firmware'),
                          leading: const Icon(Icons.memory),
                          selected:
                              GoRouterState.of(context).fullPath == '/firmware',
                          selectedTileColor: Colors.grey[300],
                          onTap: () => context.go('/firmware'),
                        ),
                      ],
                    )),
                Expanded(child: child)
              ],
            )),
        routes: [
          GoRoute(
              path: '/bluetooth',
              pageBuilder: (context, state) =>
                  const MaterialPage(child: BluetoothPage())),
          GoRoute(
            path: '/wifi',
            pageBuilder: (context, state) =>
                const MaterialPage(child: WifiPage()),
          ),
          GoRoute(
            path: '/firmware',
            pageBuilder: (context, state) =>
                const MaterialPage(child: FirmwarePage()),
          ),
        ])
  ],
);
