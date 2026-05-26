#pragma once

#include "ConnectionTreeModel.h"
#include "CredentialVault.h"

#include <string>

class RdpLauncher {
public:
    explicit RdpLauncher(ConnectionTreeModel& model);

    bool Connect(const TreeNode& session, std::wstring& error);

private:
    ConnectionTreeModel& model_;
    CredentialVault vault_;
};
