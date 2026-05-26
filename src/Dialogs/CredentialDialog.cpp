#include "CredentialDialog.h"

#include "CredentialVault.h"
#include "Resource.h"
#include "UiTheme.h"
#include "Util.h"

#include <windows.h>
#include <commctrl.h>

namespace {

struct CredEditState {
    bool accepted = false;
    bool isNew = true;
    std::wstring id;
    CredentialMeta meta;
    std::wstring password;
    HWND editLabel = nullptr;
    HWND editUser = nullptr;
    HWND editDomain = nullptr;
    HWND editPass = nullptr;
};

LRESULT CALLBACK CredEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<CredEditState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            st = static_cast<CredEditState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            const int m = 12;
            int y = m;
            auto label = [&](const wchar_t* t) {
                CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, m, y, 120, 18, hwnd, nullptr, nullptr, nullptr);
                y += 20;
            };
            auto edit = [&](const wchar_t* text, int id, bool password) {
                DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
                if (password) {
                    style |= ES_PASSWORD;
                }
                HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, style, m, y, 300, 24, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                y += 32;
                return e;
            };
            label(L"Label:");
            st->editLabel = edit(st->meta.label.c_str(), IDC_CRED_LABEL, false);
            label(L"Username:");
            st->editUser = edit(st->meta.username.c_str(), IDC_CRED_USER, false);
            label(L"Domain (optional):");
            st->editDomain = edit(st->meta.domain.c_str(), IDC_CRED_DOMAIN, false);
            label(L"Password:");
            st->editPass = edit(L"", IDC_CRED_PASS, true);
            SendMessageW(st->editPass, EM_SETPASSWORDCHAR, L'●', 0);
            if (!st->password.empty()) {
                SetWindowTextW(st->editPass, st->password.c_str());
            }
            CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, m, y + 8, 80, 28, hwnd,
                            reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, m + 90, y + 8, 80, 28, hwnd,
                            reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
            UiTheme::Apply(hwnd);
            return 0;
        }
        case WM_COMMAND:
            if (!st) {
                break;
            }
            if (LOWORD(wParam) == IDOK) {
                wchar_t buf[512]{};
                GetWindowTextW(st->editLabel, buf, 512);
                st->meta.label = Util::Trim(buf);
                GetWindowTextW(st->editUser, buf, 512);
                st->meta.username = Util::Trim(buf);
                GetWindowTextW(st->editDomain, buf, 512);
                st->meta.domain = Util::Trim(buf);
                GetWindowTextW(st->editPass, buf, 512);
                st->password = buf;
                if (st->meta.label.empty() || st->meta.username.empty()) {
                    MessageBoxW(hwnd, L"Label and username are required.", L"Credential", MB_ICONWARNING);
                    return 0;
                }
                st->accepted = true;
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

bool ShowCredEditDialog(HWND owner, CredEditState& state) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = CredEditProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"SecureRdpCredEditDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    RECT ownerRc{};
    GetWindowRect(owner, &ownerRc);
    const int w = 340;
    const int h = 280;
    const int x = ownerRc.left + (ownerRc.right - ownerRc.left - w) / 2;
    const int y = ownerRc.top + (ownerRc.bottom - ownerRc.top - h) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"SecureRdpCredEditDlg",
                               state.isNew ? L"Add Credential" : L"Edit Credential", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                               x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &state);
    if (!dlg) {
        return false;
    }
    ShowWindow(dlg, SW_SHOW);
    EnableWindow(owner, FALSE);
    MSG msg{};
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return state.accepted;
}

struct CredMgrState {
    ConnectionTreeModel* model = nullptr;
    HWND list = nullptr;
};

std::wstring SelectedCredId(HWND list, ConnectionTreeModel& model) {
    const int sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    if (sel == LB_ERR) {
        return L"";
    }
    const size_t index = static_cast<size_t>(SendMessageW(list, LB_GETITEMDATA, sel, 0));
    const auto& creds = model.Credentials();
    if (index >= creds.size()) {
        return L"";
    }
    return creds[index].id;
}

void RefreshList(CredMgrState& st) {
    SendMessageW(st.list, LB_RESETCONTENT, 0, 0);
    const auto& creds = st.model->Credentials();
    for (size_t i = 0; i < creds.size(); ++i) {
        const int idx = static_cast<int>(SendMessageW(st.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(creds[i].label.c_str())));
        SendMessageW(st.list, LB_SETITEMDATA, idx, static_cast<LPARAM>(i));
    }
}

INT_PTR CALLBACK CredMgrProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<CredMgrState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            st = static_cast<CredMgrState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            st->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                       WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_HASSTRINGS, 12, 12, 300,
                                       200, hwnd, reinterpret_cast<HMENU>(IDC_CRED_LIST), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE, 12, 220, 70, 28, hwnd,
                            reinterpret_cast<HMENU>(IDC_CRED_ADD), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Edit", WS_CHILD | WS_VISIBLE, 90, 220, 70, 28, hwnd,
                            reinterpret_cast<HMENU>(IDC_CRED_EDIT), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE, 168, 220, 70, 28, hwnd,
                            reinterpret_cast<HMENU>(IDC_CRED_DELETE), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE, 260, 220, 70, 28, hwnd,
                            reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
            RefreshList(*st);
            UiTheme::Apply(hwnd);
            return 0;
        }
        case WM_COMMAND:
            if (!st) {
                break;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_CRED_ADD) {
                CredEditState edit;
                edit.isNew = true;
                if (ShowCredEditDialog(hwnd, edit)) {
                    CredentialMeta* meta = st->model->AddCredential(edit.meta.label, edit.meta.username, edit.meta.domain);
                    CredentialVault vault;
                    std::wstring err;
                    vault.SaveProfile(meta->id, meta->label, meta->username, meta->domain, edit.password, err);
                    RefreshList(*st);
                }
                return 0;
            }
            if (LOWORD(wParam) == IDC_CRED_EDIT) {
                const std::wstring id = SelectedCredId(st->list, *st->model);
                if (id.empty()) {
                    return 0;
                }
                CredentialMeta* meta = st->model->FindCredential(id);
                if (!meta) {
                    return 0;
                }
                CredEditState edit;
                edit.isNew = false;
                edit.id = id;
                edit.meta = *meta;
                CredentialVault vault;
                CredentialSecrets secrets;
                std::wstring err;
                vault.LoadProfile(id, secrets, err);
                edit.meta.username = secrets.username;
                edit.meta.domain = secrets.domain;
                edit.password = secrets.password;
                if (ShowCredEditDialog(hwnd, edit)) {
                    *meta = edit.meta;
                    vault.SaveProfile(id, meta->label, meta->username, meta->domain, edit.password, err);
                    RefreshList(*st);
                }
                return 0;
            }
            if (LOWORD(wParam) == IDC_CRED_DELETE) {
                const std::wstring id = SelectedCredId(st->list, *st->model);
                if (id.empty()) {
                    return 0;
                }
                if (MessageBoxW(hwnd, L"Delete this credential profile?", L"Confirm", MB_YESNO | MB_ICONQUESTION) ==
                    IDYES) {
                    CredentialVault vault;
                    std::wstring err;
                    vault.DeleteProfile(id, err);
                    st->model->RemoveCredential(id);
                    RefreshList(*st);
                }
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

bool ShowCredentialManager(HWND owner, ConnectionTreeModel& model) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = CredMgrProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"SecureRdpCredMgr";
        RegisterClassExW(&wc);
        registered = true;
    }

    CredMgrState st;
    st.model = &model;

    RECT ownerRc{};
    GetWindowRect(owner, &ownerRc);
    const int w = 360;
    const int h = 300;
    const int x = ownerRc.left + (ownerRc.right - ownerRc.left - w) / 2;
    const int y = ownerRc.top + (ownerRc.bottom - ownerRc.top - h) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"SecureRdpCredMgr", L"Credential Profiles",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, w, h, owner, nullptr,
                               GetModuleHandleW(nullptr), &st);
    if (!dlg) {
        return false;
    }
    ShowWindow(dlg, SW_SHOW);
    EnableWindow(owner, FALSE);
    MSG msg{};
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return true;
}
