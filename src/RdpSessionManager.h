#pragma once

#include "ConnectionTreeModel.h"
#include "EmbeddedRdpClient.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

class RdpSessionManager {
public:
    bool Initialize(HWND parent, HINSTANCE instance);
    void Shutdown();

    void Layout(int x, int y, int width, int height);
    void ApplyTheme();

    bool OpenOrFocus(const TreeNode& session, ConnectionTreeModel& model, std::wstring& error);
    void CloseSession(const std::wstring& sessionId);
    void CloseAll();
    void CloseActiveTab();

    bool HasSession(const std::wstring& sessionId) const;
    int SessionCount() const { return static_cast<int>(sessions_.size()); }

    HWND TabHwnd() const { return tabCtrl_; }
    bool HandleNotify(LPNMHDR hdr);
    bool HandleCommand(int id);

private:
    struct SessionEntry {
        std::wstring sessionId;
        std::wstring tabTitle;
        std::unique_ptr<EmbeddedRdpClient> client;
        HWND viewHwnd = nullptr;
    };

    int FindTabIndex(const std::wstring& sessionId) const;
    void ShowActiveSession();
    void ResizeActiveClient(int width = -1, int height = -1);
    void OnTabSelectionChanged();
    void OnStatusMessage(const std::wstring& sessionId, const std::wstring& message, bool isError);

    HWND parent_ = nullptr;
    HWND tabCtrl_ = nullptr;
    HWND contentHost_ = nullptr;
    HINSTANCE instance_ = nullptr;
    ConnectionTreeModel* model_ = nullptr;
    std::vector<SessionEntry> sessions_;
    std::unordered_map<std::wstring, size_t> idToIndex_;
    int contentTop_ = 0;
    int hostWidth_ = 0;
    int hostHeight_ = 0;
};
