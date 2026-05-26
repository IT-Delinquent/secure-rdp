#pragma once

#include <string>
#include <windows.h>

struct FolderDialogResult {
    bool accepted = false;
    std::wstring name;
};

bool ShowFolderDialog(HWND owner, const std::wstring& initialName, FolderDialogResult& result);
