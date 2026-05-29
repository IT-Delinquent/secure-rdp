#include "AppSettings.h"

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

namespace AppSettings {

bool Load(UiTheme::ThemePreference& theme, double& panelSplitRatio) {
    theme = UiTheme::ThemePreference::System;
    panelSplitRatio = 0.35;
    const std::wstring path = SettingsPath();
    if (path.empty()) {
        return false;
    }
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    try {
        const nlohmann::json doc = nlohmann::json::parse(in);
        const std::string themeStr = doc.value("theme", "system");
        if (themeStr == "light") {
            theme = UiTheme::ThemePreference::Light;
        } else if (themeStr == "dark") {
            theme = UiTheme::ThemePreference::Dark;
        } else {
            theme = UiTheme::ThemePreference::System;
        }
        panelSplitRatio = doc.value("panelSplitRatio", 0.35);
        if (panelSplitRatio < 0.15) {
            panelSplitRatio = 0.15;
        }
        if (panelSplitRatio > 0.85) {
            panelSplitRatio = 0.85;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool Save(UiTheme::ThemePreference theme, double panelSplitRatio) {
    const std::wstring path = SettingsPath();
    if (path.empty()) {
        return false;
    }
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
    }

    std::string themeStr = "system";
    switch (theme) {
        case UiTheme::ThemePreference::Light:
            themeStr = "light";
            break;
        case UiTheme::ThemePreference::Dark:
            themeStr = "dark";
            break;
        default:
            break;
    }

    if (panelSplitRatio < 0.15) {
        panelSplitRatio = 0.15;
    }
    if (panelSplitRatio > 0.85) {
        panelSplitRatio = 0.85;
    }

    nlohmann::json doc;
    doc["theme"] = themeStr;
    doc["panelSplitRatio"] = panelSplitRatio;
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

}  // namespace AppSettings
