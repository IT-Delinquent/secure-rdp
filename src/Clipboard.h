#pragma once

#include "ConnectionTreeModel.h"

#include <memory>
#include <string>
#include <windows.h>

class ClipboardManager {
public:
    static constexpr const wchar_t* kFormatName = L"SecureRdp/NodeV1";

    static bool CopySubtree(const TreeNode& node);
    static std::unique_ptr<TreeNode> PasteSubtree();
    static bool HasPasteData();
    static UINT AcquireNodeClipboardFormat();
};
