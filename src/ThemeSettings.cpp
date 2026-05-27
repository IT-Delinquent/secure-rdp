#include "ThemeSettings.h"

#include "Util.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <shlobj.h>

namespace {

std::wstring SettingsPath() {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return L"";
    }
    std::wstring file = path;
    file += L"\\TinyRdp\\settings.json";
    return file;
}

}  // namespace

bool ThemeSettingsLoad(UiTheme::ThemePreference& out) {
    out = UiTheme::ThemePreference::System;
    const std::wstring path = SettingsPath();
    if (path.empty()) {
        return false;
    }
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    try {
        nlohmann::json doc = nlohmann::json::parse(in);
        const std::string theme = doc.value("theme", "system");
        if (theme == "light") {
            out = UiTheme::ThemePreference::Light;
        } else if (theme == "dark") {
            out = UiTheme::ThemePreference::Dark;
        } else {
            out = UiTheme::ThemePreference::System;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ThemeSettingsSave(UiTheme::ThemePreference preference) {
    const std::wstring path = SettingsPath();
    if (path.empty()) {
        return false;
    }
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
    }

    std::string theme = "system";
    switch (preference) {
        case UiTheme::ThemePreference::Light:
            theme = "light";
            break;
        case UiTheme::ThemePreference::Dark:
            theme = "dark";
            break;
        default:
            break;
    }

    nlohmann::json doc;
    doc["theme"] = theme;
    try {
        std::ofstream out(path, std::ios::trunc);
        if (!out) {
            return false;
        }
        out << doc.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}
