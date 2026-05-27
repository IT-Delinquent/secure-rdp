#include "AssignCredentialDialog.h"

#include "CredentialDialog.h"
#include "CredentialUi.h"
#include "ModalLoop.h"
#include "Resource.h"
#include "UiTheme.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <memory>
#include <string>

namespace {

constexpr wchar_t kStateProp[] = L"SecureRdpAssignCredDlgState";
constexpr int IDC_ASSIGN_LABEL = 3601;
constexpr int IDC_ASSIGN_CRED = 3602;
constexpr int IDC_ASSIGN_MANAGE = 3603;

struct State {
    AssignCredentialResult* result = nullptr;
    ConnectionTreeModel* model = nullptr;
    int sessionCount = 0;
    HWND label = nullptr;
    HWND comboCred = nullptr;
};

void RefreshCredentialCombo(State* st) {
    if (!st || !st->comboCred || !st->model) {
        return;
    }
    const std::wstring selected = CredentialIdFromCombo(st->comboCred, *st->model);
    FillCredentialCombo(st->comboCred, *st->model, selected);
}

void Layout(HWND hwnd, const State* st) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (rc.bottom < 100) {
        return;
    }
    const int m = 16;
    const int lh = 20;
    const int eh = 26;
    const int btnH = 32;
    const int btnW = 96;

    if (st && st->label) {
        SetWindowPos(st->label, nullptr, m, m, rc.right - 2 * m, lh * 2, SWP_NOZORDER);
    }
    const int credY = m + lh * 2 + 8;
    if (st && st->comboCred) {
        SetWindowPos(st->comboCred, nullptr, m, credY, rc.right - 2 * m - 100, eh, SWP_NOZORDER);
    }
    if (HWND manage = GetDlgItem(hwnd, IDC_ASSIGN_MANAGE)) {
        SetWindowPos(manage, nullptr, rc.right - m - 90, credY, 90, eh, SWP_NOZORDER);
    }

    const int btnY = rc.bottom - m - btnH;
    if (HWND cancel = GetDlgItem(hwnd, IDCANCEL)) {
        SetWindowPos(cancel, nullptr, rc.right - m - btnW, btnY, btnW, btnH, SWP_NOZORDER);
    }
    if (HWND ok = GetDlgItem(hwnd, IDOK)) {
        SetWindowPos(ok, nullptr, rc.right - m - btnW * 2 - 12, btnY, btnW, btnH, SWP_NOZORDER);
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

            std::wstring prompt = L"Assign credential to ";
            prompt += std::to_wstring(st->sessionCount);
            prompt += st->sessionCount == 1 ? L" session:" : L" sessions:";

            st->label = CreateWindowExW(0, L"STATIC", prompt.c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ASSIGN_LABEL)), nullptr,
                                        nullptr);
            st->comboCred = CreateWindowExW(0, WC_COMBOBOXW, L"",
                                            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 10, hwnd,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ASSIGN_CRED)), nullptr,
                                            nullptr);
            CreateWindowExW(0, L"BUTTON", L"Manage...", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ASSIGN_MANAGE)), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), nullptr, nullptr);
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
            if (st && LOWORD(wParam) == IDC_ASSIGN_MANAGE) {
                ShowCredentialManager(hwnd, *st->model);
                RefreshCredentialCombo(st);
                return 0;
            }
            if (LOWORD(wParam) == IDOK && st && st->result) {
                st->result->credentialId = CredentialIdFromCombo(st->comboCred, *st->model);
                st->result->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                if (st && st->result) {
                    st->result->accepted = false;
                }
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

bool ShowAssignCredentialDialog(HWND owner, ConnectionTreeModel& model, int sessionCount,
                                AssignCredentialResult& result) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::DialogBackgroundBrush();
        wc.lpszClassName = L"SecureRdpAssignCredDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    result.accepted = false;
    result.credentialId.clear();

    auto state = std::make_unique<State>();
    state->result = &result;
    state->model = &model;
    state->sessionCount = sessionCount;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE, L"SecureRdpAssignCredDlg",
                               L"Set Credential", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, 0, 100, 100, owner, nullptr,
                               GetModuleHandleW(nullptr), state.get());
    if (!dlg) {
        return false;
    }

    state.release();

    CenterWindowOnOwner(dlg, owner, 420, 200);
    SendMessageW(dlg, WM_SIZE, 0, MAKELPARAM(420, 200));
    UiTheme::ApplyDialog(dlg);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    if (State* shown = GetState(dlg)) {
        RefreshCredentialCombo(shown);
        if (shown->comboCred) {
            SetFocus(shown->comboCred);
        }
    }
    EnableWindow(owner, FALSE);

    RunModalLoop(dlg);

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return result.accepted;
}
