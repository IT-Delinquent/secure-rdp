#pragma once

#include <string>
#include <windows.h>

enum class FolderScopeChoice { Cancelled, DirectChildren, EntireSubtree };

FolderScopeChoice ShowFolderScopeDialog(HWND owner, const std::wstring& folderName);
