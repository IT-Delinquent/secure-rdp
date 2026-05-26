#pragma once

#include <windows.h>

// Message loop for custom modal windows (not CreateDialog). Do not use IsDialogMessageW.
inline void RunModalLoop(HWND dlg) {
    MSG msg{};
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
                continue;
            }
            if (msg.wParam == VK_RETURN) {
                HWND focus = GetFocus();
                if (focus && IsChild(dlg, focus)) {
                    wchar_t cls[16]{};
                    GetClassNameW(focus, cls, 16);
                    if (_wcsicmp(cls, L"Button") != 0) {
                        SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
                        continue;
                    }
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

inline void CenterWindowOnOwner(HWND dlg, HWND owner, int clientW, int clientH) {
    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(dlg, GWL_STYLE));
    DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(dlg, GWL_EXSTYLE));
    RECT frame{0, 0, clientW, clientH};
    AdjustWindowRectEx(&frame, style, FALSE, exStyle);

    RECT ownerRc{};
    GetWindowRect(owner, &ownerRc);
    const int winW = frame.right - frame.left;
    const int winH = frame.bottom - frame.top;
    const int x = ownerRc.left + (ownerRc.right - ownerRc.left - winW) / 2;
    const int y = ownerRc.top + (ownerRc.bottom - ownerRc.top - winH) / 2;
    SetWindowPos(dlg, nullptr, x, y, winW, winH, SWP_NOZORDER | SWP_NOACTIVATE);
}
