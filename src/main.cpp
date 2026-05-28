#include "App.h"
#include "Logger.h"

#include <objbase.h>

namespace {

constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\TinyRdp.SingleInstance";
constexpr wchar_t kMainWindowClassName[] = L"TinyRdpMainWindow";

LONG WINAPI AppExceptionFilter(EXCEPTION_POINTERS* info) {
    if (info && info->ExceptionRecord) {
        wchar_t buf[128]{};
        swprintf_s(buf, L"Unhandled exception 0x%08X", info->ExceptionRecord->ExceptionCode);
        Logger::Error(buf);
    } else {
        Logger::Error(L"Unhandled exception (unknown)");
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void ActivateExistingMainWindow() {
    HWND existing = FindWindowW(kMainWindowClassName, nullptr);
    if (!existing) {
        return;
    }

    if (IsIconic(existing)) {
        ShowWindow(existing, SW_RESTORE);
    } else {
        ShowWindow(existing, SW_SHOW);
    }

    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const DWORD currentThread = GetCurrentThreadId();
    if (foregroundThread != 0 && foregroundThread != currentThread) {
        AttachThreadInput(currentThread, foregroundThread, TRUE);
        SetForegroundWindow(existing);
        BringWindowToTop(existing);
        SetFocus(existing);
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    } else {
        SetForegroundWindow(existing);
        BringWindowToTop(existing);
        SetFocus(existing);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetUnhandledExceptionFilter(AppExceptionFilter);

    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    if (!singleInstanceMutex) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        ActivateExistingMainWindow();
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    if (!App::Instance().Init(instance)) {
        CoUninitialize();
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    const int code = App::Instance().Run();
    App::Instance().Shutdown();
    CoUninitialize();
    CloseHandle(singleInstanceMutex);
    return code;
}
