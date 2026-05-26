#pragma once

#include "ConnectionTreeModel.h"

#include <string>

struct MRemoteNgImportStats {
    int folders = 0;
    int sessions = 0;
    int skippedNonRdp = 0;
};

class MRemoteNgExchange {
public:
    static bool ImportFile(const std::wstring& path, ConnectionTreeModel& model, TreeNode& targetParent,
                           MRemoteNgImportStats& stats, std::wstring& error);

    // Exports subtree as mRemoteNG XML. For the app root folder, children are written as top-level nodes.
    static bool ExportFile(const std::wstring& path, const TreeNode& subtree, const ConnectionTreeModel& model,
                           std::wstring& error);
};
