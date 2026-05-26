# Secure RDP Connection Manager v1.0.0

A small native Windows desktop application for organizing Remote Desktop sessions in a folder tree. Passwords are stored in **Windows Credential Manager** (DPAPI-backed); connection data on disk never contains secrets.

## Technologies

| Area | Stack |
|------|--------|
| Language | **C++17** (MSVC, `/W4`, static CRT in Release) |
| Build | **CMake** 3.20+, **Visual Studio 2022** (x64), Windows SDK |
| UI | **Win32** API — Common Controls (tree view, toolbar), modal dialogs, **uxtheme** for visual styles |
| Persistence | **JSON** on disk (`connections.json`, `credentials.json`) via vendored [**nlohmann/json**](https://github.com/nlohmann/json) |
| Import / export | [**MSXML 6**](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ms763742(v=vs.85)) (DOM) for mRemoteNG-compatible XML |
| Secrets | **Windows Credential Manager** (`wincred.h`) — DPAPI-backed profile passwords and `TERMSRV/{host}` entries for RDP |
| Remote Desktop | **`mstsc.exe`**, temporary `.rdp` launch files (no password fields on disk) |
| Clipboard | Custom `SecureRdp/NodeV1` format (JSON subtree) via Win32 clipboard APIs |
| Drag-and-drop | Tree-view reparenting with Common Controls hit-testing |
| Resources | Windows **`.rc`** resources and embedded **`.ico`** application icon |
| Dev tooling (optional) | **Python** + **Pillow** — `resources/build_icon.py` regenerates `app.ico` from `app-source-no-background.png` |

No third-party UI framework, package manager, or runtime beyond the Windows SDK and the single header-only JSON dependency in `external/`.

## Features

- Tree view with folders and RDP sessions (name, host/IP, port default 3389)
- Reusable credential profiles linked to sessions
- Launch sessions in **fullscreen** via `mstsc.exe` (separate process)
- Drag-and-drop reparenting, copy/paste, duplicate
- Import and export connections in **mRemoteNG XML** format (folders and RDP sessions)
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

## Building with full paths!
```powershell
cd c:\source\secure-rdp
& "C:\Program Files\CMake\bin\cmake.exe" -B build -G "Visual Studio 17 2022" -A x64
& "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release
```

Output: `build\Release\SecureRdp.exe`

To refresh the branding icon after changing `resources/app-source-no-background.png`:

```powershell
python resources/build_icon.py
cmake --build build --config Release --clean-first
```

`build_icon.py` centers the visible artwork with uniform padding before resizing, so the title-bar icon is not clipped on one side.

If Explorer still shows a blank/generic icon after rebuilding, clear the Windows icon cache (or open the new `SecureRdp-with-icon.exe` copy) — Explorer caches icons per path.

## Usage

1. Run `SecureRdp.exe`.
2. Use **File → Manage Credentials** to add username/domain/password profiles.
3. Create folders and sessions; link a credential in the session editor.
4. Double-click a session or press **Connect** / Enter to launch fullscreen RDP.
5. Drag items to reorganize; **Ctrl+C** / **Ctrl+V** to copy/paste subtrees; **Duplicate** from the Edit menu.
6. Use **File → Import** / **Export** for mRemoteNG-compatible XML (`.xml`). Import merges into the selected folder (or the root). Non-RDP protocols are skipped on import; passwords from mRemoteNG files are not imported (they are encrypted in that format)—set passwords in **Manage Credentials** after import.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| Enter | Connect selected session |
| Delete | Delete selected item |
| Ctrl+C | Copy subtree |
| Ctrl+V | Paste subtree |
| F5 | Refresh tree view |

## Where RDP session data is stored

All persistent app data lives under **`%AppData%\SecureRdp\`** (Roaming AppData, e.g. `C:\Users\<you>\AppData\Roaming\SecureRdp\`).

| Location | What is stored |
|----------|----------------|
| `%AppData%\SecureRdp\connections.json` | Folder tree and **RDP sessions**: display name, host/IP, port (default 3389), session id, and optional `credentialId` linking to a profile. **No passwords.** |
| `%AppData%\SecureRdp\credentials.json` | Credential **profiles**: id, label, username, domain. **No passwords.** |
| Windows Credential Manager — `SecureRdp/Profile/{profile-uuid}` | Password for each credential profile (DPAPI-backed). |
| Windows Credential Manager — `TERMSRV/{host}` or `TERMSRV/{host:port}` | Mirrored credentials written when you connect, so `mstsc.exe` can sign in (same idea as `cmdkey /generic:TERMSRV/...`). |
| `%TEMP%\SecureRdp\{session-id}.rdp` | Short-lived launch file per connect: address, port, fullscreen flags, optional username. **No password fields.** |
| `%AppData%\SecureRdp\app.log` | Application log (diagnostics only; not session configuration) |

Clipboard copy/paste uses in-memory format `SecureRdp/NodeV1` (JSON subtree, no secrets)—nothing extra is written to disk for that.

## Logging

The app writes timestamped lines to `%AppData%\SecureRdp\app.log` and to the debugger output (view with [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview) or Visual Studio). Log levels: DEBUG, INFO, WARN, ERROR. Unhandled exceptions (including heap corruption) are recorded on crash.

## License

Provided as-is for local use.
