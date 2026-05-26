#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class NodeType { Folder, Session };

struct TreeNode {
    std::wstring id;
    NodeType type = NodeType::Folder;
    std::wstring name;
    std::wstring host;
    int port = 3389;
    std::wstring credentialId;
    std::vector<std::unique_ptr<TreeNode>> children;

    bool IsFolder() const { return type == NodeType::Folder; }
    bool IsSession() const { return type == NodeType::Session; }
};

struct CredentialMeta {
    std::wstring id;
    std::wstring label;
    std::wstring username;
    std::wstring domain;
};

class ConnectionTreeModel {
public:
    TreeNode& Root() { return root_; }
    const TreeNode& Root() const { return root_; }

    std::vector<CredentialMeta>& Credentials() { return credentials_; }
    const std::vector<CredentialMeta>& Credentials() const { return credentials_; }

    void ResetToDefault();
    TreeNode* FindNode(const std::wstring& id);
    const TreeNode* FindNode(const std::wstring& id) const;
    TreeNode* FindParent(const std::wstring& childId);
    bool IsDescendant(const TreeNode* ancestor, const TreeNode* node) const;

    TreeNode* AppendFolder(TreeNode& parent, const std::wstring& name);
    TreeNode* AppendSession(TreeNode& parent, const std::wstring& name, const std::wstring& host, int port,
                            const std::wstring& credentialId);
    bool RemoveNode(const std::wstring& id);
    bool MoveNode(const std::wstring& nodeId, TreeNode& newParent, int insertIndex = -1);

    std::unique_ptr<TreeNode> DetachNode(const std::wstring& id);
    std::unique_ptr<TreeNode> CloneSubtree(const TreeNode& source, bool duplicateNameSuffix = false);
    void InsertSubtree(TreeNode& parent, std::unique_ptr<TreeNode> node, int insertIndex = -1);

    CredentialMeta* FindCredential(const std::wstring& id);
    const CredentialMeta* FindCredential(const std::wstring& id) const;
    CredentialMeta* AddCredential(const std::wstring& label, const std::wstring& username, const std::wstring& domain);
    bool RemoveCredential(const std::wstring& id);

    void AssignNewIds(TreeNode& node);

    static void CollectSessions(const TreeNode& folder, bool recursive, std::vector<TreeNode*>& out);

private:
    TreeNode* FindNodeIn(TreeNode& parent, const std::wstring& id);
    const TreeNode* FindNodeIn(const TreeNode& parent, const std::wstring& id) const;
    TreeNode* FindParentIn(TreeNode& parent, const std::wstring& childId);
    std::unique_ptr<TreeNode> DetachFrom(TreeNode& parent, const std::wstring& id);
    std::unique_ptr<TreeNode> CloneNode(const TreeNode& source, bool duplicateNameSuffix);

    TreeNode root_;
    std::vector<CredentialMeta> credentials_;
};
