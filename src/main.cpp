#include "App.h"
#include "Logger.h"

#include <objbase.h>

namespace {

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

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetUnhandledExceptionFilter(AppExceptionFilter);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    if (!App::Instance().Init(instance)) {
        CoUninitialize();
        return 1;
    }

    const int code = App::Instance().Run();
    App::Instance().Shutdown();
    CoUninitialize();
    return code;
}
