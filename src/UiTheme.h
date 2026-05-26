#pragma once

#include <windows.h>

namespace UiTheme {

constexpr COLORREF kBackground = RGB(250, 250, 252);
constexpr COLORREF kAccent = RGB(0, 103, 192);

HFONT AcquireFont(int pointSize = 9, bool semibold = false);
void ReleaseFont();

void Apply(HWND hwnd);
void ApplyTree(HWND tree);

HICON LoadStockIcon(int iconId, bool smallIcon = true);

}  // namespace UiTheme
