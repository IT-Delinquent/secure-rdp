#include "CredentialVault.h"

#include "Util.h"

#include <wincred.h>

#include <vector>

namespace {

std::vector<BYTE> PasswordBlob(const std::wstring& password) {
    std::vector<BYTE> blob(password.size() * sizeof(wchar_t));
    if (!password.empty()) {
        memcpy(blob.data(), password.data(), blob.size());
    }
    return blob;
}

}  // namespace

std::wstring CredentialVault::ProfileTarget(const std::wstring& profileId) {
    return L"SecureRdp/Profile/" + profileId;
}

std::wstring CredentialVault::TermsrvTarget(const std::wstring& fullAddress) {
    return L"TERMSRV/" + fullAddress;
}

std::wstring CredentialVault::BuildUsernameForRdp(const std::wstring& username, const std::wstring& domain) {
    if (domain.empty()) {
        return username;
    }
    if (username.find(L'\\') != std::wstring::npos || username.find(L'@') != std::wstring::npos) {
        return username;
    }
    return domain + L"\\" + username;
}

bool CredentialVault::SaveProfile(const std::wstring& profileId, const std::wstring& label,
                                  const std::wstring& username, const std::wstring& domain,
                                  const std::wstring& password, std::wstring& error) {
    const std::wstring target = ProfileTarget(profileId);
    const std::wstring userField = BuildUsernameForRdp(username, domain);
    auto blob = PasswordBlob(password);

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.UserName = const_cast<LPWSTR>(userField.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
    cred.CredentialBlob = blob.empty() ? nullptr : blob.data();
    cred.Comment = const_cast<LPWSTR>(label.c_str());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        error = Util::FormatWin32Error(GetLastError());
        return false;
    }
    return true;
}

bool CredentialVault::LoadProfile(const std::wstring& profileId, CredentialSecrets& secrets, std::wstring& error) {
    const std::wstring target = ProfileTarget(profileId);
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        error = Util::FormatWin32Error(GetLastError());
        return false;
    }

    secrets.username = cred->UserName ? cred->UserName : L"";
    if (cred->CredentialBlobSize > 0 && cred->CredentialBlob) {
        secrets.password.assign(reinterpret_cast<const wchar_t*>(cred->CredentialBlob),
                                cred->CredentialBlobSize / sizeof(wchar_t));
    } else {
        secrets.password.clear();
    }
  secrets.domain.clear();
    if (!secrets.username.empty()) {
        const auto pos = secrets.username.find(L'\\');
        if (pos != std::wstring::npos) {
            secrets.domain = secrets.username.substr(0, pos);
            secrets.username = secrets.username.substr(pos + 1);
        }
    }

    CredFree(cred);
    return true;
}

bool CredentialVault::DeleteProfile(const std::wstring& profileId, std::wstring& error) {
    const std::wstring target = ProfileTarget(profileId);
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        const DWORD err = GetLastError();
        if (err == ERROR_NOT_FOUND) {
            return true;
        }
        error = Util::FormatWin32Error(err);
        return false;
    }
    return true;
}

bool CredentialVault::HasProfile(const std::wstring& profileId) {
    const std::wstring target = ProfileTarget(profileId);
    PCREDENTIALW cred = nullptr;
    const bool ok = CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred) != FALSE;
    if (cred) {
        CredFree(cred);
    }
    return ok;
}

bool CredentialVault::SyncToTermsrv(const std::wstring& fullAddress, const std::wstring& username,
                                    const std::wstring& domain, const std::wstring& password, std::wstring& error) {
    const std::wstring target = TermsrvTarget(fullAddress);
    const std::wstring userField = BuildUsernameForRdp(username, domain);
    auto blob = PasswordBlob(password);

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.UserName = const_cast<LPWSTR>(userField.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
    cred.CredentialBlob = blob.empty() ? nullptr : blob.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        error = Util::FormatWin32Error(GetLastError());
        return false;
    }
    return true;
}
