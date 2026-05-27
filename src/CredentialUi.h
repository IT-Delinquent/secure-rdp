#pragma once

#include "ConnectionTreeModel.h"

#include <string>
#include <windows.h>

void FillCredentialCombo(HWND combo, ConnectionTreeModel& model, const std::wstring& selectedId);
std::wstring CredentialIdFromCombo(HWND combo, ConnectionTreeModel& model);
