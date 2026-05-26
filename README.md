# Secure RDP Connection Manager v0.0.1

A small native Windows desktop application for organizing Remote Desktop sessions in a folder tree. Passwords are stored in **Windows Credential Manager** (DPAPI-backed); connection data on disk never contains secrets.

## Features

- Tree view with folders and RDP sessions (name, host/IP, port default 3389)
- Reusable credential profiles linked to sessions
- Launch sessions in **fullscreen** via `mstsc.exe` (separate process)
- Drag-and-drop reparenting, copy/paste, duplicate
- Inline rename (F2 / slow double-click on label)
- Toolbar, menus, and context menu

## Security model

- `connections.json` and `credentials.json` under `%AppData%\SecureRdp\` hold structure and non-secret metadata only.
- Passwords live in Credential Manager targets `SecureRdp/Profile/{uuid}`.
- On connect, credentials are mirrored to `TERMSRV/{host[:port]}` so the built-in RDP client can authenticate (same approach as `cmdkey /generic:TERMSRV/...`).
- No application-level encryption keys or reversible password storage in project files.
- Clipboard copy uses format `SecureRdp/NodeV1` (JSON subtree without passwords).

## Requirements

- Windows 10 or 11 (x64)
- Visual Studio 2022 **or** Build Tools with “Desktop development with C++”
- CMake 3.20+

## Build

```powershell
cd c:\source\secure-rdp
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build\Release\SecureRdp.exe`

To refresh the branding icon after changing `resources/app-source.png`:

```powershell
python -c "from PIL import Image; import os; img=Image.open('resources/app-source.png').convert('RGBA'); s=[(256,256),(128,128),(64,64),(48,48),(32,32),(16,16)]; icons=[img.resize(x,Image.Resampling.LANCZOS) for x in s]; icons[0].save('resources/app.ico',format='ICO',sizes=[(i.width,i.height) for i in icons],append_images=icons[1:])"
cmake --build build --config Release --clean-first
```

If Explorer still shows a blank/generic icon after rebuilding, clear the Windows icon cache (or open the new `SecureRdp-with-icon.exe` copy) — Explorer caches icons per path.

## Usage

1. Run `SecureRdp.exe`.
2. Use **File → Manage Credentials** to add username/domain/password profiles.
3. Create folders and sessions; link a credential in the session editor.
4. Double-click a session or press **Connect** / Enter to launch fullscreen RDP.
5. Drag items to reorganize; **Ctrl+C** / **Ctrl+V** to copy/paste subtrees; **Duplicate** from the Edit menu.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| Enter | Connect selected session |
| Delete | Delete selected item |
| Ctrl+C | Copy subtree |
| Ctrl+V | Paste subtree |
| F5 | Refresh tree view |

## Data locations

| Path | Contents |
|------|----------|
| `%AppData%\SecureRdp\connections.json` | Folder/session tree |
| `%AppData%\SecureRdp\credentials.json` | Credential labels and usernames (no passwords) |
| `%AppData%\SecureRdp\app.log` | Application log (diagnostics) |
| `%TEMP%\SecureRdp\*.rdp` | Temporary launch files (no password fields) |
| Credential Manager | Passwords and TERMSRV entries |

## Logging

The app writes timestamped lines to `%AppData%\SecureRdp\app.log` and to the debugger output (view with [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview) or Visual Studio). Log levels: DEBUG, INFO, WARN, ERROR. Unhandled exceptions (including heap corruption) are recorded on crash.

## License

Provided as-is for local use.
