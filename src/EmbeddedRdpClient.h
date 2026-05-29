#pragma once

#include "ConnectionTreeModel.h"

#include <freerdp/client/disp.h>

#include <functional>
#include <string>
#include <windows.h>

class ConnectionTreeModel;

class EmbeddedRdpClient {
public:
    using StatusCallback = std::function<void(const std::wstring& message, bool isError)>;

    EmbeddedRdpClient();
    ~EmbeddedRdpClient();

    EmbeddedRdpClient(const EmbeddedRdpClient&) = delete;
    EmbeddedRdpClient& operator=(const EmbeddedRdpClient&) = delete;

    bool CreateView(HWND parent, HINSTANCE instance, StatusCallback onStatus);
    void DestroyView();

    HWND ViewHwnd() const { return viewHwnd_; }

    bool Connect(const TreeNode& session, ConnectionTreeModel& model, std::wstring& error);
    void Disconnect();
    bool IsConnected() const { return connected_; }
    bool IsConnecting() const { return connecting_; }

    void Resize(int width, int height);
    void RequestFramePaint();

    void OnDispChannelConnected(DispClientContext* disp);
    void OnDispActivated();
    void SendDisplayLayout();
    void ScaleMouseCoords(int& x, int& y) const;

private:
    struct ClientContext;

    static DWORD WINAPI ClientThreadProc(LPVOID arg);
    void OnThreadConnectFinished(bool success, UINT32 errorCode);
    void OnThreadDisconnected();

    static LRESULT CALLBACK ViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void HandleConnectedMessage(bool success, UINT32 errorCode);
    void PaintView(HDC hdc, const RECT& paintRect);
    void PaintStatus(HDC hdc, const RECT& bounds);
    void SendMouse(UINT16 flags, int x, int y);
    void SendKeyboard(UINT16 flags, UINT8 code);

    HWND parentHwnd_ = nullptr;
    HWND viewHwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    StatusCallback onStatus_;
    ClientContext* context_ = nullptr;
    HANDLE thread_ = nullptr;
    bool connected_ = false;
    bool connecting_ = false;
    bool disconnectRequested_ = false;
    bool dispActivated_ = false;
    DispClientContext* disp_ = nullptr;
    int viewWidth_ = 800;
    int viewHeight_ = 600;
    std::wstring statusText_ = L"Connecting...";
};
