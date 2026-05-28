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

std::wstring LegacyProfileTarget(const std::wstring& profileId) {
    return L"SecureRdp/Profile/" + profileId;
}

bool ReadGenericCredential(const std::wstring& target, PCREDENTIALW* cred) {
    return CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, cred) != FALSE;
}

bool WriteGenericCredential(const std::wstring& target, const CREDENTIALW& source, std::wstring& error) {
    CREDENTIALW copy{};
    copy.Type = CRED_TYPE_GENERIC;
    copy.TargetName = const_cast<LPWSTR>(target.c_str());
    copy.UserName = source.UserName;
    copy.CredentialBlobSize = source.CredentialBlobSize;
    copy.CredentialBlob = source.CredentialBlob;
    copy.Comment = source.Comment;
    copy.Persist = source.Persist;
    if (!CredWriteW(&copy, 0)) {
        error = Util::FormatWin32Error(GetLastError());
        return false;
    }
    return true;
}

}  // namespace

std::wstring CredentialVault::ProfileTarget(const std::wstring& profileId) {
    return L"TinyRdp/Profile/" + profileId;
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
    if (!ReadGenericCredential(target, &cred)) {
        const DWORD err = GetLastError();
        if (err != ERROR_NOT_FOUND) {
            error = Util::FormatWin32Error(err);
            return false;
        }

        const std::wstring legacyTarget = LegacyProfileTarget(profileId);
        if (!ReadGenericCredential(legacyTarget, &cred)) {
            error = Util::FormatWin32Error(GetLastError());
            return false;
        }

        std::wstring migrateError;
        if (!WriteGenericCredential(target, *cred, migrateError)) {
            CredFree(cred);
            error = L"Failed to migrate legacy credential: " + migrateError;
            return false;
        }
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
    const std::vector<std::wstring> targets = {ProfileTarget(profileId), LegacyProfileTarget(profileId)};
    for (const auto& target : targets) {
        if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
            const DWORD err = GetLastError();
            if (err == ERROR_NOT_FOUND) {
                continue;
            }
            error = Util::FormatWin32Error(err);
            return false;
        }
    }
    return true;
}

bool CredentialVault::HasProfile(const std::wstring& profileId) {
    PCREDENTIALW cred = nullptr;
    if (ReadGenericCredential(ProfileTarget(profileId), &cred)) {
        CredFree(cred);
        return true;
    }
    const bool ok = ReadGenericCredential(LegacyProfileTarget(profileId), &cred);
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
