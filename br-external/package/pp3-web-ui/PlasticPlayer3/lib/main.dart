import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

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
          primarySwatch: Colors.blue,
        ),
        routerConfig: _router);
  }
}

final _router = GoRouter(
  initialLocation: '/wifi',
  debugLogDiagnostics: true,
  routes: [
    ShellRoute(
        builder: (context, state, child) => Scaffold(
                body: Expanded(
                    child: Row(
              children: [
                Drawer(
                    child: ListView(
                  children: [
                    const DrawerHeader(
                      child: Text("Plastic Player 3"),
                    ),
                    ListTile(
                      title: const Text('Wifi'),
                      selected: GoRouterState.of(context).fullPath == '/wifi',
                      selectedTileColor: Colors.grey[300],
                      onTap: () {
                        context.go('/wifi');
                      },
                    ),
                    ListTile(
                      title: const Text('Firmware'),
                      selected:
                          GoRouterState.of(context).fullPath == '/firmware',
                      selectedTileColor: Colors.grey[300],
                      onTap: () {
                        context.go('/firmware');
                      },
                    ),
                  ],
                )),
                Expanded(child: child)
              ],
            ))),
        routes: [
          GoRoute(
              path: '/wifi',
              pageBuilder: (context, state) {
                return const MaterialPage(child: WifiPage());
              }),
          GoRoute(
              path: '/firmware',
              pageBuilder: (context, state) {
                return const MaterialPage(child: FirmwarePage());
              }),
        ])
  ],
);
