#include "RdpLauncher.h"

#include "CredentialVault.h"
#include "Util.h"

#include <shellapi.h>

#include <fstream>
#include <sstream>

namespace {

std::wstring FullAddress(const TreeNode& session) {
    const Util::SessionEndpoint endpoint = Util::NormalizeSessionEndpoint(session.host, session.port);
    if (endpoint.port == 3389) {
        return endpoint.host;
    }
    return endpoint.host + L":" + std::to_wstring(endpoint.port);
}

std::wstring RdpFilePath(const std::wstring& sessionId) {
    return Util::GetTempRdpDir() + L"\\" + sessionId + L".rdp";
}

bool WriteRdpFile(const std::wstring& path, const TreeNode& session, const std::wstring& rdpUsername) {
    std::wostringstream out;
    const std::wstring addr = FullAddress(session);
    out << L"full address:s:" << addr << L"\r\n";
    out << L"server port:i:" << session.port << L"\r\n";
    out << L"screen mode id:i:2\r\n";
    out << L"prompt for credentials:i:0\r\n";
    if (!rdpUsername.empty()) {
        out << L"username:s:" << rdpUsername << L"\r\n";
    }

    std::wofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    const std::wstring content = out.str();
    const std::string utf8 = Util::WideToUtf8(content);
    std::ofstream binary(path, std::ios::binary);
    if (!binary) {
        return false;
    }
    binary.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return static_cast<bool>(binary);
}

}  // namespace

RdpLauncher::RdpLauncher(ConnectionTreeModel& model) : model_(model) {}

bool RdpLauncher::Connect(const TreeNode& session, std::wstring& error) {
    if (!session.IsSession()) {
        error = L"Not an RDP session.";
        return false;
    }
    const Util::SessionEndpoint endpoint = Util::NormalizeSessionEndpoint(session.host, session.port);
    if (!Util::IsValidHost(endpoint.host)) {
        error = L"Invalid hostname.";
        return false;
    }

    std::wstring rdpUsername;
    if (!session.credentialId.empty()) {
        const CredentialMeta* meta = model_.FindCredential(session.credentialId);
        if (!vault_.HasProfile(session.credentialId)) {
            // Metadata can remain when users remove the password from Credential Manager.
            // Allow launch and let MSTSC prompt for credentials.
            if (meta) {
                rdpUsername = CredentialVault::BuildUsernameForRdp(meta->username, meta->domain);
            }
        } else {
            CredentialSecrets secrets;
            if (!vault_.LoadProfile(session.credentialId, secrets, error)) {
                return false;
            }
            if (meta) {
                if (!meta->domain.empty()) {
                    secrets.domain = meta->domain;
                }
                if (!meta->username.empty()) {
                    secrets.username = meta->username;
                }
            }
            rdpUsername = CredentialVault::BuildUsernameForRdp(secrets.username, secrets.domain);
            const std::wstring addr = FullAddress(session);
            if (!vault_.SyncToTermsrv(addr, secrets.username, secrets.domain, secrets.password, error)) {
                return false;
            }
        }
    }

    const std::wstring rdpPath = RdpFilePath(session.id);
    if (!WriteRdpFile(rdpPath, session, rdpUsername)) {
        error = L"Failed to write RDP file.";
        return false;
    }

    std::wstring params = L"/f \"" + rdpPath + L"\"";
    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"mstsc.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        error = Util::FormatWin32Error(static_cast<DWORD>(reinterpret_cast<INT_PTR>(result)));
        return false;
    }
    return true;
}
