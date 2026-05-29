#include "RdpSessionManager.h"

#include "Logger.h"
#include "Resource.h"
#include "UiTheme.h"

#include <algorithm>
#include <commctrl.h>

namespace {

constexpr int kTabStripHeight = 28;

}  // namespace

bool RdpSessionManager::Initialize(HWND parent, HINSTANCE instance) {
    parent_ = parent;
    instance_ = instance;

    tabCtrl_ = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FOCUSONBUTTONDOWN,
                               0, 0, 100, kTabStripHeight, parent, reinterpret_cast<HMENU>(IDC_SESSION_TABS), instance,
                               nullptr);
    if (!tabCtrl_) {
        return false;
    }

    contentHost_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, kTabStripHeight, 100,
                                   100, parent, reinterpret_cast<HMENU>(IDC_SESSION_HOST), instance, nullptr);
    if (!contentHost_) {
        return false;
    }

    ApplyTheme();
    return true;
}

void RdpSessionManager::Shutdown() {
    CloseAll();
    if (contentHost_) {
        DestroyWindow(contentHost_);
        contentHost_ = nullptr;
    }
    if (tabCtrl_) {
        DestroyWindow(tabCtrl_);
        tabCtrl_ = nullptr;
    }
}

void RdpSessionManager::Layout(int /*x*/, int /*y*/, int width, int height) {
    hostWidth_ = width > 0 ? width : 0;
    hostHeight_ = height > 0 ? height : 0;
    contentTop_ = kTabStripHeight;

    if (tabCtrl_) {
        SetWindowPos(tabCtrl_, nullptr, 0, 0, hostWidth_, kTabStripHeight, SWP_NOZORDER);
    }
    int contentW = 0;
    int contentH = 0;
    if (contentHost_) {
        contentH = hostHeight_ > contentTop_ ? hostHeight_ - contentTop_ : 0;
        contentW = hostWidth_;
        SetWindowPos(contentHost_, nullptr, 0, contentTop_, contentW, contentH, SWP_NOZORDER);
    }
    ResizeActiveClient(contentW, contentH);
}

void RdpSessionManager::ApplyTheme() {
    UiTheme::ApplyTabControl(tabCtrl_);
    UiTheme::ApplyPanelHost(contentHost_);
}

int RdpSessionManager::FindTabIndex(const std::wstring& sessionId) const {
    const auto it = idToIndex_.find(sessionId);
    if (it == idToIndex_.end()) {
        return -1;
    }
    return static_cast<int>(it->second);
}

bool RdpSessionManager::OpenOrFocus(const TreeNode& session, ConnectionTreeModel& model, std::wstring& error) {
    model_ = &model;
    const int existing = FindTabIndex(session.id);
    if (existing >= 0) {
        TabCtrl_SetCurSel(tabCtrl_, existing);
        ShowActiveSession();
        return true;
    }

    SessionEntry entry;
    entry.sessionId = session.id;
    entry.tabTitle = session.name;
    entry.client = std::make_unique<EmbeddedRdpClient>();

    const auto statusCb = [this, id = session.id](const std::wstring& message, bool isError) {
        OnStatusMessage(id, message, isError);
    };

    if (!entry.client->CreateView(contentHost_, instance_, statusCb)) {
        error = L"Failed to create session view.";
        return false;
    }
    entry.viewHwnd = entry.client->ViewHwnd();

    sessions_.push_back(std::move(entry));
    idToIndex_[session.id] = sessions_.size() - 1;
    const int index = static_cast<int>(sessions_.size() - 1);

    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = sessions_[static_cast<size_t>(index)].tabTitle.data();
    if (TabCtrl_InsertItem(tabCtrl_, index, &item) < 0) {
        error = L"Failed to create session tab.";
        sessions_.pop_back();
        idToIndex_.erase(session.id);
        return false;
    }

    TabCtrl_SetCurSel(tabCtrl_, index);
    ShowActiveSession();

    SessionEntry& active = sessions_[static_cast<size_t>(index)];
    if (!active.client->Connect(session, model, error)) {
        TabCtrl_DeleteItem(tabCtrl_, index);
        sessions_.pop_back();
        idToIndex_.erase(session.id);
        return false;
    }
    return true;
}

void RdpSessionManager::CloseSession(const std::wstring& sessionId) {
    const int index = FindTabIndex(sessionId);
    if (index < 0) {
        return;
    }

    sessions_[static_cast<size_t>(index)].client->Disconnect();
    TabCtrl_DeleteItem(tabCtrl_, index);

    sessions_.erase(sessions_.begin() + index);
    idToIndex_.clear();
    for (size_t i = 0; i < sessions_.size(); ++i) {
        idToIndex_[sessions_[i].sessionId] = i;
    }

    if (!sessions_.empty()) {
        const int next = std::min(index, TabCtrl_GetItemCount(tabCtrl_) - 1);
        TabCtrl_SetCurSel(tabCtrl_, next >= 0 ? next : 0);
    }
    ShowActiveSession();
}

void RdpSessionManager::CloseAll() {
    while (!sessions_.empty()) {
        CloseSession(sessions_.back().sessionId);
    }
}

void RdpSessionManager::CloseActiveTab() {
    const int sel = TabCtrl_GetCurSel(tabCtrl_);
    if (sel < 0 || sel >= static_cast<int>(sessions_.size())) {
        return;
    }
    CloseSession(sessions_[static_cast<size_t>(sel)].sessionId);
}

bool RdpSessionManager::HasSession(const std::wstring& sessionId) const {
    return idToIndex_.find(sessionId) != idToIndex_.end();
}

void RdpSessionManager::ShowActiveSession() {
    const int sel = TabCtrl_GetCurSel(tabCtrl_);
    for (size_t i = 0; i < sessions_.size(); ++i) {
        HWND view = sessions_[i].viewHwnd;
        if (!view) {
            continue;
        }
        ShowWindow(view, static_cast<int>(i) == sel ? SW_SHOW : SW_HIDE);
    }
    ResizeActiveClient();
    if (sel >= 0 && sel < static_cast<int>(sessions_.size()) && sessions_[static_cast<size_t>(sel)].viewHwnd) {
        SetFocus(sessions_[static_cast<size_t>(sel)].viewHwnd);
    }
}

void RdpSessionManager::ResizeActiveClient(int width, int height) {
    if (!contentHost_) {
        return;
    }
    int w = width;
    int h = height;
    if (w < 0 || h < 0) {
        RECT rc{};
        GetClientRect(contentHost_, &rc);
        w = rc.right - rc.left;
        h = rc.bottom - rc.top;
    }
    const int sel = TabCtrl_GetCurSel(tabCtrl_);
    if (sel < 0 || sel >= static_cast<int>(sessions_.size())) {
        return;
    }
    SessionEntry& entry = sessions_[static_cast<size_t>(sel)];
    if (entry.client) {
        entry.client->Resize(w, h);
    }
}

void RdpSessionManager::OnTabSelectionChanged() {
    ShowActiveSession();
}

void RdpSessionManager::OnStatusMessage(const std::wstring& sessionId, const std::wstring& message, bool isError) {
    if (isError) {
        LOG_ERROR(L"Session " + sessionId + L": " + message);
    } else {
        LOG_DEBUG(L"Session " + sessionId + L": " + message);
    }
    (void)sessionId;
    (void)message;
}

bool RdpSessionManager::HandleNotify(LPNMHDR hdr) {
    if (!hdr || hdr->hwndFrom != tabCtrl_) {
        return false;
    }
    switch (hdr->code) {
        case TCN_SELCHANGE:
            OnTabSelectionChanged();
            return true;
    }
    return false;
}

bool RdpSessionManager::HandleCommand(int id) {
    (void)id;
    return false;
}
