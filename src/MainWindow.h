#pragma once

#include "ConnectionTreeModel.h"
#include "DragDrop.h"
#include "RdpLauncher.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <windows.h>

class MainWindow {
public:
    static bool Create(HINSTANCE hInstance);
    static MainWindow* Get();

    HWND Hwnd() const { return hwnd_; }
    ConnectionTreeModel& Model();

    void RefreshTree();
    std::wstring GetSelectedNodeId() const;
    TreeNode* GetSelectedNode();
    HTREEITEM GetItemForId(const std::wstring& id) const;
    HTREEITEM HitTestItem(int x, int y) const;

    bool ReparentNode(const std::wstring& nodeId, const std::wstring& newParentId, int insertIndex = -1);
    void Save();

private:
    MainWindow() = default;

    bool RegisterClass(HINSTANCE instance);
    bool CreateWindowInstance(HINSTANCE instance);
    void CreateControls();
    void LayoutControls(int width, int height);
    void CreateMenus();
    void PopulateTree(const TreeNode& node, HTREEITEM parent);
    HTREEITEM InsertTreeItem(const TreeNode& node, HTREEITEM parent);

    void OnCommand(int id);
    void OnNotify(LPNMHDR hdr);
    void ConnectSelected();
    void NewSession();
    void NewFolder();
    void EditSelected();
    void DeleteSelected();
    void DuplicateSelected();
    void CopySelected();
    void PasteToSelected();
    void ShowContextMenu(int x, int y);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void SetStatusText(const std::wstring& text);
    void FreeTreeItemData(HTREEITEM item);

    HWND hwnd_ = nullptr;
    HWND tree_ = nullptr;
    HWND status_ = nullptr;
    HINSTANCE hInstance_ = nullptr;
    HIMAGELIST imageList_ = nullptr;

    std::unordered_map<std::wstring, HTREEITEM> idToItem_;
    DragDropController dragDrop_;
    std::unique_ptr<RdpLauncher> launcher_;

    static MainWindow* s_instance;
};
