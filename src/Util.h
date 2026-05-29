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
bool MigrateLegacyStorage(std::wstring& error);

std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);

bool EnsureDirectory(const std::wstring& path);
bool IsValidHost(const std::wstring& host);
std::wstring Trim(const std::wstring& s);

struct SessionEndpoint {
    std::wstring host;
    int port = 3389;
};

// Splits "host:port" in the host field (mstsc full-address style) into separate values.
SessionEndpoint NormalizeSessionEndpoint(const std::wstring& hostField, int portField);

std::wstring FormatWin32Error(DWORD code);

}  // namespace Util
