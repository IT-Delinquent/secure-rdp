#include "ThemeSettings.h"

#include "AppSettings.h"

bool ThemeSettingsLoad(UiTheme::ThemePreference& out) {
    double splitRatio = 0.35;
    return AppSettings::Load(out, splitRatio);
}

bool ThemeSettingsSave(UiTheme::ThemePreference preference) {
    UiTheme::ThemePreference theme = UiTheme::ThemePreference::System;
    double splitRatio = 0.35;
    AppSettings::Load(theme, splitRatio);
    return AppSettings::Save(preference, splitRatio);
}
