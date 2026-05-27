#include "FolderDialog.h"

#include "Logger.h"
#include "ModalLoop.h"
#include "Resource.h"
#include "UiTheme.h"
#include "Util.h"

#include <commctrl.h>
#include <windows.h>

#include <memory>

namespace {

constexpr wchar_t kStateProp[] = L"SecureRdpFolderDlgState";

struct State {
    FolderDialogResult* result = nullptr;
    std::wstring initial;
    HWND editName = nullptr;
};

void Layout(HWND hwnd, const State* st) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (rc.bottom < 80) {
        return;
    }
    const int margin = 16;
    const int labelH = 20;
    const int editH = 28;
    const int btnH = 32;
    const int btnW = 96;
    const int y = margin;

    if (HWND lbl = GetDlgItem(hwnd, IDC_FOLDER_LABEL)) {
        SetWindowPos(lbl, nullptr, margin, y, rc.right - 2 * margin, labelH, SWP_NOZORDER);
    }
    if (st && st->editName) {
        SetWindowPos(st->editName, nullptr, margin, y + labelH + 8, rc.right - 2 * margin, editH, SWP_NOZORDER);
    }
    const int btnY = rc.bottom - margin - btnH;
    if (HWND cancel = GetDlgItem(hwnd, IDCANCEL)) {
        SetWindowPos(cancel, nullptr, rc.right - margin - btnW, btnY, btnW, btnH, SWP_NOZORDER);
    }
    if (HWND ok = GetDlgItem(hwnd, IDOK)) {
        SetWindowPos(ok, nullptr, rc.right - margin - btnW * 2 - 12, btnY, btnW, btnH, SWP_NOZORDER);
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

            CreateWindowExW(0, L"STATIC", L"Folder name", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FOLDER_LABEL)), nullptr, nullptr);
            st->editName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->initial.c_str(),
                                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, hwnd,
                                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FOLDER_NAME)), nullptr,
                                           nullptr);
            CreateWindowExW(0, L"BUTTON", L"Create", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), nullptr, nullptr);
            UiTheme::ApplyDialog(hwnd);
            Layout(hwnd, st);
            SetFocus(st->editName);
            return 0;
        }
        case WM_SIZE:
            Layout(hwnd, st);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK && st && st->result) {
                wchar_t buf[512]{};
                GetWindowTextW(st->editName, buf, 512);
                st->result->name = buf;
                st->result->accepted = !st->result->name.empty();
                LOG_INFO(L"FolderDialog: accepted name=" + st->result->name);
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                if (st && st->result) {
                    st->result->accepted = false;
                }
                LOG_INFO(L"FolderDialog: cancelled");
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            if (st && st->result) {
                st->result->accepted = false;
            }
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

bool ShowFolderDialog(HWND owner, const std::wstring& initialName, FolderDialogResult& result) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::DialogBackgroundBrush();
        wc.lpszClassName = L"SecureRdpFolderDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    result.accepted = false;

    auto state = std::make_unique<State>();
    state->result = &result;
    state->initial = initialName;

    LOG_INFO(L"FolderDialog: opening");

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE;

    HWND dlg = CreateWindowExW(exStyle, L"SecureRdpFolderDlg", L"New Folder", style, 0, 0, 100, 100, owner, nullptr,
                               GetModuleHandleW(nullptr), state.get());
    if (!dlg) {
        LOG_ERROR(L"FolderDialog: CreateWindowEx failed: " + Util::FormatWin32Error(GetLastError()));
        return false;
    }

    state.release();

    CenterWindowOnOwner(dlg, owner, 380, 140);
    SendMessageW(dlg, WM_SIZE, 0, MAKELPARAM(380, 140));
    UiTheme::ApplyDialog(dlg);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    EnableWindow(owner, FALSE);

    RunModalLoop(dlg);

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return result.accepted;
}
