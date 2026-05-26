#pragma once

#include <string>

struct CredentialSecrets {
    std::wstring username;
    std::wstring domain;
    std::wstring password;
};

class CredentialVault {
public:
    static std::wstring ProfileTarget(const std::wstring& profileId);
    static std::wstring TermsrvTarget(const std::wstring& fullAddress);

    bool SaveProfile(const std::wstring& profileId, const std::wstring& label, const std::wstring& username,
                    const std::wstring& domain, const std::wstring& password, std::wstring& error);
    bool LoadProfile(const std::wstring& profileId, CredentialSecrets& secrets, std::wstring& error);
    bool DeleteProfile(const std::wstring& profileId, std::wstring& error);
    bool HasProfile(const std::wstring& profileId);

    bool SyncToTermsrv(const std::wstring& fullAddress, const std::wstring& username, const std::wstring& domain,
                       const std::wstring& password, std::wstring& error);

    static std::wstring BuildUsernameForRdp(const std::wstring& username, const std::wstring& domain);
};
