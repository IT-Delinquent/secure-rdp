#pragma once

#include "ConnectionTreeModel.h"
#include "DragDrop.h"
#include "RdpLauncher.h"
#include "RdpSessionManager.h"
#include "UiTheme.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
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
    std::vector<TreeNode*> GetSelectedSessionNodes();
    int GetSelectedTreeItemCount() const;
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
    void DeferredLayout();
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
    void ImportConnections();
    void ExportConnections();
    void SetCredentialOnSelected();
    void SetCredentialOnFolder();
    void ExpandCollapseSelectedFolder(bool expand);
    void AssignCredentialToSessions(const std::vector<TreeNode*>& sessions, const std::wstring& credentialId);
    void ShowContextMenu(int screenX, int screenY);
    void UpdateMenuState(HMENU menu);
    TreeNode* GetNodeForTreeItem(HTREEITEM item);
    bool HandleTreeSelChanging(LPNMTREEVIEW info);
    void ClearAllTreeSelections();
    void SelectSessionsOnly(const std::vector<HTREEITEM>& items);
    void EnforceSingleSelectionIfPlainClick(LPNMTREEVIEW selChange = nullptr);
    void BuildVisibleTreeOrder(std::vector<HTREEITEM>& out) const;
    void SelectSessionRange(HTREEITEM from, HTREEITEM to);
    bool HandleAltClickRange(HTREEITEM hit);
    bool OnTreeLButtonDown(HWND tree, bool ctrl, bool alt, bool shift, LPARAM lParam);
    void ApplyTheme();
    void SetThemePreference(UiTheme::ThemePreference preference);
    void UpdateThemeMenuChecks();
    void SaveAppSettings();
    int GetSplitX(int clientWidth) const;
    void BeginSplitterDrag(int x);
    void UpdateSplitterDrag(int x, int clientWidth);
    void EndSplitterDrag(int clientWidth);
    bool IsSplitterHit(int x, int y, int clientHeight) const;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK TreeInputSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                                DWORD_PTR refData);

    void FreeTreeItemData(HTREEITEM item);

    HWND hwnd_ = nullptr;
    HWND tree_ = nullptr;
    HWND panelHost_ = nullptr;
    HINSTANCE hInstance_ = nullptr;
    HIMAGELIST imageList_ = nullptr;
    HMENU fileMenu_ = nullptr;
    HMENU editMenu_ = nullptr;
    HMENU themeMenu_ = nullptr;

    HTREEITEM rangeAnchor_ = nullptr;
    bool suppressTreeSelChanging_ = false;
    bool suppressSelectionEnforce_ = false;

    std::unordered_map<std::wstring, HTREEITEM> idToItem_;
    DragDropController dragDrop_;
    std::unique_ptr<RdpLauncher> launcher_;
    RdpSessionManager sessionManager_;
    double panelSplitRatio_ = 0.35;
    int splitDragX_ = -1;
    bool splitterDragging_ = false;

    static MainWindow* s_instance;
};
