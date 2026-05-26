#include "Clipboard.h"

#include "Util.h"

#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;
UINT g_format = 0;

json W(const std::wstring& s) { return Util::WideToUtf8(s); }
std::wstring JW(const json& j, const char* key) {
    if (!j.contains(key) || !j[key].is_string()) {
        return L"";
    }
    return Util::Utf8ToWide(j[key].get<std::string>());
}

void Serialize(const TreeNode& node, json& out) {
    out["id"] = W(node.id);
    out["name"] = W(node.name);
    if (node.IsFolder()) {
        out["type"] = "folder";
        json children = json::array();
        for (const auto& c : node.children) {
            json child;
            Serialize(*c, child);
            children.push_back(std::move(child));
        }
        out["children"] = std::move(children);
    } else {
        out["type"] = "session";
        out["host"] = W(node.host);
        out["port"] = node.port;
        if (!node.credentialId.empty()) {
            out["credentialId"] = W(node.credentialId);
        }
    }
}

std::unique_ptr<TreeNode> Deserialize(const json& j) {
    auto node = std::make_unique<TreeNode>();
    node->name = JW(j, "name");
    const std::string type = j.value("type", "folder");
    if (type == "session") {
        node->type = NodeType::Session;
        node->host = JW(j, "host");
        node->port = j.value("port", 3389);
        node->credentialId = JW(j, "credentialId");
    } else {
        node->type = NodeType::Folder;
        if (j.contains("children") && j["children"].is_array()) {
            for (const auto& c : j["children"]) {
                node->children.push_back(Deserialize(c));
            }
        }
    }
    return node;
}

}  // namespace

UINT ClipboardManager::AcquireNodeClipboardFormat() {
    if (g_format == 0) {
        g_format = RegisterClipboardFormatW(kFormatName);
    }
    return g_format;
}

bool ClipboardManager::CopySubtree(const TreeNode& node) {
    const UINT format = AcquireNodeClipboardFormat();
    if (!format) {
        return false;
    }
    json root;
    Serialize(node, root);
    const std::string payload = root.dump();

    if (!OpenClipboard(nullptr)) {
        return false;
    }
    EmptyClipboard();

    const size_t bytes = payload.size() + 1;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) {
        CloseClipboard();
        return false;
    }
    void* ptr = GlobalLock(mem);
    memcpy(ptr, payload.c_str(), bytes);
    GlobalUnlock(mem);
    SetClipboardData(format, mem);
    CloseClipboard();
    return true;
}

std::unique_ptr<TreeNode> ClipboardManager::PasteSubtree() {
    const UINT format = AcquireNodeClipboardFormat();
    if (!format || !OpenClipboard(nullptr)) {
        return nullptr;
    }
    HANDLE data = GetClipboardData(format);
    if (!data) {
        CloseClipboard();
        return nullptr;
    }
    const char* text = static_cast<const char*>(GlobalLock(data));
    if (!text) {
        CloseClipboard();
        return nullptr;
    }
    std::unique_ptr<TreeNode> result;
    try {
        json j = json::parse(text);
        result = Deserialize(j);
    } catch (...) {
        result = nullptr;
    }
    GlobalUnlock(data);
    CloseClipboard();
    return result;
}

bool ClipboardManager::HasPasteData() {
    const UINT format = AcquireNodeClipboardFormat();
    if (!format || !OpenClipboard(nullptr)) {
        return false;
    }
    const bool has = IsClipboardFormatAvailable(format) != FALSE;
    CloseClipboard();
    return has;
}
