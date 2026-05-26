#pragma once

#include "ConnectionTreeModel.h"

#include <windows.h>

struct SessionDialogResult {
    bool accepted = false;
    std::wstring name;
    std::wstring host;
    int port = 3389;
    std::wstring credentialId;
};

bool ShowSessionDialog(HWND owner, ConnectionTreeModel& model, const TreeNode* existing, SessionDialogResult& result);
