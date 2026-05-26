#pragma once

#include <windows.h>

namespace UiTheme {

constexpr COLORREF kBackground = RGB(250, 250, 252);
constexpr COLORREF kAccent = RGB(0, 103, 192);

constexpr UINT kMenuItemMagic = 0x53524D50;  // 'SRMP'

struct MenuItemData {
    UINT magic = kMenuItemMagic;
    const wchar_t* text = nullptr;
    bool compact = false;  // menu bar top-level labels
};

HFONT AcquireFont(int pointSize = 9, bool semibold = false);
void ReleaseFont();

void Apply(HWND hwnd);
void ApplyTree(HWND tree);

void MenuInitItem(MenuItemData& item, const wchar_t* text);
bool MenuAppend(HMENU menu, UINT_PTR id, MenuItemData& item, UINT flags = 0);
bool MenuOnMeasureItem(LPARAM lParam);
bool MenuOnDrawItem(LPARAM lParam);

HICON LoadStockIcon(int iconId, bool smallIcon = true);

}  // namespace UiTheme
