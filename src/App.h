#pragma once

#include "ConnectionTreeModel.h"
#include "JsonStore.h"

#include <windows.h>

class App {
public:
    static App& Instance();

    bool Init(HINSTANCE instance);
    int Run();
    void Shutdown();

    HINSTANCE GetHInstance() const { return instance_; }
    ConnectionTreeModel& Model() { return model_; }
    JsonStore& Store() { return store_; }

    bool Save(std::wstring& error);
    bool Load(std::wstring& error);

private:
    App() = default;

    HINSTANCE instance_ = nullptr;
    ConnectionTreeModel model_;
    JsonStore store_;
};
