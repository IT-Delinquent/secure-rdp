#include "FolderScopeDialog.h"

#include "ModalLoop.h"
#include "Resource.h"
#include "UiTheme.h"

#include <commctrl.h>
#include <windows.h>

#include <memory>
#include <string>

namespace {

constexpr wchar_t kStateProp[] = L"TinyRdpFolderScopeDlgState";

struct State {
    std::wstring folderName;
    FolderScopeChoice choice = FolderScopeChoice::Cancelled;
    HWND label = nullptr;
};

void Layout(HWND hwnd, const State* st) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (rc.bottom < 100) {
        return;
    }
    const int m = 16;
    const int labelH = 40;
    const int btnH = 32;
    const int btnW = 120;
    const int btnGap = 8;

    if (st && st->label) {
        SetWindowPos(st->label, nullptr, m, m, rc.right - 2 * m, labelH, SWP_NOZORDER);
    }

    const int totalBtnW = btnW * 3 + btnGap * 2;
    int x = m;
    const int btnY = rc.bottom - m - btnH;
    if (HWND direct = GetDlgItem(hwnd, IDC_SCOPE_DIRECT)) {
        SetWindowPos(direct, nullptr, x, btnY, btnW, btnH, SWP_NOZORDER);
        x += btnW + btnGap;
    }
    if (HWND subtree = GetDlgItem(hwnd, IDC_SCOPE_SUBTREE)) {
        SetWindowPos(subtree, nullptr, x, btnY, btnW, btnH, SWP_NOZORDER);
        x += btnW + btnGap;
    }
    if (HWND cancel = GetDlgItem(hwnd, IDCANCEL)) {
        SetWindowPos(cancel, nullptr, rc.right - m - btnW, btnY, btnW, btnH, SWP_NOZORDER);
    }
    (void)totalBtnW;
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

            std::wstring body = L"Apply credential to sessions in \"";
            body += st->folderName;
            body += L"\"?";

            st->label = CreateWindowExW(0, L"STATIC", body.c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SCOPE_LABEL)), nullptr,
                                        nullptr);
            CreateWindowExW(0, L"BUTTON", L"Direct children", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SCOPE_DIRECT)), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Entire subtree", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SCOPE_SUBTREE)), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), nullptr, nullptr);

            UiTheme::ApplyDialog(hwnd);
            Layout(hwnd, st);
            return 0;
        }
        case WM_SIZE:
            Layout(hwnd, st);
            return 0;
        case WM_COMMAND:
            if (!st) {
                break;
            }
            switch (LOWORD(wParam)) {
                case IDC_SCOPE_DIRECT:
                    st->choice = FolderScopeChoice::DirectChildren;
                    DestroyWindow(hwnd);
                    return 0;
                case IDC_SCOPE_SUBTREE:
                    st->choice = FolderScopeChoice::EntireSubtree;
                    DestroyWindow(hwnd);
                    return 0;
                case IDCANCEL:
                    st->choice = FolderScopeChoice::Cancelled;
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            break;
        case WM_CLOSE:
            if (st) {
                st->choice = FolderScopeChoice::Cancelled;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            RemovePropW(hwnd, kStateProp);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

FolderScopeChoice ShowFolderScopeDialog(HWND owner, const std::wstring& folderName) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::DialogBackgroundBrush();
        wc.lpszClassName = L"TinyRdpFolderScopeDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    State state;
    state.folderName = folderName;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE, L"TinyRdpFolderScopeDlg", L"Set Credential",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, 0, 100, 100, owner, nullptr,
                               GetModuleHandleW(nullptr), &state);
    if (!dlg) {
        return FolderScopeChoice::Cancelled;
    }

    CenterWindowOnOwner(dlg, owner, 420, 160);
    SendMessageW(dlg, WM_SIZE, 0, MAKELPARAM(420, 160));
    UiTheme::ApplyDialog(dlg);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    EnableWindow(owner, FALSE);

    RunModalLoop(dlg);

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return state.choice;
}
