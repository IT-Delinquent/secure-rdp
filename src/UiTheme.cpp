#include "UiTheme.h"

#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <cwchar>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace {

HFONT g_font = nullptr;
HFONT g_fontSemibold = nullptr;

constexpr int kMenuPadY = 6;
constexpr int kMenuHorzPad = 28;
constexpr int kMenuInnerPad = 16;
constexpr int kMenuAccelGap = 20;

BOOL CALLBACK EnumChildFontProc(HWND child, LPARAM lParam) {
    SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(lParam), TRUE);
    return TRUE;
}

const UiTheme::MenuItemData* AsMenuItem(LPARAM data) {
    const auto* item = reinterpret_cast<const UiTheme::MenuItemData*>(data);
    if (!item || item->magic != UiTheme::kMenuItemMagic || !item->text) {
        return nullptr;
    }
    return item;
}

void SplitMenuText(const wchar_t* text, std::wstring& label, std::wstring& accel) {
    label.clear();
    accel.clear();
    if (!text) {
        return;
    }
    const wchar_t* tab = wcschr(text, L'\t');
    if (tab) {
        label.assign(text, tab - text);
        accel = tab + 1;
    } else {
        label = text;
    }
}

}  // namespace

namespace UiTheme {

HFONT AcquireFont(int pointSize, bool semibold) {
    if (semibold) {
        if (!g_fontSemibold) {
            g_fontSemibold = CreateFontW(-pointSize * 96 / 72, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                         VARIABLE_PITCH, L"Segoe UI");
        }
        return g_fontSemibold ? g_fontSemibold : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    if (!g_font) {
        g_font = CreateFontW(-pointSize * 96 / 72, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    }
    return g_font ? g_font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

void ReleaseFont() {
    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }
    if (g_fontSemibold) {
        DeleteObject(g_fontSemibold);
        g_fontSemibold = nullptr;
    }
}

void Apply(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    const HFONT font = AcquireFont();
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    EnumChildWindows(hwnd, EnumChildFontProc, reinterpret_cast<LPARAM>(font));
}

void ApplyTree(HWND tree) {
    if (!tree) {
        return;
    }
    SetWindowTheme(tree, L"Explorer", nullptr);
    SendMessageW(tree, WM_SETFONT, reinterpret_cast<WPARAM>(AcquireFont()), TRUE);
    TreeView_SetBkColor(tree, RGB(255, 255, 255));
    TreeView_SetLineColor(tree, RGB(220, 220, 225));
}

void MenuInitItem(MenuItemData& item, const wchar_t* text) {
    item.magic = kMenuItemMagic;
    item.text = text;
}

bool MenuAppend(HMENU menu, UINT_PTR id, MenuItemData& item, UINT flags) {
    if (!menu || !item.text) {
        return false;
    }
    item.magic = kMenuItemMagic;
    return AppendMenuW(menu, MF_OWNERDRAW | flags, id, reinterpret_cast<LPCTSTR>(&item)) != FALSE;
}

bool MenuOnMeasureItem(LPARAM lParam) {
    auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
    const MenuItemData* item = AsMenuItem(mis->itemData);
    if (!item) {
        return false;
    }

    std::wstring label;
    std::wstring accel;
    SplitMenuText(item->text, label, accel);

    HDC hdc = GetDC(nullptr);
    if (!hdc) {
        return false;
    }
    const HFONT font = AcquireFont();
    const HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    SIZE labelSize{};
    SIZE accelSize{};
    GetTextExtentPoint32W(hdc, label.c_str(), static_cast<int>(label.size()), &labelSize);
    if (!accel.empty()) {
        GetTextExtentPoint32W(hdc, accel.c_str(), static_cast<int>(accel.size()), &accelSize);
    }

    TEXTMETRICW tm{};
    GetTextMetricsW(hdc, &tm);
    SelectObject(hdc, oldFont);
    ReleaseDC(nullptr, hdc);

    if (item->compact) {
        mis->itemHeight = tm.tmHeight + 4;
    } else {
        mis->itemHeight = tm.tmHeight + tm.tmExternalLeading + kMenuPadY * 2;
    }
    mis->itemWidth = kMenuHorzPad + labelSize.cx + kMenuInnerPad;
    if (!accel.empty()) {
        mis->itemWidth += kMenuAccelGap + accelSize.cx;
    }
    mis->itemWidth += kMenuInnerPad;
    return true;
}

bool MenuOnDrawItem(LPARAM lParam) {
    const auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
    if (dis->CtlType != ODT_MENU) {
        return false;
    }
    const MenuItemData* item = AsMenuItem(dis->itemData);
    if (!item) {
        return false;
    }

    std::wstring label;
    std::wstring accel;
    SplitMenuText(item->text, label, accel);

    const bool selected = (dis->itemState & ODS_SELECTED) != 0;
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bg = RGB(255, 255, 255);
    COLORREF fg = RGB(32, 32, 32);
    if (disabled) {
        fg = GetSysColor(COLOR_GRAYTEXT);
    } else if (selected) {
        bg = kAccent;
        fg = RGB(255, 255, 255);
    }

    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(dis->hDC, &dis->rcItem, brush);
    DeleteObject(brush);

    const HFONT font = AcquireFont();
    const HFONT oldFont = static_cast<HFONT>(SelectObject(dis->hDC, font));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, fg);

    RECT textRc = dis->rcItem;
    textRc.left += kMenuHorzPad;
    textRc.right -= kMenuInnerPad;
    if (!accel.empty()) {
        SIZE accelSize{};
        GetTextExtentPoint32W(dis->hDC, accel.c_str(), static_cast<int>(accel.size()), &accelSize);
        RECT accelRc = textRc;
        accelRc.left = accelRc.right - accelSize.cx;
        DrawTextW(dis->hDC, accel.c_str(), static_cast<int>(accel.size()), &accelRc,
                  DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
        textRc.right = accelRc.left - kMenuAccelGap;
    }
    DrawTextW(dis->hDC, label.c_str(), static_cast<int>(label.size()), &textRc,
              DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_HIDEPREFIX);

    SelectObject(dis->hDC, oldFont);
    return true;
}

HICON LoadStockIcon(int iconId, bool smallIcon) {
    SHSTOCKICONINFO info{};
    info.cbSize = sizeof(info);
    UINT flags = SHGSI_ICON | (smallIcon ? SHGSI_SMALLICON : SHGSI_LARGEICON);
    if (FAILED(SHGetStockIconInfo(static_cast<SHSTOCKICONID>(iconId), flags, &info))) {
        return nullptr;
    }
    return info.hIcon;
}

}  // namespace UiTheme
