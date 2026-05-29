#include "Util.h"

#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <cwctype>
#include <sstream>
#include <vector>

#pragma comment(lib, "shlwapi.lib")

namespace Util {

namespace {

std::wstring BuildAppDataSubdir(const wchar_t* leaf) {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return L"";
    }
    return std::wstring(path) + L"\\" + leaf;
}

std::wstring BuildTempSubdir(const wchar_t* leaf) {
    wchar_t temp[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, temp) == 0) {
        return L"";
    }
    return std::wstring(temp) + leaf;
}

bool PathExists(const std::wstring& path) {
    return !path.empty() && PathFileExistsW(path.c_str()) != FALSE;
}

}  // namespace

std::wstring NewUuid() {
    UUID uuid{};
    if (UuidCreate(&uuid) != RPC_S_OK) {
        return L"";
    }
    wchar_t buffer[64]{};
    if (StringFromGUID2(uuid, buffer, static_cast<int>(std::size(buffer))) == 0) {
        return L"";
    }
    std::wstring result(buffer);
    if (!result.empty() && result.front() == L'{' && result.back() == L'}') {
        result = result.substr(1, result.size() - 2);
    }
    return result;
}

std::wstring GetAppDataDir() {
    std::wstring dir = BuildAppDataSubdir(L"TinyRdp");
    EnsureDirectory(dir);
    return dir;
}

std::wstring GetConnectionsPath() {
    return GetAppDataDir() + L"\\connections.json";
}

std::wstring GetCredentialsMetaPath() {
    return GetAppDataDir() + L"\\credentials.json";
}

std::wstring GetTempRdpDir() {
    std::wstring dir = BuildTempSubdir(L"TinyRdp");
    EnsureDirectory(dir);
    return dir;
}

bool MigrateLegacyStorage(std::wstring& error) {
    error.clear();

    const std::wstring oldAppDir = BuildAppDataSubdir(L"SecureRdp");
    const std::wstring newAppDir = BuildAppDataSubdir(L"TinyRdp");
    if (newAppDir.empty()) {
        error = L"Failed to resolve %AppData% path.";
        return false;
    }
    EnsureDirectory(newAppDir);

    if (PathExists(oldAppDir)) {
        const std::vector<std::wstring> files = {L"connections.json", L"credentials.json", L"settings.json", L"app.log"};
        for (const auto& file : files) {
            const std::wstring oldPath = oldAppDir + L"\\" + file;
            const std::wstring newPath = newAppDir + L"\\" + file;
            if (!PathExists(oldPath) || PathExists(newPath)) {
                continue;
            }
            if (!MoveFileExW(oldPath.c_str(), newPath.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
                error = L"Failed migrating " + file + L": " + FormatWin32Error(GetLastError());
                return false;
            }
        }
    }

    const std::wstring oldTempDir = BuildTempSubdir(L"SecureRdp");
    const std::wstring newTempDir = BuildTempSubdir(L"TinyRdp");
    const bool newTempExists = PathExists(newTempDir);
    if (PathExists(oldTempDir) && !newTempExists) {
        if (!MoveFileExW(oldTempDir.c_str(), newTempDir.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
            error = L"Failed migrating temp directory: " + FormatWin32Error(GetLastError());
            return false;
        }
    }
    return true;
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return L"";
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) {
        return L"";
    }
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), len);
    return wide;
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return {};
    }
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), utf8.data(), len, nullptr, nullptr);
    return utf8;
}

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    if (PathFileExistsW(path.c_str())) {
        return true;
    }
    return SHCreateDirectoryExW(nullptr, path.c_str(), nullptr) == ERROR_SUCCESS ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

SessionEndpoint NormalizeSessionEndpoint(const std::wstring& hostField, int portField) {
    SessionEndpoint endpoint;
    endpoint.host = Trim(hostField);
    endpoint.port = portField > 0 ? portField : 3389;

    const size_t colon = endpoint.host.rfind(L':');
    if (colon == std::wstring::npos || colon == 0) {
        return endpoint;
    }

    const std::wstring tail = endpoint.host.substr(colon + 1);
    if (tail.empty() || !std::all_of(tail.begin(), tail.end(), [](wchar_t ch) { return std::iswdigit(ch); })) {
        return endpoint;
    }

    endpoint.host = Trim(endpoint.host.substr(0, colon));
    endpoint.port = std::stoi(tail);
    return endpoint;
}

bool IsValidHost(const std::wstring& host) {
    if (host.empty() || host.size() > 253) {
        return false;
    }
    for (wchar_t ch : host) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t' || ch == L' ' || ch == L'"') {
            return false;
        }
    }
    return true;
}

std::wstring Trim(const std::wstring& s) {
    const auto start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) {
        return L"";
    }
    const auto end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::wstring FormatWin32Error(DWORD code) {
    wchar_t* msg = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageW(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   reinterpret_cast<LPWSTR>(&msg), 0, nullptr);
    std::wstring result = (len && msg) ? std::wstring(msg, len) : L"Unknown error";
    if (msg) {
        LocalFree(msg);
    }
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) {
        result.pop_back();
    }
    return result;
}

}  // namespace Util
