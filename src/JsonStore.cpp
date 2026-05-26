#include "JsonStore.h"

#include "Util.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace {

using json = nlohmann::json;

std::wstring JsonToW(const json& j, const char* key, const std::wstring& def = L"") {
    if (!j.contains(key) || j[key].is_null()) {
        return def;
    }
    if (j[key].is_string()) {
        return Util::Utf8ToWide(j[key].get<std::string>());
    }
    return def;
}

json WToJson(const std::wstring& w) {
    return Util::WideToUtf8(w);
}

void SerializeNode(const TreeNode& node, json& out) {
    out["id"] = WToJson(node.id);
    out["name"] = WToJson(node.name);
    if (node.IsFolder()) {
        out["type"] = "folder";
        json children = json::array();
        for (const auto& child : node.children) {
            json childJson;
            SerializeNode(*child, childJson);
            children.push_back(std::move(childJson));
        }
        out["children"] = std::move(children);
    } else {
        out["type"] = "session";
        out["host"] = WToJson(node.host);
        out["port"] = node.port;
        if (!node.credentialId.empty()) {
            out["credentialId"] = WToJson(node.credentialId);
        }
    }
}

std::unique_ptr<TreeNode> DeserializeNode(const json& j) {
    auto node = std::make_unique<TreeNode>();
    node->id = JsonToW(j, "id");
    if (node->id.empty()) {
        node->id = Util::NewUuid();
    }
    node->name = JsonToW(j, "name", L"Unnamed");
    const std::string type = j.value("type", "folder");
    if (type == "session") {
        node->type = NodeType::Session;
        node->host = JsonToW(j, "host");
        node->port = j.value("port", 3389);
        node->credentialId = JsonToW(j, "credentialId");
    } else {
        node->type = NodeType::Folder;
        if (j.contains("children") && j["children"].is_array()) {
            for (const auto& childJson : j["children"]) {
                if (auto child = DeserializeNode(childJson)) {
                    node->children.push_back(std::move(child));
                }
            }
        }
    }
    return node;
}

void SerializeCredentials(const std::vector<CredentialMeta>& creds, json& out) {
    out = json::array();
    for (const auto& c : creds) {
        json item;
        item["id"] = WToJson(c.id);
        item["label"] = WToJson(c.label);
        item["username"] = WToJson(c.username);
        if (!c.domain.empty()) {
            item["domain"] = WToJson(c.domain);
        }
        out.push_back(std::move(item));
    }
}

void DeserializeCredentials(const json& arr, std::vector<CredentialMeta>& creds) {
    creds.clear();
    if (!arr.is_array()) {
        return;
    }
    for (const auto& item : arr) {
        CredentialMeta c;
        c.id = JsonToW(item, "id");
        c.label = JsonToW(item, "label");
        c.username = JsonToW(item, "username");
        c.domain = JsonToW(item, "domain");
        if (!c.id.empty()) {
            creds.push_back(std::move(c));
        }
    }
}

}  // namespace

bool JsonStore::ReadFileUtf8(const std::wstring& path, std::string& content) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool JsonStore::WriteFileUtf8(const std::wstring& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

bool JsonStore::Load(ConnectionTreeModel& model, std::wstring& error) {
    model.ResetToDefault();

    const std::wstring connPath = Util::GetConnectionsPath();
    std::string connContent;
    if (ReadFileUtf8(connPath, connContent)) {
        try {
            json doc = json::parse(connContent);
            const int version = doc.value("version", 1);
            if (version != 1) {
                error = L"Unsupported connections file version.";
                return false;
            }
            if (doc.contains("root")) {
                if (auto loaded = DeserializeNode(doc["root"])) {
                    TreeNode& root = model.Root();
                    root.id = loaded->id;
                    root.name = loaded->name;
                    root.type = NodeType::Folder;
                    root.host.clear();
                    root.port = 3389;
                    root.credentialId.clear();
                    root.children = std::move(loaded->children);
                }
            }
        } catch (const std::exception& ex) {
            error = Util::Utf8ToWide(ex.what());
            return false;
        }
    }

    const std::wstring credPath = Util::GetCredentialsMetaPath();
    std::string credContent;
    if (ReadFileUtf8(credPath, credContent)) {
        try {
            json doc = json::parse(credContent);
            DeserializeCredentials(doc.value("profiles", json::array()), model.Credentials());
        } catch (const std::exception& ex) {
            error = Util::Utf8ToWide(ex.what());
            return false;
        }
    }

    return true;
}

bool JsonStore::Save(const ConnectionTreeModel& model, std::wstring& error) {
  try {
    json conn;
    conn["version"] = 1;
    SerializeNode(model.Root(), conn["root"]);

    if (!WriteFileUtf8(Util::GetConnectionsPath(), conn.dump(2))) {
        error = L"Failed to write connections.json";
        return false;
    }

    json credDoc;
    credDoc["version"] = 1;
    SerializeCredentials(model.Credentials(), credDoc["profiles"]);

    if (!WriteFileUtf8(Util::GetCredentialsMetaPath(), credDoc.dump(2))) {
        error = L"Failed to write credentials.json";
        return false;
    }

    return true;
  } catch (const std::exception& ex) {
    error = Util::Utf8ToWide(ex.what());
    return false;
  }
}
