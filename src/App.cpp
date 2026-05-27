#include "App.h"

#include "Logger.h"
#include "MainWindow.h"
#include "Util.h"

#include <commctrl.h>

App& App::Instance() {
    static App app;
    return app;
}

bool App::Init(HINSTANCE instance) {
    instance_ = instance;

    std::wstring migrationError;
    const bool migratedLegacy = Util::MigrateLegacyStorage(migrationError);
    Logger::Init();
    if (!migratedLegacy) {
        Logger::Warn(L"Legacy storage migration failed: " + migrationError);
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    std::wstring error;
    if (!Load(error)) {
        Logger::Warn(L"Load connections: " + error);
    } else {
        Logger::Info(L"Loaded connections from disk");
    }

    if (!MainWindow::Create(instance)) {
        Logger::Error(L"MainWindow::Create failed");
        return false;
    }
    return true;
}

int App::Run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void App::Shutdown() {
    std::wstring error;
    if (!Save(error)) {
        Logger::Error(L"Shutdown save failed: " + error);
    }
    Logger::Shutdown();
}

bool App::Save(std::wstring& error) {
    return store_.Save(model_, error);
}

bool App::Load(std::wstring& error) {
    return store_.Load(model_, error);
}
