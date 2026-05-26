#include "SessionDialog.h"

#include "CredentialDialog.h"
#include "ModalLoop.h"
#include "Resource.h"
#include "UiTheme.h"
#include "Util.h"

#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>

namespace {

struct State {
    SessionDialogResult* result = nullptr;
    ConnectionTreeModel* model = nullptr;
    const TreeNode* existing = nullptr;
    HWND editName = nullptr;
    HWND editHost = nullptr;
    HWND editPort = nullptr;
    HWND comboCred = nullptr;
};

void FillCredentials(HWND combo, ConnectionTreeModel& model, const std::wstring& selected) {
    ComboBox_ResetContent(combo);
    ComboBox_AddString(combo, L"(None)");
    int sel = 0;
    int idx = 1;
    for (const auto& c : model.Credentials()) {
        ComboBox_AddString(combo, c.label.c_str());
        ComboBox_SetItemData(combo, idx, reinterpret_cast<LPARAM>(c.id.c_str()));
        if (c.id == selected) {
            sel = idx;
        }
        ++idx;
    }
    ComboBox_SetCurSel(combo, sel);
}

void Layout(HWND hwnd, State* st) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int m = 12;
    const int lh = 18;
    const int eh = 24;
    const int row = lh + eh + 8;
    int y = m;

    auto placeLabel = [&](int id, const wchar_t* text) {
        HWND w = GetDlgItem(hwnd, id);
        if (w) {
            SetWindowTextW(w, text);
            SetWindowPos(w, nullptr, m, y, 120, lh, SWP_NOZORDER);
        }
    };
    auto placeEdit = [&](HWND edit, int yOff) {
        if (edit) {
            SetWindowPos(edit, nullptr, m, yOff, rc.right - 2 * m, eh, SWP_NOZORDER);
        }
    };

    placeLabel(10, L"Name:");
    placeEdit(st->editName, y + lh);
    y += row;
    placeLabel(11, L"Host / IP:");
    placeEdit(st->editHost, y + lh);
    y += row;
    placeLabel(12, L"Port:");
    placeEdit(st->editPort, y + lh);
    y += row;
    placeLabel(13, L"Credential:");
    if (st->comboCred) {
        SetWindowPos(st->comboCred, nullptr, m, y + lh, rc.right - 2 * m - 100, eh, SWP_NOZORDER);
    }
    HWND manage = GetDlgItem(hwnd, IDC_SESS_MANAGE);
    if (manage) {
        SetWindowPos(manage, nullptr, rc.right - m - 90, y + lh, 90, eh, SWP_NOZORDER);
    }
    y += row + 8;

    HWND ok = GetDlgItem(hwnd, IDOK);
    HWND cancel = GetDlgItem(hwnd, IDCANCEL);
    const int btnH = 28;
    const int btnW = 80;
    const int btnY = rc.bottom - m - btnH;
    if (cancel) {
        SetWindowPos(cancel, nullptr, rc.right - m - btnW, btnY, btnW, btnH, SWP_NOZORDER);
    }
    if (ok) {
        SetWindowPos(ok, nullptr, rc.right - m - btnW * 2 - 8, btnY, btnW, btnH, SWP_NOZORDER);
    }
}

INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    State* st = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            st = static_cast<State*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

            auto mkLabel = [&](int id) {
                return CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
            };
            mkLabel(10);
            mkLabel(11);
            mkLabel(12);
            mkLabel(13);

            std::wstring name, host, port = L"3389", cred;
            if (st->existing) {
                name = st->existing->name;
                host = st->existing->host;
                port = std::to_wstring(st->existing->port);
                cred = st->existing->credentialId;
            }

            st->editName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", name.c_str(),
                                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, hwnd,
                                           reinterpret_cast<HMENU>(IDC_SESS_NAME), nullptr, nullptr);
            st->editHost = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", host.c_str(),
                                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, hwnd,
                                           reinterpret_cast<HMENU>(IDC_SESS_HOST), nullptr, nullptr);
            st->editPort = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", port.c_str(),
                                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 0, 0, 10, 10, hwnd,
                                           reinterpret_cast<HMENU>(IDC_SESS_PORT), nullptr, nullptr);
            st->comboCred = CreateWindowExW(0, WC_COMBOBOXW, L"",
                                            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 10, hwnd,
                                            reinterpret_cast<HMENU>(IDC_SESS_CRED), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Manage...", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(IDC_SESS_MANAGE), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);

            FillCredentials(st->comboCred, *st->model, cred);
            UiTheme::Apply(hwnd);
            Layout(hwnd, st);
            SetFocus(st->editName);
            return 0;
        }
        case WM_SIZE:
            if (st) {
                Layout(hwnd, st);
            }
            return 0;
        case WM_CTLCOLORSTATIC:
            return UiTheme::OnCtlColorStatic(reinterpret_cast<HDC>(wParam));
        case WM_COMMAND:
            if (st && LOWORD(wParam) == IDC_SESS_MANAGE) {
                ShowCredentialManager(hwnd, *st->model);
                const int sel = ComboBox_GetCurSel(st->comboCred);
                std::wstring selected;
                if (sel > 0) {
                    const int len = ComboBox_GetLBTextLen(st->comboCred, sel);
                    std::wstring text(len, L'\0');
                    ComboBox_GetLBText(st->comboCred, sel, text.data());
                    for (const auto& c : st->model->Credentials()) {
                        if (c.label == text) {
                            selected = c.id;
                            break;
                        }
                    }
                }
                FillCredentials(st->comboCred, *st->model, selected);
                return 0;
            }
            if (LOWORD(wParam) == IDOK && st) {
                wchar_t buf[512]{};
                GetWindowTextW(st->editName, buf, 512);
                st->result->name = Util::Trim(buf);
                GetWindowTextW(st->editHost, buf, 512);
                st->result->host = Util::Trim(buf);
                GetWindowTextW(st->editPort, buf, 512);
                st->result->port = _wtoi(buf);
                if (st->result->port <= 0 || st->result->port > 65535) {
                    st->result->port = 3389;
                }
                st->result->credentialId.clear();
                const int sel = ComboBox_GetCurSel(st->comboCred);
                if (sel > 0) {
                    const int len = ComboBox_GetLBTextLen(st->comboCred, sel);
                    std::wstring text(len, L'\0');
                    ComboBox_GetLBText(st->comboCred, sel, text.data());
                    for (const auto& c : st->model->Credentials()) {
                        if (c.label == text) {
                            st->result->credentialId = c.id;
                            break;
                        }
                    }
                }
                if (st->result->name.empty() || st->result->host.empty() || !Util::IsValidHost(st->result->host)) {
                    MessageBoxW(hwnd, L"Enter a valid name and host.", L"Session", MB_ICONWARNING);
                    return 0;
                }
                st->result->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

bool ShowSessionDialog(HWND owner, ConnectionTreeModel& model, const TreeNode* existing,
                       SessionDialogResult& result) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::DialogBackgroundBrush();
        wc.lpszClassName = L"SecureRdpSessionDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    State st;
    st.result = &result;
    st.model = &model;
    st.existing = existing;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE, L"SecureRdpSessionDlg",
                               existing ? L"Edit Session" : L"New Session", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, 0,
                               100, 100, owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (!dlg) {
        return false;
    }

    CenterWindowOnOwner(dlg, owner, 420, 300);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    EnableWindow(owner, FALSE);

    RunModalLoop(dlg);

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return result.accepted;
}
