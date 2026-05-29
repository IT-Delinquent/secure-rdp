#include "EmbeddedRdpClient.h"

#include "CredentialVault.h"
#include "Logger.h"
#include "Util.h"

#include <freerdp/channels/channels.h>
#include <freerdp/channels/disp.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/disp.h>
#include <freerdp/event.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/input.h>
#include <winpr/collections.h>
#include <winpr/wtypes.h>

#include <cmath>
#include <cstring>

#include <windowsx.h>

namespace {

constexpr wchar_t kViewClassName[] = L"TinyRdpSessionView";
constexpr UINT WM_TINYRDP_FRAME = WM_APP + 1;
constexpr UINT WM_TINYRDP_CONNECTED = WM_APP + 2;
constexpr UINT WM_TINYRDP_DISCONNECTED = WM_APP + 3;

struct TinyRdpClientContext {
    rdpContext context;
    EmbeddedRdpClient* owner = nullptr;
    HWND viewHwnd = nullptr;
};

TinyRdpClientContext* ContextFromRdp(rdpContext* ctx) {
    return reinterpret_cast<TinyRdpClientContext*>(ctx);
}

std::wstring FullAddress(const TreeNode& session) {
    const Util::SessionEndpoint endpoint = Util::NormalizeSessionEndpoint(session.host, session.port);
    if (endpoint.port == 3389) {
        return endpoint.host;
    }
    return endpoint.host + L":" + std::to_wstring(endpoint.port);
}

static DWORD TfVerifyCertificateEx(freerdp* instance, const char* host, UINT16 port, const char* common_name,
                                   const char* subject, const char* issuer, const char* fingerprint, DWORD flags) {
    WINPR_UNUSED(instance);
    WINPR_UNUSED(host);
    WINPR_UNUSED(port);
    WINPR_UNUSED(common_name);
    WINPR_UNUSED(subject);
    WINPR_UNUSED(issuer);
    WINPR_UNUSED(fingerprint);
    WINPR_UNUSED(flags);
    return 2;
}

static UINT TfDispDisplayControlCaps(DispClientContext* disp, UINT32 maxNumMonitors,
                                     UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB) {
    WINPR_UNUSED(maxNumMonitors);
    WINPR_UNUSED(maxMonitorAreaFactorA);
    WINPR_UNUSED(maxMonitorAreaFactorB);
    auto* tf = disp ? static_cast<TinyRdpClientContext*>(disp->custom) : nullptr;
    if (tf && tf->owner) {
        tf->owner->OnDispActivated();
    }
    return CHANNEL_RC_OK;
}

static void TfOnChannelConnected(void* context, const ChannelConnectedEventArgs* e) {
    if (!context || !e || !e->name || !e->pInterface) {
        return;
    }
    if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) != 0) {
        return;
    }
    auto* tf = ContextFromRdp(static_cast<rdpContext*>(context));
    if (tf && tf->owner) {
        tf->owner->OnDispChannelConnected(static_cast<DispClientContext*>(e->pInterface));
    }
}

static void TfOnChannelDisconnected(void* context, const ChannelDisconnectedEventArgs* e) {
    if (!context || !e || !e->name) {
        return;
    }
    if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) != 0) {
        return;
    }
    auto* tf = ContextFromRdp(static_cast<rdpContext*>(context));
    if (tf && tf->owner) {
        tf->owner->OnDispChannelConnected(nullptr);
    }
}

static DWORD TfVerifyChangedCertificateEx(freerdp* instance, const char* host, UINT16 port, const char* common_name,
                                          const char* subject, const char* issuer, const char* new_fingerprint,
                                          const char* old_subject, const char* old_issuer, const char* old_fingerprint,
                                          DWORD flags) {
    WINPR_UNUSED(instance);
    WINPR_UNUSED(host);
    WINPR_UNUSED(port);
    WINPR_UNUSED(common_name);
    WINPR_UNUSED(subject);
    WINPR_UNUSED(issuer);
    WINPR_UNUSED(new_fingerprint);
    WINPR_UNUSED(old_subject);
    WINPR_UNUSED(old_issuer);
    WINPR_UNUSED(old_fingerprint);
    WINPR_UNUSED(flags);
    return 2;
}

}  // namespace

struct EmbeddedRdpClient::ClientContext {
    RDP_CLIENT_ENTRY_POINTS entryPoints{};
    rdpContext* context = nullptr;
    freerdp* instance = nullptr;
};

EmbeddedRdpClient::EmbeddedRdpClient() = default;

EmbeddedRdpClient::~EmbeddedRdpClient() {
    Disconnect();
    DestroyView();
}

bool EmbeddedRdpClient::CreateView(HWND parent, HINSTANCE instance, StatusCallback onStatus) {
    parentHwnd_ = parent;
    instance_ = instance;
    onStatus_ = std::move(onStatus);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = ViewWndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kViewClassName;
        if (!RegisterClassExW(&wc)) {
            return false;
        }
        registered = true;
    }

    viewHwnd_ = CreateWindowExW(0, kViewClassName, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 100, 100, parent,
                                nullptr, instance, this);
    return viewHwnd_ != nullptr;
}

void EmbeddedRdpClient::DestroyView() {
    if (viewHwnd_) {
        DestroyWindow(viewHwnd_);
        viewHwnd_ = nullptr;
    }
}

static BOOL TfBeginPaint(rdpContext* context) {
    rdpGdi* gdi = context ? context->gdi : nullptr;
    if (!gdi || !gdi->primary || !gdi->primary->hdc || !gdi->primary->hdc->hwnd) {
        return TRUE;
    }
    gdi->primary->hdc->hwnd->invalid->null = TRUE;
    return TRUE;
}

static BOOL TfEndPaint(rdpContext* context) {
    TinyRdpClientContext* tf = ContextFromRdp(context);
    if (!tf || !tf->owner) {
        return TRUE;
    }
    tf->owner->RequestFramePaint();
    return TRUE;
}

static BOOL TfDesktopResize(rdpContext* context) {
    if (!context || !context->gdi || !context->settings) {
        return FALSE;
    }
    const UINT32 w = freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopWidth);
    const UINT32 h = freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopHeight);
    const BOOL resized = gdi_resize(context->gdi, w, h);
    TinyRdpClientContext* tf = ContextFromRdp(context);
    if (resized && tf && tf->owner) {
        tf->owner->RequestFramePaint();
    }
    return resized;
}

static BOOL TfPreConnect(freerdp* instance) {
    if (!instance || !instance->context) {
        return FALSE;
    }
    rdpSettings* settings = instance->context->settings;
    if (!settings) {
        return FALSE;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_CertificateCallbackPreferPEM, TRUE)) {
        return FALSE;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard, TRUE)) {
        return FALSE;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE)) {
        return FALSE;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE)) {
        return FALSE;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE)) {
        return FALSE;
    }
    return freerdp_client_load_addins(instance->context->channels, settings);
}

static BOOL TfPostConnect(freerdp* instance) {
    if (!instance || !instance->context) {
        return FALSE;
    }
    if (!gdi_init(instance, PIXEL_FORMAT_BGRX32)) {
        return FALSE;
    }
    rdpContext* context = instance->context;
    context->update->BeginPaint = TfBeginPaint;
    context->update->EndPaint = TfEndPaint;
    context->update->DesktopResize = TfDesktopResize;

    TinyRdpClientContext* tf = ContextFromRdp(context);
    if (tf && tf->owner) {
        tf->owner->RequestFramePaint();
    }
    return TRUE;
}

static void TfPostDisconnect(freerdp* instance) {
    if (!instance) {
        return;
    }
    gdi_free(instance);
}

static BOOL ClientGlobalInit() {
    return TRUE;
}

static void ClientGlobalUninit() {}

static BOOL ClientNew(freerdp* instance, rdpContext* context) {
    if (!instance || !context) {
        return FALSE;
    }
    instance->PreConnect = TfPreConnect;
    instance->PostConnect = TfPostConnect;
    instance->PostDisconnect = TfPostDisconnect;
    instance->VerifyCertificateEx = TfVerifyCertificateEx;
    instance->VerifyChangedCertificateEx = TfVerifyChangedCertificateEx;
    return TRUE;
}

static void ClientFree(freerdp* instance, rdpContext* context) {
    WINPR_UNUSED(instance);
    WINPR_UNUSED(context);
}

static int ClientStart(rdpContext* context) {
    WINPR_UNUSED(context);
    return 0;
}

static int ClientStop(rdpContext* context) {
    WINPR_UNUSED(context);
    return 0;
}

static int TinyRdpClientEntry(RDP_CLIENT_ENTRY_POINTS* entryPoints) {
    if (!entryPoints) {
        return -1;
    }
    ZeroMemory(entryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
    entryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
    entryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
    entryPoints->GlobalInit = ClientGlobalInit;
    entryPoints->GlobalUninit = ClientGlobalUninit;
    entryPoints->ContextSize = sizeof(TinyRdpClientContext);
    entryPoints->ClientNew = ClientNew;
    entryPoints->ClientFree = ClientFree;
    entryPoints->ClientStart = ClientStart;
    entryPoints->ClientStop = ClientStop;
    return 0;
}

bool EmbeddedRdpClient::Connect(const TreeNode& session, ConnectionTreeModel& model, std::wstring& error) {
    if (!viewHwnd_) {
        error = L"Session view not created.";
        return false;
    }
    if (connecting_ || connected_) {
        Disconnect();
    }

    auto client = std::make_unique<ClientContext>();

    TinyRdpClientEntry(&client->entryPoints);
    client->context = freerdp_client_context_new(&client->entryPoints);
    if (!client->context) {
        error = L"Failed to create RDP context.";
        return false;
    }

    TinyRdpClientContext* tf = ContextFromRdp(client->context);
    tf->owner = this;
    tf->viewHwnd = viewHwnd_;

    client->instance = freerdp_client_get_instance(client->context);
    if (!client->instance) {
        error = L"Failed to get RDP instance.";
        freerdp_client_context_free(client->context);
        return false;
    }

    rdpSettings* settings = client->context->settings;
    const Util::SessionEndpoint endpoint = Util::NormalizeSessionEndpoint(session.host, session.port);
    const std::wstring addr = FullAddress(session);

    if (!Util::IsValidHost(endpoint.host)) {
        error = L"Invalid hostname.";
        freerdp_client_context_free(client->context);
        return false;
    }

    const std::string hostUtf8 = Util::WideToUtf8(endpoint.host);
    if (hostUtf8.empty()) {
        error = L"Invalid hostname encoding.";
        freerdp_client_context_free(client->context);
        return false;
    }

    LOG_DEBUG(L"RDP connect host=" + endpoint.host + L" port=" + std::to_wstring(endpoint.port));

    if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, hostUtf8.c_str())) {
        error = L"Failed to set hostname.";
        freerdp_client_context_free(client->context);
        return false;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, static_cast<UINT32>(endpoint.port))) {
        error = L"Failed to set port.";
        freerdp_client_context_free(client->context);
        return false;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, static_cast<UINT32>(viewWidth_ > 0 ? viewWidth_ : 800))) {
        error = L"Failed to set desktop width.";
        freerdp_client_context_free(client->context);
        return false;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, static_cast<UINT32>(viewHeight_ > 0 ? viewHeight_ : 600))) {
        error = L"Failed to set desktop height.";
        freerdp_client_context_free(client->context);
        return false;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, TRUE)) {
        error = L"Failed to enable display control.";
        freerdp_client_context_free(client->context);
        return false;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_DynamicResolutionUpdate, TRUE)) {
        error = L"Failed to enable dynamic resolution.";
        freerdp_client_context_free(client->context);
        return false;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
        error = L"Failed to set color depth.";
        freerdp_client_context_free(client->context);
        return false;
    }

    if (!session.credentialId.empty()) {
        CredentialVault vault;
        const CredentialMeta* meta = model.FindCredential(session.credentialId);
        if (vault.HasProfile(session.credentialId)) {
            CredentialSecrets secrets;
            if (!vault.LoadProfile(session.credentialId, secrets, error)) {
                freerdp_client_context_free(client->context);
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
            const std::wstring user = CredentialVault::BuildUsernameForRdp(secrets.username, secrets.domain);
            const std::string userUtf8 = Util::WideToUtf8(user);
            const std::string passUtf8 = Util::WideToUtf8(secrets.password);
            if (!freerdp_settings_set_string(settings, FreeRDP_Username, userUtf8.c_str())) {
                error = L"Failed to set username.";
                freerdp_client_context_free(client->context);
                return false;
            }
            if (!freerdp_settings_set_string(settings, FreeRDP_Password, passUtf8.c_str())) {
                error = L"Failed to set password.";
                freerdp_client_context_free(client->context);
                return false;
            }
        } else if (meta) {
            const std::wstring user = CredentialVault::BuildUsernameForRdp(meta->username, meta->domain);
            const std::string userUtf8 = Util::WideToUtf8(user);
            if (!freerdp_settings_set_string(settings, FreeRDP_Username, userUtf8.c_str())) {
                error = L"Failed to set username.";
                freerdp_client_context_free(client->context);
                return false;
            }
        }
    }

    if (client->context->pubSub) {
        PubSub_SubscribeChannelConnected(client->context->pubSub, TfOnChannelConnected);
        PubSub_SubscribeChannelDisconnected(client->context->pubSub, TfOnChannelDisconnected);
    }

    context_ = client.release();
    connecting_ = true;
    connected_ = false;
    disconnectRequested_ = false;
    dispActivated_ = false;
    disp_ = nullptr;
    statusText_ = L"Connecting to " + addr + L"...";

    if (onStatus_) {
        onStatus_(statusText_, false);
    }

    if (freerdp_client_start(context_->context) != 0) {
        error = L"Failed to start RDP client.";
        delete context_;
        context_ = nullptr;
        connecting_ = false;
        return false;
    }

    InvalidateRect(viewHwnd_, nullptr, FALSE);

    thread_ = CreateThread(nullptr, 0, ClientThreadProc, context_->instance, 0, nullptr);
    if (!thread_) {
        error = Util::FormatWin32Error(GetLastError());
        freerdp_client_stop(context_->context);
        freerdp_client_context_free(context_->context);
        delete context_;
        context_ = nullptr;
        connecting_ = false;
        return false;
    }
    return true;
}

void EmbeddedRdpClient::RequestFramePaint() {
    if (viewHwnd_) {
        PostMessageW(viewHwnd_, WM_TINYRDP_FRAME, 0, 0);
    }
}

void EmbeddedRdpClient::HandleConnectedMessage(bool success, UINT32 errorCode) {
    connecting_ = false;
    connected_ = success;
    if (success) {
        statusText_.clear();
        if (onStatus_) {
            onStatus_(L"Connected.", false);
        }
        SendDisplayLayout();
    } else {
        const char* errName = freerdp_get_last_error_name(errorCode);
        const char* errText = freerdp_get_last_error_string(errorCode);
        statusText_ = L"Connection failed";
        if (errName) {
            statusText_ += L": ";
            statusText_ += Util::Utf8ToWide(errName);
        }
        if (errText) {
            statusText_ += L" (";
            statusText_ += Util::Utf8ToWide(errText);
            statusText_ += L")";
        }
        if (onStatus_) {
            onStatus_(statusText_, true);
        }
        LOG_ERROR(statusText_);
    }
    InvalidateRect(viewHwnd_, nullptr, FALSE);
}

void EmbeddedRdpClient::OnThreadConnectFinished(bool success, UINT32 errorCode) {
    if (viewHwnd_) {
        PostMessageW(viewHwnd_, WM_TINYRDP_CONNECTED, success ? 1 : 0, static_cast<LPARAM>(errorCode));
    }
}

void EmbeddedRdpClient::OnThreadDisconnected() {
    if (viewHwnd_) {
        PostMessageW(viewHwnd_, WM_TINYRDP_DISCONNECTED, 0, 0);
    }
}

DWORD WINAPI EmbeddedRdpClient::ClientThreadProc(LPVOID arg) {
    auto* instance = static_cast<freerdp*>(arg);
    if (!instance || !instance->context) {
        return 1;
    }

    TinyRdpClientContext* tf = ContextFromRdp(instance->context);
    EmbeddedRdpClient* owner = tf ? tf->owner : nullptr;

    const BOOL ok = freerdp_connect(instance);
    const UINT32 connectError = freerdp_get_last_error(instance->context);
    if (owner) {
        owner->OnThreadConnectFinished(ok == TRUE, connectError);
    }
    if (!ok) {
        return connectError;
    }

    while (!freerdp_shall_disconnect_context(instance->context)) {
        if (owner && owner->disconnectRequested_) {
            freerdp_abort_connect_context(instance->context);
            break;
        }
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        const DWORD count = freerdp_get_event_handles(instance->context, handles, ARRAYSIZE(handles));
        if (count == 0) {
            break;
        }
        const DWORD status = WaitForMultipleObjects(count, handles, FALSE, 100);
        if (status == WAIT_FAILED) {
            break;
        }
        if (!freerdp_check_event_handles(instance->context)) {
            if (freerdp_get_last_error(instance->context) != FREERDP_ERROR_SUCCESS) {
                break;
            }
        }
    }

    freerdp_disconnect(instance);
    if (owner) {
        owner->OnThreadDisconnected();
    }
    return 0;
}

void EmbeddedRdpClient::Disconnect() {
    disconnectRequested_ = true;
    if (context_ && context_->context && context_->context->pubSub) {
        PubSub_UnsubscribeChannelConnected(context_->context->pubSub, TfOnChannelConnected);
        PubSub_UnsubscribeChannelDisconnected(context_->context->pubSub, TfOnChannelDisconnected);
    }
    disp_ = nullptr;
    dispActivated_ = false;
    if (context_ && context_->context) {
        freerdp_client_stop(context_->context);
        if (context_->instance) {
            freerdp_shall_disconnect_context(context_->context);
        }
    }
    if (thread_) {
        WaitForSingleObject(thread_, 15000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (context_) {
        if (context_->context) {
            freerdp_client_context_free(context_->context);
        }
        delete context_;
        context_ = nullptr;
    }
    connected_ = false;
    connecting_ = false;
    disconnectRequested_ = false;
    statusText_ = L"Disconnected.";
    if (viewHwnd_) {
        InvalidateRect(viewHwnd_, nullptr, FALSE);
    }
}

void EmbeddedRdpClient::OnDispChannelConnected(DispClientContext* disp) {
    disp_ = disp;
    dispActivated_ = false;
    if (!disp_) {
        return;
    }
    if (context_ && context_->context) {
        disp_->custom = ContextFromRdp(context_->context);
    }
    disp_->DisplayControlCaps = TfDispDisplayControlCaps;
}

void EmbeddedRdpClient::OnDispActivated() {
    dispActivated_ = true;
    SendDisplayLayout();
}

void EmbeddedRdpClient::SendDisplayLayout() {
    if (!connected_ || !dispActivated_ || !disp_ || !disp_->SendMonitorLayout) {
        return;
    }
    if (!context_ || !context_->instance || !context_->instance->context) {
        return;
    }
    rdpSettings* settings = context_->instance->context->settings;
    if (!settings) {
        return;
    }

    UINT32 width = static_cast<UINT32>(viewWidth_ > 0 ? viewWidth_ : 1);
    UINT32 height = static_cast<UINT32>(viewHeight_ > 0 ? viewHeight_ : 1);
    width = (width + 3) & ~3U;
    if (width < DISPLAY_CONTROL_MIN_MONITOR_WIDTH) {
        width = DISPLAY_CONTROL_MIN_MONITOR_WIDTH;
    }
    if (height < DISPLAY_CONTROL_MIN_MONITOR_HEIGHT) {
        height = DISPLAY_CONTROL_MIN_MONITOR_HEIGHT;
    }

    DISPLAY_CONTROL_MONITOR_LAYOUT layout{};
    layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
    layout.Width = width;
    layout.Height = height;
    layout.PhysicalWidth = static_cast<UINT32>(lround(width / 75.0 * 25.4));
    layout.PhysicalHeight = static_cast<UINT32>(lround(height / 75.0 * 25.4));

    freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, width);
    freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, height);

    if (disp_->SendMonitorLayout(disp_, 1, &layout) != CHANNEL_RC_OK) {
        return;
    }
    RequestFramePaint();
}

void EmbeddedRdpClient::ScaleMouseCoords(int& x, int& y) const {
    if (!connected_ || !context_ || !context_->instance || !context_->instance->context) {
        return;
    }
    rdpGdi* gdi = context_->instance->context->gdi;
    if (!gdi || gdi->width <= 0 || gdi->height <= 0 || viewWidth_ <= 0 || viewHeight_ <= 0) {
        return;
    }
    if (gdi->width == viewWidth_ && gdi->height == viewHeight_) {
        return;
    }
    x = static_cast<int>((static_cast<INT64>(x) * gdi->width) / viewWidth_);
    y = static_cast<int>((static_cast<INT64>(y) * gdi->height) / viewHeight_);
}

void EmbeddedRdpClient::Resize(int width, int height) {
    viewWidth_ = width > 0 ? width : 1;
    viewHeight_ = height > 0 ? height : 1;
    if (connected_) {
        SendDisplayLayout();
    }
    if (viewHwnd_) {
        SetWindowPos(viewHwnd_, nullptr, 0, 0, viewWidth_, viewHeight_, SWP_NOMOVE | SWP_NOZORDER);
        InvalidateRect(viewHwnd_, nullptr, FALSE);
    }
}

void EmbeddedRdpClient::PaintStatus(HDC hdc, const RECT& bounds) {
    if (statusText_.empty()) {
        return;
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(96, 96, 96));
    DrawTextW(hdc, statusText_.c_str(), static_cast<int>(statusText_.size()), const_cast<RECT*>(&bounds),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void EmbeddedRdpClient::PaintView(HDC hdc, const RECT& paintRect) {
    if (!connected_ || !context_ || !context_->instance || !context_->instance->context) {
        PaintStatus(hdc, paintRect);
        return;
    }
    rdpGdi* gdi = context_->instance->context->gdi;
    if (!gdi) {
        PaintStatus(hdc, paintRect);
        return;
    }

    const UINT32 bw = static_cast<UINT32>(gdi->width);
    const UINT32 bh = static_cast<UINT32>(gdi->height);
    BYTE* data = gdi->primary_buffer;
    if (bw == 0 || bh == 0 || !data) {
        PaintStatus(hdc, paintRect);
        return;
    }

    RECT dest = paintRect;
    if ((dest.right - dest.left) <= 0 || (dest.bottom - dest.top) <= 0) {
        GetClientRect(viewHwnd_, &dest);
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(bw);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(bh);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(hdc, HALFTONE);
    if (gdi->stride == bw * 4) {
        StretchDIBits(hdc, dest.left, dest.top, dest.right - dest.left, dest.bottom - dest.top, 0, 0,
                      static_cast<int>(bw), static_cast<int>(bh), data, &bmi, DIB_RGB_COLORS, SRCCOPY);
        return;
    }

    for (UINT32 y = 0; y < bh; ++y) {
        const int destY = dest.top + static_cast<int>((static_cast<INT64>(y) * (dest.bottom - dest.top)) / bh);
        const int destH = dest.top + static_cast<int>((static_cast<INT64>(y + 1) * (dest.bottom - dest.top)) / bh) - destY;
        if (destH <= 0) {
            continue;
        }
        StretchDIBits(hdc, dest.left, destY, dest.right - dest.left, destH, 0, static_cast<int>(y),
                      static_cast<int>(bw), 1, data + static_cast<size_t>(y) * gdi->stride, &bmi, DIB_RGB_COLORS,
                      SRCCOPY);
    }
}

void EmbeddedRdpClient::SendMouse(UINT16 flags, int x, int y) {
    if (!connected_ || !context_ || !context_->instance) {
        return;
    }
    rdpInput* input = context_->instance->context->input;
    if (!input) {
        return;
    }
    ScaleMouseCoords(x, y);
    freerdp_input_send_mouse_event(input, flags, static_cast<UINT16>(x), static_cast<UINT16>(y));
}

void EmbeddedRdpClient::SendKeyboard(UINT16 flags, UINT8 code) {
    if (!connected_ || !context_ || !context_->instance) {
        return;
    }
    rdpInput* input = context_->instance->context->input;
    if (!input) {
        return;
    }
    freerdp_input_send_keyboard_event(input, flags, code);
}

LRESULT CALLBACK EmbeddedRdpClient::ViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EmbeddedRdpClient* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<EmbeddedRdpClient*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->viewHwnd_ = hwnd;
        }
    } else {
        self = reinterpret_cast<EmbeddedRdpClient*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_ERASEBKGND: {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            FillRect(reinterpret_cast<HDC>(wParam), &rc,
                     reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            self->PaintView(hdc, ps.rcPaint);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TINYRDP_FRAME:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_TINYRDP_CONNECTED:
            self->HandleConnectedMessage(wParam != 0, static_cast<UINT32>(lParam));
            return 0;
        case WM_TINYRDP_DISCONNECTED:
            self->connecting_ = false;
            self->connected_ = false;
            self->statusText_ = L"Disconnected.";
            if (self->onStatus_) {
                self->onStatus_(self->statusText_, false);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SIZE:
            self->viewWidth_ = LOWORD(lParam) > 0 ? static_cast<int>(LOWORD(lParam)) : 1;
            self->viewHeight_ = HIWORD(lParam) > 0 ? static_cast<int>(HIWORD(lParam)) : 1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            SetFocus(hwnd);
            self->SendMouse(PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
            self->SendMouse(PTR_FLAGS_BUTTON1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            SetCapture(hwnd);
            self->SendMouse(PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_RBUTTONUP:
            self->SendMouse(PTR_FLAGS_BUTTON2, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE:
            self->SendMouse(PTR_FLAGS_MOVE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            const UINT16 wheel = static_cast<UINT16>(
                delta > 0 ? (PTR_FLAGS_WHEEL | 0x0078) : (PTR_FLAGS_WHEEL | 0x0088));
            self->SendMouse(wheel, pt.x, pt.y);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_KEYUP: {
            const UINT16 flags = msg == WM_KEYDOWN ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE;
            const UINT8 code = static_cast<UINT8>(MapVirtualKeyW(static_cast<UINT>(wParam), MAPVK_VK_TO_VSC) & 0xFF);
            self->SendKeyboard(flags, code);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
