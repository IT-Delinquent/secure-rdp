#pragma once

#include <windows.h>
#include <commctrl.h>

namespace UiTheme {

enum class ThemePreference { System, Light, Dark };

struct Palette {
    COLORREF background = RGB(250, 250, 252);
    COLORREF text = RGB(32, 32, 32);
    COLORREF accent = RGB(0, 103, 192);
    COLORREF treeBackground = RGB(255, 255, 255);
    COLORREF treeLine = RGB(220, 220, 225);
    COLORREF menuBackground = RGB(255, 255, 255);
    COLORREF menuText = RGB(32, 32, 32);
    COLORREF menuSelectedText = RGB(255, 255, 255);
    bool useDarkModeChrome = false;
};

constexpr UINT kMenuItemMagic = 0x53524D50;  // 'SRMP'
constexpr int kMainWindowTreeTopPadding = 20;

struct MenuItemData {
    UINT magic = kMenuItemMagic;
    const wchar_t* text = nullptr;
    bool compact = false;
    bool separator = false;
};

ThemePreference GetPreference();
void SetPreference(ThemePreference preference);
bool IsOsDarkMode();
bool IsDarkEffective();
const Palette& CurrentPalette();

void RefreshPalette();
void ApplyWindowChrome(HWND hwnd);
void PaintMenuBarBand(HWND hwnd);
void ApplyDialog(HWND hwnd);
void ApplyRichEdit(HWND richEdit);
void ApplyComboBox(HWND combo);
bool HandleDialogMessages(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result);
void Apply(HWND hwnd);
void ApplyTree(HWND tree);
void ApplyMainWindow(HWND hwnd, HWND tree);
void ApplyMenuColors(HMENU menu);
void ApplyWindowMenus(HWND hwnd);
void RedrawMenus(HWND hwnd);
HBRUSH DialogBackgroundBrush();
HBRUSH InputBackgroundBrush();
INT_PTR OnCtlColorStatic(HDC hdc);
INT_PTR OnDialogCtlColor(UINT msg, HDC hdc, HWND control);

HFONT AcquireFont(int pointSize = 9, bool semibold = false);
void ReleaseFont();

void MenuInitItem(MenuItemData& item, const wchar_t* text);
void MenuInitSeparator(MenuItemData& item);
bool MenuAppend(HMENU menu, UINT_PTR id, MenuItemData& item, UINT flags = 0);
bool MenuAppendSeparator(HMENU menu, UINT_PTR id, MenuItemData& item);
bool MenuOnMeasureItem(LPARAM lParam);
bool MenuOnDrawItem(LPARAM lParam);

HICON LoadStockIcon(int iconId, bool smallIcon = true);

}  // namespace UiTheme
