#pragma once

#include "ConnectionTreeModel.h"

#include <windows.h>

struct AssignCredentialResult {
    bool accepted = false;
    std::wstring credentialId;
};

bool ShowAssignCredentialDialog(HWND owner, ConnectionTreeModel& model, int sessionCount,
                                AssignCredentialResult& result);
