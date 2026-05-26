#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace Util {

std::wstring NewUuid();
std::wstring GetAppDataDir();
std::wstring GetConnectionsPath();
std::wstring GetCredentialsMetaPath();
std::wstring GetTempRdpDir();

std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);

bool EnsureDirectory(const std::wstring& path);
bool IsValidHost(const std::wstring& host);
std::wstring Trim(const std::wstring& s);

std::wstring FormatWin32Error(DWORD code);

}  // namespace Util
