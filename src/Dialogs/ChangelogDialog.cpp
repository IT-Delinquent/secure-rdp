#include "ChangelogDialog.h"

#include "ModalLoop.h"
#include "Resource.h"
#include "UiTheme.h"
#include "Version.h"

#include <richedit.h>
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

namespace {

constexpr wchar_t kStateProp[] = L"SecureRdpChangelogDlgState";
constexpr int IDC_CHANGELOG_TEXT = 3501;

#ifndef MSFTEDIT_CLASS
#define MSFTEDIT_CLASS L"RICHEDIT50W"
#endif

struct ChangelogSection {
    std::wstring name;
    std::vector<std::wstring> items;
};

struct ChangelogRelease {
    std::wstring version;
    std::wstring date;
    std::vector<ChangelogSection> sections;
};

std::wstring Trim(const std::wstring& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == L' ' || s[start] == L'\t' || s[start] == L'\r' || s[start] == L'\n')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == L' ' || s[end - 1] == L'\t' || s[end - 1] == L'\r' || s[end - 1] == L'\n')) {
        --end;
    }
    return s.substr(start, end - start);
}

bool StartsWith(const std::wstring& s, const wchar_t* prefix) {
    const size_t len = wcslen(prefix);
    return s.size() >= len && _wcsnicmp(s.c_str(), prefix, static_cast<int>(len)) == 0;
}

bool ParseVersionLine(const std::wstring& line, std::wstring& version, std::wstring& date) {
    // [1.0.1] - 2026-05-26  or  ## [1.0.1] - 2026-05-26
    std::wstring trimmed = Trim(line);
    if (trimmed.empty()) {
        return false;
    }
    while (!trimmed.empty() && trimmed.front() == L'#') {
        trimmed.erase(trimmed.begin());
    }
    trimmed = Trim(trimmed);
    if (trimmed.empty() || trimmed.front() != L'[') {
        return false;
    }
    const size_t close = trimmed.find(L']');
    if (close == std::wstring::npos || close < 2) {
        return false;
    }
    version = trimmed.substr(1, close - 1);
    std::wstring rest = Trim(trimmed.substr(close + 1));
    if (StartsWith(rest, L"-")) {
        date = Trim(rest.substr(1));
    }
    return !version.empty();
}

bool ParseSectionLine(const std::wstring& line, std::wstring& sectionName) {
    std::wstring trimmed = Trim(line);
    if (trimmed.empty() || trimmed.front() == L'-' || trimmed.front() == L'[') {
        return false;
    }
    while (!trimmed.empty() && trimmed.front() == L'#') {
        trimmed.erase(trimmed.begin());
    }
    trimmed = Trim(trimmed);
    if (trimmed.empty() || trimmed.front() == L'-' || trimmed.front() == L'[') {
        return false;
    }
    sectionName = std::move(trimmed);
    return true;
}

bool ParseBulletLine(const std::wstring& line, std::wstring& item) {
    std::wstring trimmed = Trim(line);
    if (trimmed.empty()) {
        return false;
    }
    if (trimmed.front() == L'-') {
        item = Trim(trimmed.substr(1));
        return !item.empty();
    }
    if (trimmed.front() == L'*') {
        item = Trim(trimmed.substr(1));
        return !item.empty();
    }
    return false;
}

std::vector<ChangelogRelease> ParseChangelog(const std::wstring& text) {
    std::vector<ChangelogRelease> releases;
    ChangelogRelease* current = nullptr;
    ChangelogSection* currentSection = nullptr;

    size_t lineStart = 0;
    while (lineStart <= text.size()) {
        size_t lineEnd = text.find(L'\n', lineStart);
        if (lineEnd == std::wstring::npos) {
            lineEnd = text.size();
        }
        const std::wstring line = text.substr(lineStart, lineEnd - lineStart);

        std::wstring version;
        std::wstring date;
        if (ParseVersionLine(line, version, date)) {
            releases.push_back({});
            current = &releases.back();
            current->version = std::move(version);
            current->date = std::move(date);
            currentSection = nullptr;
        } else {
            std::wstring sectionName;
            if (current && ParseSectionLine(line, sectionName)) {
                current->sections.push_back({});
                current->sections.back().name = std::move(sectionName);
                currentSection = &current->sections.back();
            } else {
                std::wstring item;
                if (currentSection && ParseBulletLine(line, item)) {
                    currentSection->items.push_back(std::move(item));
                }
            }
        }

        if (lineEnd >= text.size()) {
            break;
        }
        lineStart = lineEnd + 1;
    }

    return releases;
}

std::wstring LoadChangelogText() {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(IDR_CHANGELOG), RT_RCDATA);
    if (!res) {
        return L"";
    }
    HGLOBAL loaded = LoadResource(module, res);
    if (!loaded) {
        return L"";
    }
    const void* data = LockResource(loaded);
    const DWORD size = SizeofResource(module, res);
    if (!data || size == 0) {
        return L"";
    }
    const char* bytes = static_cast<const char*>(data);
    const bool utf8Bom = size >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
                         static_cast<unsigned char>(bytes[1]) == 0xBB &&
                         static_cast<unsigned char>(bytes[2]) == 0xBF;
    const int offset = utf8Bom ? 3 : 0;
    const int charCount = static_cast<int>(size - offset);
    if (charCount <= 0) {
        return L"";
    }
    const int wideLen = MultiByteToWideChar(CP_UTF8, 0, bytes + offset, charCount, nullptr, 0);
    if (wideLen <= 0) {
        return L"";
    }
    std::wstring text(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes + offset, charCount, text.data(), wideLen);
    return text;
}

bool EnsureRichEdit() {
    static bool loaded = []() {
        return LoadLibraryW(L"Msftedit.dll") != nullptr;
    }();
    return loaded;
}

void SelectEnd(HWND richEdit) {
    const int len = GetWindowTextLengthW(richEdit);
    CHARRANGE range{};
    range.cpMin = len;
    range.cpMax = len;
    SendMessageW(richEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
}

void SetSelectionFormat(HWND richEdit, bool bold, int heightTwips, COLORREF textColor, int startIndentTwips) {
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_BOLD | CFM_SIZE | CFM_COLOR | CFM_EFFECTS;
    cf.dwEffects &= ~CFE_AUTOCOLOR;
    if (bold) {
        cf.dwEffects |= CFE_BOLD;
    } else {
        cf.dwEffects &= ~CFE_BOLD;
    }
    cf.yHeight = heightTwips;
    cf.crTextColor = textColor;
    SendMessageW(richEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

    if (startIndentTwips > 0) {
        PARAFORMAT2 pf{};
        pf.cbSize = sizeof(pf);
        pf.dwMask = PFM_STARTINDENT;
        pf.dxStartIndent = startIndentTwips;
        SendMessageW(richEdit, EM_SETPARAFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&pf));
    }
}

void AppendLine(HWND richEdit, const std::wstring& text, bool bold, int heightTwips, COLORREF textColor,
                int startIndentTwips) {
    SelectEnd(richEdit);
    SetSelectionFormat(richEdit, bold, heightTwips, textColor, startIndentTwips);
    SendMessageW(richEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(richEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r\n"));
}

void PopulateStructuredChangelog(HWND richEdit) {
    const std::vector<ChangelogRelease> releases = ParseChangelog(LoadChangelogText());
    const UiTheme::Palette& palette = UiTheme::CurrentPalette();
    const COLORREF accent = palette.accent;
    const COLORREF body = palette.text;

    SetWindowTextW(richEdit, L"");

    if (releases.empty()) {
        AppendLine(richEdit, L"No release notes are available.", false, 200, body, 0);
        return;
    }

    for (size_t i = 0; i < releases.size(); ++i) {
        const ChangelogRelease& release = releases[i];
        if (i > 0) {
            AppendLine(richEdit, L"", false, 200, body, 0);
        }

        std::wstring heading = L"Version " + release.version;
        if (!release.date.empty()) {
            heading += L"  \u2014  " + release.date;
        }
        AppendLine(richEdit, heading, true, 280, accent, 0);

        for (const ChangelogSection& section : release.sections) {
            AppendLine(richEdit, section.name, true, 220, body, 0);

            for (const std::wstring& item : section.items) {
                AppendLine(richEdit, L"\u2022  " + item, false, 200, body, 240);
            }

            if (section.items.empty()) {
                AppendLine(richEdit, L"(none)", false, 200, body, 240);
            }
        }
    }

    CHARRANGE range{};
    range.cpMin = 0;
    range.cpMax = 0;
    SendMessageW(richEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
}

struct State {
    HWND text = nullptr;
};

void Layout(HWND hwnd, const State* st) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (rc.bottom < 120) {
        return;
    }
    const int margin = 16;
    const int btnH = 32;
    const int btnW = 96;
    const int btnY = rc.bottom - margin - btnH;
    const int textBottom = btnY - margin;

    if (st && st->text) {
        SetWindowPos(st->text, nullptr, margin, margin, rc.right - 2 * margin, textBottom - margin, SWP_NOZORDER);
    }
    if (HWND ok = GetDlgItem(hwnd, IDOK)) {
        SetWindowPos(ok, nullptr, rc.right - margin - btnW, btnY, btnW, btnH, SWP_NOZORDER);
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

            if (!EnsureRichEdit()) {
                st->text = CreateWindowExW(
                    WS_EX_CLIENTEDGE, L"EDIT", L"Rich Edit is not available on this system.",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 10, 10,
                    hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CHANGELOG_TEXT)), nullptr, nullptr);
            } else {
                st->text = CreateWindowExW(
                    WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 10, 10,
                    hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CHANGELOG_TEXT)), nullptr, nullptr);
            }

            CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, hwnd,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), nullptr, nullptr);
            UiTheme::ApplyDialog(hwnd);
            if (st->text && EnsureRichEdit()) {
                PopulateStructuredChangelog(st->text);
                UiTheme::ApplyRichEdit(st->text);
            } else if (st->text) {
                UiTheme::ApplyRichEdit(st->text);
            }
            Layout(hwnd, st);
            return 0;
        }
        case WM_SIZE:
            Layout(hwnd, st);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
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

void ShowChangelogDialog(HWND owner) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::DialogBackgroundBrush();
        wc.lpszClassName = L"SecureRdpChangelogDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    auto state = std::make_unique<State>();

    std::wstring title = L"Changelog ";
    title += SECURE_RDP_VERSION;

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE;

    HWND dlg = CreateWindowExW(exStyle, L"SecureRdpChangelogDlg", title.c_str(), style, 0, 0, 100, 100, owner, nullptr,
                               GetModuleHandleW(nullptr), state.get());
    if (!dlg) {
        return;
    }

    state.release();

    CenterWindowOnOwner(dlg, owner, 560, 460);
    SendMessageW(dlg, WM_SIZE, 0, MAKELPARAM(560, 460));
    UiTheme::ApplyDialog(dlg);
    if (HWND rich = GetDlgItem(dlg, IDC_CHANGELOG_TEXT)) {
        UiTheme::ApplyRichEdit(rich);
        InvalidateRect(rich, nullptr, TRUE);
    }
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    EnableWindow(owner, FALSE);

    RunModalLoop(dlg);

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
}
