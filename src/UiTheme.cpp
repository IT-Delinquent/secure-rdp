#include "UiTheme.h"

#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace {

HFONT g_font = nullptr;
HFONT g_fontSemibold = nullptr;

BOOL CALLBACK EnumChildFontProc(HWND child, LPARAM lParam) {
    SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(lParam), TRUE);
    return TRUE;
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
