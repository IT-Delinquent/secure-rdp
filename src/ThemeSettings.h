#pragma once

#include "UiTheme.h"

#include <windows.h>

bool ThemeSettingsLoad(UiTheme::ThemePreference& out);
bool ThemeSettingsSave(UiTheme::ThemePreference preference);
