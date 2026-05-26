#include "AboutDialog.h"

#include "ModalLoop.h"
#include "UiTheme.h"
#include "Version.h"

#include <commctrl.h>
#include <windows.h>

#include <memory>
#include <string>

namespace {

constexpr wchar_t kStateProp[] = L"SecureRdpAboutDlgState";
constexpr int IDC_ABOUT_TEXT = 3401;

std::wstring AboutBody() {
    std::wstring text = L"Secure RDP Connection Manager ";
    text += SECURE_RDP_VERSION;
    text += L"\r\n\r\nA native Windows desktop application for organizing Remote Desktop sessions in a folder tree.\r\n";
    text += L"Passwords are stored in Windows Credential Manager (DPAPI-backed); connection data on disk never contains secrets.\r\n\r\n";
    text += L"RDP session data is stored in the following locations:\r\n";
    text += L" - %AppData%\\SecureRdp\\connections.json — folder tree and sessions (name, host, port, credential link; no passwords)\r\n";
    text += L" - %AppData%\\SecureRdp\\credentials.json — profile labels, usernames, domains (no passwords)\r\n";
    text += L" - Credential Manager: SecureRdp/Profile/{uuid} — profile passwords\r\n";
    text += L" - Credential Manager: TERMSRV/{host[:port]} — mirrored on connect for mstsc.exe\r\n";
    text += L" - %TEMP%\\SecureRdp\\{session-id}.rdp — temporary launch files (no password fields)\r\n";
    text += L" - %AppData%\\SecureRdp\\app.log — diagnostics only\r\n\r\n";
    text += L"Technologies\r\n";
    text += L"  Language: C++17 (MSVC, static CRT in Release)\r\n";
    text += L"  Build: CMake 3.20+, Visual Studio 2022 (x64), Windows SDK\r\n";
    text += L"  UI: Win32 API — Common Controls (tree, toolbar), uxtheme\r\n";
    text += L"  Persistence: JSON via nlohmann/json\r\n";
    text += L"  Import/export: MSXML 6 (mRemoteNG-compatible XML)\r\n";
    text += L"  Secrets: Windows Credential Manager (DPAPI)\r\n";
    text += L"  Remote Desktop: mstsc.exe, temporary .rdp launch files\r\n";
    text += L"  Clipboard: SecureRdp/NodeV1 format (JSON subtree)\r\n";
    text += L"  Drag-and-drop: Tree-view reparenting\r\n";
    text += L"  Resources: Windows .rc resources and embedded .ico icon\r\n";
    return text;
}

struct State {
    HWND text = nullptr;
};

void Layout(HWND hwnd, const State* st) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (rc.bottom < 120) {
        return;
    }
    const int margin = 16;
    const int btnH = 32;
    const int btnW = 96;
    const int btnY = rc.bottom - margin - btnH;
    const int textBottom = btnY - margin;

    if (st && st->text) {
        SetWindowPos(st->text, nullptr, margin, margin, rc.right - 2 * margin, textBottom - margin, SWP_NOZORDER);
    }
    if (HWND ok = GetDlgItem(hwnd, IDOK)) {
        SetWindowPos(ok, nullptr, rc.right - margin - btnW, btnY, btnW, btnH, SWP_NOZORDER);
    }
}

State* GetState(HWND hwnd) {
    return reinterpret_cast<State*>(GetPropW(hwnd, kStateProp));
}

INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LRESULT themeResult = 0;
    if (UiTheme::HandleDialogMessages(hwnd, msg, wParam, lParam, themeResult)) {
        return themeResult;
    }

    State* st = GetState(hwnd);
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            st = static_cast<State*>(cs->lpCreateParams);
            if (!st) {
                return -1;
            }
            SetPropW(hwnd, kStateProp, reinterpret_cast<HANDLE>(st));
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

            st->text = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", AboutBody().c_str(),
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 10, 10, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ABOUT_TEXT)), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), nullptr, nullptr);
            UiTheme::ApplyDialog(hwnd);
            Layout(hwnd, st);
            return 0;
        }
        case WM_SIZE:
            Layout(hwnd, st);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY: {
            State* owned = GetState(hwnd);
            RemovePropW(hwnd, kStateProp);
            delete owned;
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

void ShowAboutDialog(HWND owner) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::DialogBackgroundBrush();
        wc.lpszClassName = L"SecureRdpAboutDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    auto state = std::make_unique<State>();

    std::wstring title = L"About Secure RDP ";
    title += SECURE_RDP_VERSION;

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE;

    HWND dlg = CreateWindowExW(exStyle, L"SecureRdpAboutDlg", title.c_str(), style, 0, 0, 100, 100, owner, nullptr,
                               GetModuleHandleW(nullptr), state.get());
    if (!dlg) {
        return;
    }

    state.release();

    CenterWindowOnOwner(dlg, owner, 480, 380);
    SendMessageW(dlg, WM_SIZE, 0, MAKELPARAM(480, 380));
    UiTheme::ApplyDialog(dlg);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    EnableWindow(owner, FALSE);

    RunModalLoop(dlg);

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
}
