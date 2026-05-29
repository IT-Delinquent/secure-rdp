#pragma once

#include "UiTheme.h"

namespace AppSettings {

bool Load(UiTheme::ThemePreference& theme, double& panelSplitRatio);
bool Save(UiTheme::ThemePreference theme, double panelSplitRatio);

}  // namespace AppSettings
