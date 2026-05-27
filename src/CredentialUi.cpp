#include "CredentialUi.h"

#include "UiTheme.h"

#include <commctrl.h>
#include <windowsx.h>

void FillCredentialCombo(HWND combo, ConnectionTreeModel& model, const std::wstring& selectedId) {
    if (!combo) {
        return;
    }

    UiTheme::ApplyComboBox(combo);

    ComboBox_ResetContent(combo);
    ComboBox_AddString(combo, L"(None)");
    int sel = 0;
    int idx = 1;
    for (const auto& c : model.Credentials()) {
        ComboBox_AddString(combo, c.label.c_str());
        if (c.id == selectedId) {
            sel = idx;
        }
        ++idx;
    }

    const int count = ComboBox_GetCount(combo);
    if (count > 0) {
        ComboBox_SetCurSel(combo, sel);
        const WPARAM visible = static_cast<WPARAM>(count > 16 ? 16 : count);
        SendMessageW(combo, CB_SETMINVISIBLE, visible, 0);
    }

    RECT rc{};
    GetWindowRect(combo, &rc);
    const int width = rc.right - rc.left;
    if (width > 0) {
        SendMessageW(combo, CB_SETDROPPEDWIDTH, static_cast<WPARAM>(width), 0);
    }

    UiTheme::ThemeComboDropdownList(combo, true);
    RedrawWindow(combo, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

std::wstring CredentialIdFromCombo(HWND combo, ConnectionTreeModel& model) {
    const int sel = ComboBox_GetCurSel(combo);
    if (sel <= 0) {
        return L"";
    }
    const size_t credIndex = static_cast<size_t>(sel - 1);
    const auto& creds = model.Credentials();
    if (credIndex >= creds.size()) {
        return L"";
    }
    return creds[credIndex].id;
}
