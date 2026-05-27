#include "ConnectionTreeModel.h"

#include "Util.h"

#include <algorithm>

void ConnectionTreeModel::ResetToDefault() {
    root_ = TreeNode{};
    root_.id = Util::NewUuid();
    root_.type = NodeType::Folder;
    root_.name = L"Connections";
    credentials_.clear();
}

TreeNode* ConnectionTreeModel::FindNodeIn(TreeNode& parent, const std::wstring& id) {
    if (parent.id == id) {
        return &parent;
    }
    for (auto& child : parent.children) {
        if (TreeNode* found = FindNodeIn(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

const TreeNode* ConnectionTreeModel::FindNodeIn(const TreeNode& parent, const std::wstring& id) const {
    if (parent.id == id) {
        return &parent;
    }
    for (const auto& child : parent.children) {
        if (const TreeNode* found = FindNodeIn(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

TreeNode* ConnectionTreeModel::FindNode(const std::wstring& id) {
    return FindNodeIn(root_, id);
}

const TreeNode* ConnectionTreeModel::FindNode(const std::wstring& id) const {
    return FindNodeIn(root_, id);
}

TreeNode* ConnectionTreeModel::FindParentIn(TreeNode& parent, const std::wstring& childId) {
    for (auto& child : parent.children) {
        if (child->id == childId) {
            return &parent;
        }
        if (TreeNode* found = FindParentIn(*child, childId)) {
            return found;
        }
    }
    return nullptr;
}

TreeNode* ConnectionTreeModel::FindParent(const std::wstring& childId) {
    if (root_.id == childId) {
        return nullptr;
    }
    return FindParentIn(root_, childId);
}

bool ConnectionTreeModel::IsDescendant(const TreeNode* ancestor, const TreeNode* node) const {
    if (!ancestor || !node) {
        return false;
    }
    if (ancestor == node) {
        return true;
    }
    for (const auto& child : ancestor->children) {
        if (IsDescendant(child.get(), node)) {
            return true;
        }
    }
    return false;
}

TreeNode* ConnectionTreeModel::AppendFolder(TreeNode& parent, const std::wstring& name) {
    auto node = std::make_unique<TreeNode>();
    node->id = Util::NewUuid();
    node->type = NodeType::Folder;
    node->name = name;
    TreeNode* ptr = node.get();
    parent.children.push_back(std::move(node));
    return ptr;
}

TreeNode* ConnectionTreeModel::AppendSession(TreeNode& parent, const std::wstring& name, const std::wstring& host,
                                             int port, const std::wstring& credentialId) {
    auto node = std::make_unique<TreeNode>();
    node->id = Util::NewUuid();
    node->type = NodeType::Session;
    node->name = name;
    node->host = host;
    node->port = port;
    node->credentialId = credentialId;
    TreeNode* ptr = node.get();
    parent.children.push_back(std::move(node));
    return ptr;
}

std::unique_ptr<TreeNode> ConnectionTreeModel::DetachFrom(TreeNode& parent, const std::wstring& id) {
    auto& children = parent.children;
    for (auto it = children.begin(); it != children.end(); ++it) {
        if ((*it)->id == id) {
            std::unique_ptr<TreeNode> removed = std::move(*it);
            children.erase(it);
            return removed;
        }
        if (auto nested = DetachFrom(**it, id)) {
            return nested;
        }
    }
    return nullptr;
}

std::unique_ptr<TreeNode> ConnectionTreeModel::DetachNode(const std::wstring& id) {
    if (root_.id == id) {
        return nullptr;
    }
    return DetachFrom(root_, id);
}

bool ConnectionTreeModel::RemoveNode(const std::wstring& id) {
    if (root_.id == id) {
        return false;
    }
    return DetachNode(id) != nullptr;
}

void ConnectionTreeModel::InsertSubtree(TreeNode& parent, std::unique_ptr<TreeNode> node, int insertIndex) {
    if (!node) {
        return;
    }
    if (insertIndex < 0 || insertIndex >= static_cast<int>(parent.children.size())) {
        parent.children.push_back(std::move(node));
    } else {
        parent.children.insert(parent.children.begin() + insertIndex, std::move(node));
    }
}

bool ConnectionTreeModel::MoveNode(const std::wstring& nodeId, TreeNode& newParent, int insertIndex) {
    if (nodeId == newParent.id) {
        return false;
    }
    auto node = DetachNode(nodeId);
    if (!node) {
        return false;
    }
    if (IsDescendant(node.get(), &newParent)) {
        InsertSubtree(root_, std::move(node));
        return false;
    }
    InsertSubtree(newParent, std::move(node), insertIndex);
    return true;
}

void ConnectionTreeModel::AssignNewIds(TreeNode& node) {
    node.id = Util::NewUuid();
    for (auto& child : node.children) {
        AssignNewIds(*child);
    }
}

std::unique_ptr<TreeNode> ConnectionTreeModel::CloneNode(const TreeNode& source, bool duplicateNameSuffix) {
    auto copy = std::make_unique<TreeNode>();
    copy->id = Util::NewUuid();
    copy->type = source.type;
    copy->name = source.name;
    if (duplicateNameSuffix) {
        copy->name += L" (copy)";
    }
    copy->host = source.host;
    copy->port = source.port;
    copy->credentialId = source.credentialId;
    for (const auto& child : source.children) {
        copy->children.push_back(CloneNode(*child, duplicateNameSuffix));
    }
    return copy;
}

std::unique_ptr<TreeNode> ConnectionTreeModel::CloneSubtree(const TreeNode& source, bool duplicateNameSuffix) {
    return CloneNode(source, duplicateNameSuffix);
}

CredentialMeta* ConnectionTreeModel::FindCredential(const std::wstring& id) {
    for (auto& c : credentials_) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

const CredentialMeta* ConnectionTreeModel::FindCredential(const std::wstring& id) const {
    for (const auto& c : credentials_) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

CredentialMeta* ConnectionTreeModel::AddCredential(const std::wstring& label, const std::wstring& username,
                                                 const std::wstring& domain) {
    CredentialMeta meta;
    meta.id = Util::NewUuid();
    meta.label = label;
    meta.username = username;
    meta.domain = domain;
    credentials_.push_back(meta);
    return &credentials_.back();
}

bool ConnectionTreeModel::RemoveCredential(const std::wstring& id) {
    auto it = std::remove_if(credentials_.begin(), credentials_.end(),
                             [&](const CredentialMeta& c) { return c.id == id; });
    if (it == credentials_.end()) {
        return false;
    }
    credentials_.erase(it, credentials_.end());
    return true;
}

void ConnectionTreeModel::CollectSessions(const TreeNode& folder, bool recursive, std::vector<TreeNode*>& out) {
    for (const auto& child : folder.children) {
        if (child->IsSession()) {
            out.push_back(child.get());
        } else if (recursive && child->IsFolder()) {
            CollectSessions(*child, true, out);
        }
    }
}
