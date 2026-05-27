#include "UiTheme.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>

#include <commctrl.h>
#include <dwmapi.h>
#include <richedit.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <cwchar>
#include <string>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace {

HFONT g_font = nullptr;
HFONT g_fontSemibold = nullptr;
HBRUSH g_dialogBgBrush = nullptr;
HBRUSH g_inputBgBrush = nullptr;
HBRUSH g_menuBgBrush = nullptr;
UiTheme::ThemePreference g_preference = UiTheme::ThemePreference::System;
UiTheme::Palette g_palette{};

constexpr int kMenuPadY = 6;
constexpr int kMenuHorzPad = 28;
constexpr int kMenuInnerPad = 16;
constexpr int kMenuBarHorzPad = 8;
constexpr int kMenuAccelGap = 20;
BOOL CALLBACK EnumChildFontProc(HWND child, LPARAM lParam) {
    SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(lParam), TRUE);
    return TRUE;
}

const UiTheme::MenuItemData* AsMenuItem(LPARAM data) {
    const auto* item = reinterpret_cast<const UiTheme::MenuItemData*>(data);
    if (!item || item->magic != UiTheme::kMenuItemMagic) {
        return nullptr;
    }
    if (!item->separator && !item->text) {
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

void SetPaletteLight() {
    g_palette.background = RGB(250, 250, 252);
    g_palette.text = RGB(32, 32, 32);
    g_palette.accent = RGB(0, 103, 192);
    g_palette.treeBackground = RGB(255, 255, 255);
    g_palette.treeLine = RGB(220, 220, 225);
    g_palette.menuBackground = RGB(255, 255, 255);
    g_palette.menuText = RGB(32, 32, 32);
    g_palette.menuSelectedText = RGB(255, 255, 255);
    g_palette.useDarkModeChrome = false;
}

void SetPaletteDark() {
    g_palette.background = RGB(32, 32, 32);
    g_palette.text = RGB(230, 230, 230);
    g_palette.accent = RGB(96, 169, 255);
    g_palette.treeBackground = RGB(45, 45, 48);
    g_palette.treeLine = RGB(70, 70, 74);
    g_palette.menuBackground = RGB(45, 45, 48);
    g_palette.menuText = RGB(230, 230, 230);
    g_palette.menuSelectedText = RGB(255, 255, 255);
    g_palette.useDarkModeChrome = true;
}

}  // namespace

namespace UiTheme {

ThemePreference GetPreference() {
    return g_preference;
}

void SetPreference(ThemePreference preference) {
    g_preference = preference;
    RefreshPalette();
}

bool IsOsDarkMode() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return false;
    }
    return value == 0;
}

bool IsDarkEffective() {
    switch (g_preference) {
        case ThemePreference::Dark:
            return true;
        case ThemePreference::Light:
            return false;
        default:
            return IsOsDarkMode();
    }
}

const Palette& CurrentPalette() {
    return g_palette;
}

void RefreshPalette() {
    if (IsDarkEffective()) {
        SetPaletteDark();
    } else {
        SetPaletteLight();
    }
    if (g_dialogBgBrush) {
        DeleteObject(g_dialogBgBrush);
        g_dialogBgBrush = nullptr;
    }
    if (g_menuBgBrush) {
        DeleteObject(g_menuBgBrush);
        g_menuBgBrush = nullptr;
    }
    if (g_inputBgBrush) {
        DeleteObject(g_inputBgBrush);
        g_inputBgBrush = nullptr;
    }
}

void ApplyWindowChrome(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    const BOOL useDark = CurrentPalette().useDarkModeChrome ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
    if (IsDarkEffective()) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    } else {
        // Restore themed controls (otherwise Win32 can fall back to a classic look).
        SetWindowTheme(hwnd, L"Explorer", nullptr);
    }
}

void PaintMenuBarBand(HWND hwnd) {
    if (!hwnd) {
        return;
    }

    MENUBARINFO bar{};
    bar.cbSize = sizeof(bar);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &bar)) {
        return;
    }

    RECT rc = bar.rcBar;
    MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<POINT*>(&rc), 2);

    if (IsDarkEffective()) {
        // Cover the default light separator and match the tree top padding band.
        if (rc.bottom < kMainWindowTreeTopPadding) {
            rc.bottom = kMainWindowTreeTopPadding;
        }
    } else {
        rc.bottom += 1;
    }

    HDC hdc = GetWindowDC(hwnd);
    if (!hdc) {
        return;
    }

    const COLORREF band =
        IsDarkEffective() ? CurrentPalette().treeBackground : CurrentPalette().menuBackground;
    HBRUSH brush = CreateSolidBrush(band);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    ReleaseDC(hwnd, hdc);
}

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

void ReleaseDialogBrush() {
    if (g_dialogBgBrush) {
        DeleteObject(g_dialogBgBrush);
        g_dialogBgBrush = nullptr;
    }
    if (g_menuBgBrush) {
        DeleteObject(g_menuBgBrush);
        g_menuBgBrush = nullptr;
    }
    if (g_inputBgBrush) {
        DeleteObject(g_inputBgBrush);
        g_inputBgBrush = nullptr;
    }
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
    ReleaseDialogBrush();
}

HBRUSH DialogBackgroundBrush() {
    if (!g_dialogBgBrush) {
        g_dialogBgBrush = CreateSolidBrush(g_palette.background);
    }
    return g_dialogBgBrush;
}

HBRUSH MenuBackgroundBrush() {
    if (!g_menuBgBrush) {
        g_menuBgBrush = CreateSolidBrush(g_palette.menuBackground);
    }
    return g_menuBgBrush;
}

HBRUSH InputBackgroundBrush() {
    if (!g_inputBgBrush) {
        g_inputBgBrush = CreateSolidBrush(g_palette.treeBackground);
    }
    return g_inputBgBrush;
}

INT_PTR OnCtlColorStatic(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, g_palette.text);
    return reinterpret_cast<INT_PTR>(DialogBackgroundBrush());
}

bool IsComboBoxField(HWND control) {
    if (!control) {
        return false;
    }
    wchar_t cls[32]{};
    GetClassNameW(control, cls, 32);
    if (lstrcmpW(cls, L"ComboBox") == 0) {
        return true;
    }
    const HWND parent = GetParent(control);
    if (!parent) {
        return false;
    }
    GetClassNameW(parent, cls, 32);
    return lstrcmpW(cls, L"ComboBox") == 0;
}

INT_PTR OnDialogCtlColor(UINT msg, HDC hdc, HWND control) {
    SetTextColor(hdc, g_palette.text);
    if (msg == WM_CTLCOLORSTATIC && control) {
        wchar_t cls[32]{};
        GetClassNameW(control, cls, 32);
        // CBS_DROPDOWNLIST uses a child Static for the closed-field preview.
        if (lstrcmpW(cls, L"Edit") == 0 || IsComboBoxField(control)) {
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, g_palette.treeBackground);
            return reinterpret_cast<INT_PTR>(InputBackgroundBrush());
        }
        return OnCtlColorStatic(hdc);
    }
    SetBkMode(hdc, OPAQUE);
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) {
        SetBkColor(hdc, g_palette.treeBackground);
        return reinterpret_cast<INT_PTR>(InputBackgroundBrush());
    }
    SetBkColor(hdc, g_palette.background);
    return reinterpret_cast<INT_PTR>(DialogBackgroundBrush());
}

#ifndef CB_SETBACKGROUNDCOLOR
#define CB_SETBACKGROUNDCOLOR 0x0163
#endif
#ifndef CB_SETTEXTCOLOR
#define CB_SETTEXTCOLOR 0x0167
#endif
#ifndef LB_SETBKCOLOR
#define LB_SETBKCOLOR 0x01F2
#endif
#ifndef LB_SETTEXTCOLOR
#define LB_SETTEXTCOLOR 0x01F0
#endif

namespace {

constexpr UINT_PTR kComboSubclassId = 2;

using AllowDarkModeForWindowFn = bool(WINAPI*)(HWND hWnd, bool allow);

AllowDarkModeForWindowFn GetAllowDarkModeForWindow() {
    static AllowDarkModeForWindowFn fn = reinterpret_cast<AllowDarkModeForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(133)));
    return fn;
}

void EnableDarkModeForHwnd(HWND hwnd) {
    if (!hwnd || !UiTheme::IsDarkEffective()) {
        return;
    }
    if (AllowDarkModeForWindowFn allow = GetAllowDarkModeForWindow()) {
        allow(hwnd, true);
    }
}

LRESULT CALLBACK ComboSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ComboSubclassProc, kComboSubclassId);
            break;
        case CBN_DROPDOWN:
            UiTheme::ThemeComboDropdownList(hwnd);
            break;
        default:
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

}  // namespace

void ThemeComboDropdownList(HWND combo, bool ensureListExists) {
    if (!combo) {
        return;
    }
    COMBOBOXINFO cbi{};
    cbi.cbSize = sizeof(cbi);
    if (!GetComboBoxInfo(combo, &cbi) || !cbi.hwndList) {
        if (!ensureListExists) {
            return;
        }
        SendMessageW(combo, CB_SHOWDROPDOWN, TRUE, 0);
        SendMessageW(combo, CB_SHOWDROPDOWN, FALSE, 0);
        cbi = {};
        cbi.cbSize = sizeof(cbi);
        if (!GetComboBoxInfo(combo, &cbi) || !cbi.hwndList) {
            return;
        }
    }
    EnableDarkModeForHwnd(cbi.hwndList);
    const wchar_t* theme = IsDarkEffective() ? L"DarkMode_Explorer" : L"Explorer";
    SetWindowTheme(cbi.hwndList, theme, nullptr);
    const auto& palette = CurrentPalette();
    SendMessageW(cbi.hwndList, LB_SETBKCOLOR, 0, static_cast<LPARAM>(palette.treeBackground));
    SendMessageW(cbi.hwndList, LB_SETTEXTCOLOR, 0, static_cast<LPARAM>(palette.text));
}

void ApplyComboBox(HWND combo) {
    if (!combo) {
        return;
    }
    SendMessageW(combo, CCM_SETVERSION, COMCTL32_VERSION, 0);
    EnableDarkModeForHwnd(combo);
    const wchar_t* theme = IsDarkEffective() ? L"DarkMode_Explorer" : L"Explorer";
    SetWindowTheme(combo, theme, nullptr);
    SendMessageW(combo, CB_SETBACKGROUNDCOLOR, 0, static_cast<LPARAM>(g_palette.treeBackground));
    SendMessageW(combo, CB_SETTEXTCOLOR, 0, static_cast<LPARAM>(g_palette.text));

    COMBOBOXINFO cbi{};
    cbi.cbSize = sizeof(cbi);
    if (GetComboBoxInfo(combo, &cbi) && cbi.hwndItem) {
        EnableDarkModeForHwnd(cbi.hwndItem);
    }

    SetWindowSubclass(combo, ComboSubclassProc, kComboSubclassId, 0);
    InvalidateRect(combo, nullptr, TRUE);
}

bool IsRichEditClass(const wchar_t* cls) {
    return wcsncmp(cls, L"RichEdit", 8) == 0 || wcsncmp(cls, L"RICHEDIT", 8) == 0;
}

void ApplyRichEdit(HWND richEdit) {
    if (!richEdit) {
        return;
    }
    EnableDarkModeForHwnd(richEdit);
    const wchar_t* theme = IsDarkEffective() ? L"DarkMode_Explorer" : L"Explorer";
    if (IsDarkEffective()) {
        const DWORD ex = static_cast<DWORD>(GetWindowLongW(richEdit, GWL_EXSTYLE));
        SetWindowLongW(richEdit, GWL_EXSTYLE, ex & ~WS_EX_CLIENTEDGE);
    }
    SetWindowTheme(richEdit, theme, nullptr);
    SendMessageW(richEdit, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(g_palette.background));

    // Default format for new text; do not use SCF_ALL or accent/body runs are flattened.
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_EFFECTS;
    cf.dwEffects &= ~CFE_AUTOCOLOR;
    cf.crTextColor = g_palette.text;
    SendMessageW(richEdit, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));
    InvalidateRect(richEdit, nullptr, TRUE);
}

BOOL CALLBACK ThemeChildProc(HWND child, LPARAM lParam) {
    const wchar_t* theme = reinterpret_cast<const wchar_t*>(lParam);
    wchar_t cls[32]{};
    GetClassNameW(child, cls, 32);

    if (IsRichEditClass(cls)) {
        ApplyRichEdit(child);
    } else {
        if (lstrcmpW(cls, L"Edit") == 0) {
            if (IsDarkEffective()) {
                const DWORD ex = static_cast<DWORD>(GetWindowLongW(child, GWL_EXSTYLE));
                SetWindowLongW(child, GWL_EXSTYLE, ex & ~WS_EX_CLIENTEDGE);
            }
            SetWindowTheme(child, theme, nullptr);
            SendMessageW(child, WM_THEMECHANGED, 0, 0);
        } else if (lstrcmpW(cls, L"ComboBox") == 0) {
            ApplyComboBox(child);
        } else if (lstrcmpW(cls, L"ListBox") == 0) {
            SetWindowTheme(child, theme, nullptr);
            SendMessageW(child, WM_THEMECHANGED, 0, 0);
        } else if (lstrcmpW(cls, L"Button") == 0) {
            SetWindowTheme(child, theme, nullptr);
            SendMessageW(child, WM_THEMECHANGED, 0, 0);
        } else {
            SendMessageW(child, WM_THEMECHANGED, 0, 0);
        }
    }

    EnumChildWindows(child, ThemeChildProc, lParam);
    return TRUE;
}

void ApplyDialog(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    ApplyWindowChrome(hwnd);
    Apply(hwnd);
    const wchar_t* theme = IsDarkEffective() ? L"DarkMode_Explorer" : L"Explorer";
    EnumChildWindows(hwnd, ThemeChildProc, reinterpret_cast<LPARAM>(theme));
    InvalidateRect(hwnd, nullptr, TRUE);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

bool HandleDialogMessages(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result) {
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, DialogBackgroundBrush());
            result = 1;
            return true;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN:
            result = OnDialogCtlColor(msg, reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
            return true;
        default:
            break;
    }
    (void)hwnd;
    return false;
}

void Apply(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    const HFONT font = AcquireFont();
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    EnumChildWindows(hwnd, EnumChildFontProc, reinterpret_cast<LPARAM>(font));
}

#ifndef TVM_SETBORDERCOLOR
#define TVM_SETBORDERCOLOR (TV_FIRST + 35)
#endif
#ifndef TreeView_SetBorderColor
#define TreeView_SetBorderColor(hwnd, clr) \
    static_cast<COLORREF>(SendMessageW((hwnd), TVM_SETBORDERCOLOR, 0, static_cast<LPARAM>(clr)))
#endif

void ApplyTree(HWND tree) {
    if (!tree) {
        return;
    }
    if (IsDarkEffective()) {
        if (FAILED(SetWindowTheme(tree, L"DarkMode_Explorer", nullptr))) {
            SetWindowTheme(tree, L"", L"");
        }
    } else {
        SetWindowTheme(tree, L"Explorer", nullptr);
    }
    SendMessageW(tree, WM_SETFONT, reinterpret_cast<WPARAM>(AcquireFont()), TRUE);
    TreeView_SetBkColor(tree, g_palette.treeBackground);
    TreeView_SetTextColor(tree, g_palette.text);
    TreeView_SetLineColor(tree, g_palette.treeLine);
    TreeView_SetBorderColor(tree, g_palette.treeBackground);
    InvalidateRect(tree, nullptr, TRUE);
}

void ApplyMenuColors(HMENU menu) {
    if (!menu) {
        return;
    }
    MENUINFO mi{};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    mi.hbrBack = MenuBackgroundBrush();
    SetMenuInfo(menu, &mi);
}

void ApplyWindowMenus(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    ApplyMenuColors(GetMenu(hwnd));
}

void RedrawMenus(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    DrawMenuBar(hwnd);
    const DWORD threadId = GetCurrentThreadId();
    for (HWND menuWnd = FindWindowExW(nullptr, nullptr, L"#32768", nullptr); menuWnd;
         menuWnd = FindWindowExW(nullptr, menuWnd, L"#32768", nullptr)) {
        DWORD menuThreadId = 0;
        GetWindowThreadProcessId(menuWnd, &menuThreadId);
        if (menuThreadId == threadId) {
            InvalidateRect(menuWnd, nullptr, TRUE);
            UpdateWindow(menuWnd);
        }
    }
}

void ApplyMainWindow(HWND hwnd, HWND tree) {
    ApplyWindowChrome(hwnd);
    Apply(hwnd);
    ApplyTree(tree);
    ApplyWindowMenus(hwnd);
    RedrawMenus(hwnd);
    PaintMenuBarBand(hwnd);
    // Force a non-client repaint so the menu bar band doesn't keep stale (white) pixels.
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

void MenuInitItem(MenuItemData& item, const wchar_t* text) {
    item.magic = kMenuItemMagic;
    item.text = text;
    item.separator = false;
}

void MenuInitSeparator(MenuItemData& item) {
    item.magic = kMenuItemMagic;
    item.text = L"";
    item.separator = true;
    item.compact = false;
}

bool MenuAppendSeparator(HMENU menu, UINT_PTR id, MenuItemData& item) {
    if (!menu) {
        return false;
    }
    item.magic = kMenuItemMagic;
    item.separator = true;
    return AppendMenuW(menu, MF_OWNERDRAW | MF_DISABLED | MF_GRAYED, id, reinterpret_cast<LPCTSTR>(&item)) !=
           FALSE;
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

    if (item->separator) {
        mis->itemHeight = 9;
        mis->itemWidth = 160;
        return true;
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
        mis->itemWidth = kMenuBarHorzPad * 2 + labelSize.cx;
    } else {
        mis->itemHeight = tm.tmHeight + tm.tmExternalLeading + kMenuPadY * 2;
        mis->itemWidth = kMenuHorzPad + labelSize.cx + kMenuInnerPad;
        if (!accel.empty()) {
            mis->itemWidth += kMenuAccelGap + accelSize.cx;
        }
        mis->itemWidth += kMenuInnerPad;
    }
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

    if (item->separator) {
        FillRect(dis->hDC, &dis->rcItem, MenuBackgroundBrush());
        const int y = (dis->rcItem.top + dis->rcItem.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, 1, g_palette.treeLine);
        const HPEN oldPen = static_cast<HPEN>(SelectObject(dis->hDC, pen));
        MoveToEx(dis->hDC, dis->rcItem.left + kMenuHorzPad, y, nullptr);
        LineTo(dis->hDC, dis->rcItem.right - kMenuInnerPad, y);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);
        return true;
    }

    std::wstring label;
    std::wstring accel;
    SplitMenuText(item->text, label, accel);

    const bool selected = (dis->itemState & ODS_SELECTED) != 0;
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bg = g_palette.menuBackground;
    COLORREF fg = g_palette.menuText;
    if (disabled) {
        fg = GetSysColor(COLOR_GRAYTEXT);
    } else if (selected) {
        bg = g_palette.accent;
        fg = g_palette.menuSelectedText;
    }

    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(dis->hDC, &dis->rcItem, brush);
    DeleteObject(brush);

    if ((dis->itemState & ODS_CHECKED) != 0) {
        const int checkX = dis->rcItem.left + 10;
        const int checkY = (dis->rcItem.top + dis->rcItem.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, 2, fg);
        const HPEN oldPen = static_cast<HPEN>(SelectObject(dis->hDC, pen));
        MoveToEx(dis->hDC, checkX, checkY, nullptr);
        LineTo(dis->hDC, checkX + 4, checkY + 5);
        LineTo(dis->hDC, checkX + 11, checkY - 4);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);
    }

    const HFONT font = AcquireFont();
    const HFONT oldFont = static_cast<HFONT>(SelectObject(dis->hDC, font));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, fg);

    RECT textRc = dis->rcItem;
    const int horzPad = item->compact ? kMenuBarHorzPad : kMenuHorzPad;
    const int innerPad = item->compact ? kMenuBarHorzPad : kMenuInnerPad;
    textRc.left += horzPad;
    textRc.right -= innerPad;
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
