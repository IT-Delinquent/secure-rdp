#pragma once

#include "ConnectionTreeModel.h"

#include <string>

class JsonStore {
public:
    bool Load(ConnectionTreeModel& model, std::wstring& error);
    bool Save(const ConnectionTreeModel& model, std::wstring& error);

private:
    static bool ReadFileUtf8(const std::wstring& path, std::string& content);
    static bool WriteFileUtf8(const std::wstring& path, const std::string& content);
};
