#include "MainWindow.h"

#include "App.h"
#include "Clipboard.h"
#include "Dialogs/AboutDialog.h"
#include "Dialogs/AssignCredentialDialog.h"
#include "Dialogs/ChangelogDialog.h"
#include "Dialogs/CredentialDialog.h"
#include "Dialogs/FolderDialog.h"
#include "Dialogs/FolderScopeDialog.h"
#include "Dialogs/SessionDialog.h"
#include "ThemeSettings.h"
#include "Logger.h"
#include "MRemoteNgExchange.h"
#include "Resource.h"
#include "UiTheme.h"
#include "Version.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <sstream>
#include <vector>
#include <windows.h>
#include <windowsx.h>

namespace {

constexpr UINT_PTR kTreeInputSubclassId = 3;

struct MainMenuItems {
    UiTheme::MenuItemData fileLabel{};
    UiTheme::MenuItemData editLabel{};
    UiTheme::MenuItemData connect{};
    UiTheme::MenuItemData newSession{};
    UiTheme::MenuItemData newFolder{};
    UiTheme::MenuItemData manageCreds{};
    UiTheme::MenuItemData import{};
    UiTheme::MenuItemData exportItem{};
    UiTheme::MenuItemData about{};
    UiTheme::MenuItemData exit{};
    UiTheme::MenuItemData edit{};
    UiTheme::MenuItemData del{};
    UiTheme::MenuItemData duplicate{};
    UiTheme::MenuItemData copy{};
    UiTheme::MenuItemData paste{};
    UiTheme::MenuItemData changelog{};
    UiTheme::MenuItemData setCredential{};
    UiTheme::MenuItemData themeLabel{};
    UiTheme::MenuItemData themeSystem{};
    UiTheme::MenuItemData themeLight{};
    UiTheme::MenuItemData themeDark{};
    UiTheme::MenuItemData fileSep1{};
    UiTheme::MenuItemData fileSep2{};
    UiTheme::MenuItemData fileSep3{};
    UiTheme::MenuItemData fileSep4{};
    UiTheme::MenuItemData fileSep5{};
    UiTheme::MenuItemData fileSep6{};
    UiTheme::MenuItemData editSep1{};
    UiTheme::MenuItemData editSep2{};

    MainMenuItems() {
        UiTheme::MenuInitItem(fileLabel, L"&File");
        fileLabel.compact = true;
        UiTheme::MenuInitItem(editLabel, L"&Edit");
        editLabel.compact = true;
        UiTheme::MenuInitItem(connect, L"&Connect\tEnter");
        UiTheme::MenuInitItem(newSession, L"New &Session");
        UiTheme::MenuInitItem(newFolder, L"New &Folder");
        UiTheme::MenuInitItem(manageCreds, L"Manage &Credentials...");
        UiTheme::MenuInitItem(import, L"&Import...");
        UiTheme::MenuInitItem(exportItem, L"E&xport...");
        UiTheme::MenuInitItem(changelog, L"Chan&gelog...");
        UiTheme::MenuInitItem(about, L"&About...");
        UiTheme::MenuInitItem(exit, L"E&xit");
        UiTheme::MenuInitItem(edit, L"&Edit");
        UiTheme::MenuInitItem(del, L"&Delete");
        UiTheme::MenuInitItem(duplicate, L"Dupli&cate");
        UiTheme::MenuInitItem(copy, L"&Copy\tCtrl+C");
        UiTheme::MenuInitItem(paste, L"&Paste\tCtrl+V");
        UiTheme::MenuInitItem(setCredential, L"Set &Credential...");
        UiTheme::MenuInitItem(themeLabel, L"&Theme");
        UiTheme::MenuInitItem(themeSystem, L"&System default");
        UiTheme::MenuInitItem(themeLight, L"&Light");
        UiTheme::MenuInitItem(themeDark, L"&Dark");
        UiTheme::MenuInitSeparator(fileSep1);
        UiTheme::MenuInitSeparator(fileSep2);
        UiTheme::MenuInitSeparator(fileSep3);
        UiTheme::MenuInitSeparator(fileSep4);
        UiTheme::MenuInitSeparator(fileSep5);
        UiTheme::MenuInitSeparator(fileSep6);
        UiTheme::MenuInitSeparator(editSep1);
        UiTheme::MenuInitSeparator(editSep2);
    }
};

MainMenuItems& MainMenu() {
    static MainMenuItems items;
    return items;
}

}  // namespace

MainWindow* MainWindow::s_instance = nullptr;

MainWindow* MainWindow::Get() {
    return s_instance;
}

ConnectionTreeModel& MainWindow::Model() {
    return App::Instance().Model();
}

bool MainWindow::Create(HINSTANCE hInstance) {
    static MainWindow window;
    s_instance = &window;
    window.hInstance_ = hInstance;
    if (!window.RegisterClass(hInstance)) {
        return false;
    }
    if (!window.CreateWindowInstance(hInstance)) {
        return false;
    }
    window.launcher_ = std::make_unique<RdpLauncher>(App::Instance().Model());
    window.RefreshTree();
    ShowWindow(window.hwnd_, SW_SHOW);
    UpdateWindow(window.hwnd_);
    return true;
}

bool MainWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"TinyRdpMainWindow";
    wc.hIconSm = wc.hIcon;
    return RegisterClassExW(&wc) != 0;
}

bool MainWindow::CreateWindowInstance(HINSTANCE hInstance) {
    hwnd_ = CreateWindowExW(0, L"TinyRdpMainWindow", SECURE_RDP_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                            CW_USEDEFAULT, 960, 640, nullptr, nullptr, hInstance, this);
    return hwnd_ != nullptr;
}

void MainWindow::CreateControls() {
    CreateMenus();

    tree_ = CreateWindowExW(0, WC_TREEVIEWW, L"",
                            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS |
                                TVS_SHOWSELALWAYS | TVS_EDITLABELS | TVS_FULLROWSELECT,
                            0, 0, 100, 100, hwnd_, reinterpret_cast<HMENU>(IDC_TREE), hInstance_, nullptr);

    imageList_ = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
    HICON folderIcon = UiTheme::LoadStockIcon(SIID_FOLDER);
    HICON computerIcon = UiTheme::LoadStockIcon(SIID_DESKTOPPC);
    if (!folderIcon) {
        folderIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    if (!computerIcon) {
        computerIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    ImageList_AddIcon(imageList_, folderIcon);
    ImageList_AddIcon(imageList_, computerIcon);
    if (folderIcon) {
        DestroyIcon(folderIcon);
    }
    if (computerIcon) {
        DestroyIcon(computerIcon);
    }
    TreeView_SetImageList(tree_, imageList_, TVSIL_NORMAL);
    TreeView_SetExtendedStyle(tree_, TVS_EX_MULTISELECT, TVS_EX_MULTISELECT);
    SetWindowSubclass(tree_, MainWindow::TreeInputSubclassProc, kTreeInputSubclassId,
                      reinterpret_cast<DWORD_PTR>(this));
    UiTheme::ApplyTree(tree_);

    UiTheme::ThemePreference pref = UiTheme::ThemePreference::System;
    ThemeSettingsLoad(pref);
    UiTheme::SetPreference(pref);
    ApplyTheme();
}

void MainWindow::CreateMenus() {
    MainMenuItems& m = MainMenu();
    HMENU menuBar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    HMENU editMenu = CreatePopupMenu();

    UiTheme::MenuAppend(fileMenu, IDM_CONNECT, m.connect);
    UiTheme::MenuAppendSeparator(fileMenu, IDM_SEP_FILE_1, m.fileSep1);
    UiTheme::MenuAppend(fileMenu, IDM_NEW_SESSION, m.newSession);
    UiTheme::MenuAppend(fileMenu, IDM_NEW_FOLDER, m.newFolder);
    UiTheme::MenuAppendSeparator(fileMenu, IDM_SEP_FILE_2, m.fileSep2);
    UiTheme::MenuAppend(fileMenu, IDM_MANAGE_CREDS, m.manageCreds);
    UiTheme::MenuAppendSeparator(fileMenu, IDM_SEP_FILE_3, m.fileSep3);
    UiTheme::MenuAppend(fileMenu, IDM_IMPORT, m.import);
    UiTheme::MenuAppend(fileMenu, IDM_EXPORT, m.exportItem);
    UiTheme::MenuAppendSeparator(fileMenu, IDM_SEP_FILE_4, m.fileSep4);
    UiTheme::MenuAppend(fileMenu, IDM_CHANGELOG, m.changelog);
    UiTheme::MenuAppendSeparator(fileMenu, IDM_SEP_FILE_5, m.fileSep5);
    HMENU themeMenu = CreatePopupMenu();
    UiTheme::MenuAppend(themeMenu, IDM_THEME_SYSTEM, m.themeSystem);
    UiTheme::MenuAppend(themeMenu, IDM_THEME_LIGHT, m.themeLight);
    UiTheme::MenuAppend(themeMenu, IDM_THEME_DARK, m.themeDark);
    UiTheme::MenuAppend(fileMenu, reinterpret_cast<UINT_PTR>(themeMenu), m.themeLabel, MF_POPUP);
    UiTheme::MenuAppendSeparator(fileMenu, IDM_SEP_FILE_6, m.fileSep6);
    UiTheme::MenuAppend(fileMenu, IDM_ABOUT, m.about);
    UiTheme::MenuAppend(fileMenu, IDM_EXIT, m.exit);

    UiTheme::MenuAppend(editMenu, IDM_EDIT, m.edit);
    UiTheme::MenuAppend(editMenu, IDM_DELETE, m.del);
    UiTheme::MenuAppend(editMenu, IDM_DUPLICATE, m.duplicate);
    UiTheme::MenuAppendSeparator(editMenu, IDM_SEP_EDIT_1, m.editSep1);
    UiTheme::MenuAppend(editMenu, IDM_COPY, m.copy);
    UiTheme::MenuAppend(editMenu, IDM_PASTE, m.paste);
    UiTheme::MenuAppendSeparator(editMenu, IDM_SEP_EDIT_2, m.editSep2);
    UiTheme::MenuAppend(editMenu, IDM_SET_CREDENTIAL, m.setCredential);

    UiTheme::MenuAppend(menuBar, reinterpret_cast<UINT_PTR>(fileMenu), m.fileLabel, MF_POPUP);
    UiTheme::MenuAppend(menuBar, reinterpret_cast<UINT_PTR>(editMenu), m.editLabel, MF_POPUP);
    fileMenu_ = fileMenu;
    editMenu_ = editMenu;
    themeMenu_ = themeMenu;
    SetMenu(hwnd_, menuBar);
    UpdateThemeMenuChecks();
}

TreeNode* MainWindow::GetNodeForTreeItem(HTREEITEM item) {
    if (!item) {
        return nullptr;
    }
    TVITEMW tv{};
    tv.mask = TVIF_PARAM;
    tv.hItem = item;
    if (!TreeView_GetItem(tree_, &tv) || !tv.lParam) {
        return nullptr;
    }
    return Model().FindNode(*reinterpret_cast<std::wstring*>(tv.lParam));
}

void MainWindow::UpdateMenuState(HMENU menu) {
    TreeNode* node = GetSelectedNode();
    const bool hasEditableItem = node && node->id != Model().Root().id;
    const bool isSession = node && node->IsSession();
    const int selectedCount = GetSelectedTreeItemCount();
    const auto selectedSessions = GetSelectedSessionNodes();
    const bool multiSelect = selectedCount > 1;
    const bool canBulkCredential = selectedSessions.size() >= 2;
    const UINT enable = MF_BYCOMMAND | MF_ENABLED;
    const UINT disable = MF_BYCOMMAND | MF_GRAYED;

    if (menu == fileMenu_) {
        EnableMenuItem(menu, IDM_CONNECT, isSession && !multiSelect ? enable : disable);
    }
    if (menu == editMenu_) {
        const bool singleItemActions = hasEditableItem && !multiSelect;
        EnableMenuItem(menu, IDM_EDIT, singleItemActions ? enable : disable);
        EnableMenuItem(menu, IDM_DELETE, singleItemActions ? enable : disable);
        EnableMenuItem(menu, IDM_DUPLICATE, singleItemActions ? enable : disable);
        EnableMenuItem(menu, IDM_COPY, singleItemActions ? enable : disable);
        EnableMenuItem(menu, IDM_PASTE, ClipboardManager::HasPasteData() ? enable : disable);
        EnableMenuItem(menu, IDM_SET_CREDENTIAL, canBulkCredential ? enable : disable);
    }
}

void MainWindow::FreeTreeItemData(HTREEITEM item) {
    if (!item) {
        return;
    }
    TVITEMW tv{};
    tv.mask = TVIF_PARAM;
    tv.hItem = item;
    if (TreeView_GetItem(tree_, &tv) && tv.lParam) {
        delete reinterpret_cast<std::wstring*>(tv.lParam);
        tv.lParam = 0;
        TreeView_SetItem(tree_, &tv);
    }
}

void MainWindow::LayoutControls(int width, int height) {
    const int treeX = 0;
    const int treeOverlap = UiTheme::IsDarkEffective() ? 1 : 0;
    const int treeY = UiTheme::kMainWindowTreeTopPadding - treeOverlap;
    const int treeW = width;
    const int treeH = height - UiTheme::kMainWindowTreeTopPadding + treeOverlap;
    SetWindowPos(tree_, nullptr, treeX, treeY, treeW > 0 ? treeW : 0, treeH > 0 ? treeH : 0, SWP_NOZORDER);
}

HTREEITEM MainWindow::InsertTreeItem(const TreeNode& node, HTREEITEM parent) {
    std::wstring text = node.name;
    if (node.IsSession()) {
        text += L" (" + node.host;
        if (node.port != 3389) {
            text += L":" + std::to_wstring(node.port);
        }
        text += L")";
    }

    TVINSERTSTRUCTW ins{};
    ins.hParent = parent;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    ins.item.pszText = const_cast<LPWSTR>(text.c_str());
    ins.item.iImage = node.IsFolder() ? 0 : 1;
    ins.item.iSelectedImage = ins.item.iImage;
    ins.item.lParam = reinterpret_cast<LPARAM>(new std::wstring(node.id));

    const HTREEITEM item = TreeView_InsertItem(tree_, &ins);
    idToItem_[node.id] = item;
    return item;
}

void MainWindow::PopulateTree(const TreeNode& node, HTREEITEM parent) {
    const HTREEITEM item = InsertTreeItem(node, parent);
    for (const auto& child : node.children) {
        PopulateTree(*child, item);
    }
    TreeView_Expand(tree_, item, TVE_EXPAND);
}

void MainWindow::RefreshTree() {
    LOG_DEBUG(L"RefreshTree: rebuilding tree view");
    rangeAnchor_ = nullptr;
    TreeView_DeleteAllItems(tree_);
    idToItem_.clear();

    const HTREEITEM rootItem = InsertTreeItem(Model().Root(), TVI_ROOT);
    for (const auto& child : Model().Root().children) {
        PopulateTree(*child, rootItem);
    }
    TreeView_Expand(tree_, rootItem, TVE_EXPAND);
}

std::wstring MainWindow::GetSelectedNodeId() const {
    HTREEITEM item = TreeView_GetSelection(tree_);
    if (!item) {
        return L"";
    }
    TVITEMW tv{};
    tv.mask = TVIF_PARAM;
    tv.hItem = item;
    if (!TreeView_GetItem(tree_, &tv) || !tv.lParam) {
        return L"";
    }
    return *reinterpret_cast<std::wstring*>(tv.lParam);
}

TreeNode* MainWindow::GetSelectedNode() {
    const std::wstring id = GetSelectedNodeId();
    if (id.empty()) {
        return nullptr;
    }
    return Model().FindNode(id);
}

int MainWindow::GetSelectedTreeItemCount() const {
    return TreeView_GetSelectedCount(tree_);
}

std::vector<TreeNode*> MainWindow::GetSelectedSessionNodes() {
    std::vector<TreeNode*> sessions;
    for (HTREEITEM item = nullptr;;) {
        item = TreeView_GetNextItem(tree_, item, TVGN_NEXTSELECTED);
        if (!item) {
            break;
        }
        TreeNode* node = GetNodeForTreeItem(item);
        if (node && node->IsSession()) {
            sessions.push_back(node);
        }
    }
    return sessions;
}

LRESULT CALLBACK MainWindow::TreeInputSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR,
                                                 DWORD_PTR refData) {
    auto* self = reinterpret_cast<MainWindow*>(refData);
    if (!self) {
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    switch (msg) {
        case WM_LBUTTONDOWN: {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (self->OnTreeLButtonDown(hwnd, ctrl, alt, shift, lParam)) {
                return 0;
            }
            break;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, TreeInputSubclassProc, kTreeInputSubclassId);
            break;
        default:
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool MainWindow::OnTreeLButtonDown(HWND tree, bool ctrl, bool alt, bool shift, LPARAM lParam) {
    if (ctrl || alt || shift) {
        return false;
    }

    TVHITTESTINFO ht{};
    ht.pt.x = GET_X_LPARAM(lParam);
    ht.pt.y = GET_Y_LPARAM(lParam);
    const HTREEITEM hit = TreeView_HitTest(tree, &ht);
    TreeNode* node = GetNodeForTreeItem(hit);
    if (!node || !node->IsSession()) {
        return false;
    }

    if (TreeView_GetSelectedCount(tree_) > 1) {
        rangeAnchor_ = nullptr;
        SelectSessionsOnly({hit});
        return true;
    }

    return false;
}

bool MainWindow::HandleTreeSelChanging(LPNMTREEVIEW info) {
    if (!info || !info->itemNew.hItem || suppressTreeSelChanging_) {
        return false;
    }

    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    TreeNode* node = GetNodeForTreeItem(info->itemNew.hItem);
    if (alt && !ctrl && !shift && node && node->IsSession()) {
        suppressTreeSelChanging_ = true;
        HandleAltClickRange(info->itemNew.hItem);
        suppressTreeSelChanging_ = false;
        return true;
    }

    if (node && node->IsSession()) {
        if (!ctrl && !shift && !alt && TreeView_GetSelectedCount(tree_) > 1) {
            suppressTreeSelChanging_ = true;
            rangeAnchor_ = nullptr;
            SelectSessionsOnly({info->itemNew.hItem});
            suppressTreeSelChanging_ = false;
            return true;
        }
        return false;
    }
    if (!node) {
        return false;
    }
    return ctrl || shift;
}

void MainWindow::ClearAllTreeSelections() {
    const HTREEITEM root = TreeView_GetRoot(tree_);
    if (!root) {
        return;
    }

    auto walk = [&](auto&& self, HTREEITEM parent) -> void {
        for (HTREEITEM child = TreeView_GetChild(tree_, parent); child;
             child = TreeView_GetNextSibling(tree_, child)) {
            TreeView_SetItemState(tree_, child, 0, TVIS_SELECTED);
            self(self, child);
        }
    };
    walk(walk, root);
}

void MainWindow::SelectSessionsOnly(const std::vector<HTREEITEM>& items) {
    suppressSelectionEnforce_ = true;
    suppressTreeSelChanging_ = true;
    ClearAllTreeSelections();
    for (HTREEITEM item : items) {
        if (item) {
            TreeView_SetItemState(tree_, item, TVIS_SELECTED, TVIS_SELECTED);
        }
    }
    if (!items.empty()) {
        TreeView_SelectItem(tree_, items.back());
    }
    suppressTreeSelChanging_ = false;
    suppressSelectionEnforce_ = false;
}

void MainWindow::EnforceSingleSelectionIfPlainClick(LPNMTREEVIEW selChange) {
    if (suppressSelectionEnforce_) {
        return;
    }
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetKeyState(VK_SHIFT) & 0x8000) != 0 ||
        (GetKeyState(VK_MENU) & 0x8000) != 0) {
        return;
    }
    if (TreeView_GetSelectedCount(tree_) <= 1) {
        return;
    }

    rangeAnchor_ = nullptr;

    HTREEITEM keep = TreeView_GetSelection(tree_);
    if (selChange && selChange->itemNew.hItem) {
        TreeNode* clicked = GetNodeForTreeItem(selChange->itemNew.hItem);
        if (clicked && clicked->IsSession()) {
            keep = selChange->itemNew.hItem;
        }
    }
    if (!keep) {
        suppressSelectionEnforce_ = true;
        ClearAllTreeSelections();
        suppressSelectionEnforce_ = false;
        return;
    }

    SelectSessionsOnly({keep});
}

void MainWindow::BuildVisibleTreeOrder(std::vector<HTREEITEM>& out) const {
    out.clear();
    const HTREEITEM root = TreeView_GetRoot(tree_);
    if (!root) {
        return;
    }

    auto walk = [&](auto&& self, HTREEITEM parent) -> void {
        for (HTREEITEM child = TreeView_GetChild(tree_, parent); child;
             child = TreeView_GetNextSibling(tree_, child)) {
            out.push_back(child);
            if (TreeView_GetItemState(tree_, child, TVIS_EXPANDED) & TVIS_EXPANDED) {
                self(self, child);
            }
        }
    };
    walk(walk, root);
}

void MainWindow::SelectSessionRange(HTREEITEM from, HTREEITEM to) {
    if (!from || !to) {
        return;
    }
    std::vector<HTREEITEM> order;
    BuildVisibleTreeOrder(order);
    auto fromIt = std::find(order.begin(), order.end(), from);
    auto toIt = std::find(order.begin(), order.end(), to);
    if (fromIt == order.end() || toIt == order.end()) {
        return;
    }
    if (fromIt > toIt) {
        std::swap(fromIt, toIt);
    }

    std::vector<HTREEITEM> sessions;
    for (auto it = fromIt; it <= toIt; ++it) {
        TreeNode* node = GetNodeForTreeItem(*it);
        if (node && node->IsSession()) {
            sessions.push_back(*it);
        }
    }
    SelectSessionsOnly(sessions);
}

bool MainWindow::HandleAltClickRange(HTREEITEM hit) {
    if ((GetKeyState(VK_MENU) & 0x8000) == 0 || !hit) {
        return false;
    }
    TreeNode* node = GetNodeForTreeItem(hit);
    if (!node || !node->IsSession()) {
        return false;
    }

    if (!rangeAnchor_) {
        rangeAnchor_ = hit;
        SelectSessionsOnly({hit});
        return true;
    }

    bool anchorValid = false;
    std::vector<HTREEITEM> order;
    BuildVisibleTreeOrder(order);
    for (HTREEITEM item : order) {
        if (item == rangeAnchor_) {
            anchorValid = true;
            break;
        }
    }
    if (!anchorValid) {
        rangeAnchor_ = hit;
        SelectSessionsOnly({hit});
        return true;
    }

    SelectSessionRange(rangeAnchor_, hit);
    rangeAnchor_ = hit;
    return true;
}

void MainWindow::ApplyTheme() {
    UiTheme::ApplyMainWindow(hwnd_, tree_);
}

void MainWindow::SetThemePreference(UiTheme::ThemePreference preference) {
    UiTheme::SetPreference(preference);
    ThemeSettingsSave(preference);
    ApplyTheme();
    UpdateThemeMenuChecks();
}

void MainWindow::UpdateThemeMenuChecks() {
    if (!themeMenu_) {
        return;
    }
    const UiTheme::ThemePreference pref = UiTheme::GetPreference();
    auto check = [&](UINT id, bool on) {
        CheckMenuItem(themeMenu_, id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED));
    };
    check(IDM_THEME_SYSTEM, pref == UiTheme::ThemePreference::System);
    check(IDM_THEME_LIGHT, pref == UiTheme::ThemePreference::Light);
    check(IDM_THEME_DARK, pref == UiTheme::ThemePreference::Dark);
}

void MainWindow::AssignCredentialToSessions(const std::vector<TreeNode*>& sessions,
                                            const std::wstring& credentialId) {
    for (TreeNode* session : sessions) {
        if (session) {
            session->credentialId = credentialId;
        }
    }
    RefreshTree();
    Save();
}

void MainWindow::SetCredentialOnSelected() {
    const std::vector<TreeNode*> sessions = GetSelectedSessionNodes();
    if (sessions.size() < 2) {
        MessageBoxW(hwnd_, L"Select two or more sessions (Ctrl+click).", L"Set Credential", MB_ICONINFORMATION);
        return;
    }
    AssignCredentialResult dlg;
    if (!ShowAssignCredentialDialog(hwnd_, Model(), static_cast<int>(sessions.size()), dlg)) {
        return;
    }
    AssignCredentialToSessions(sessions, dlg.credentialId);
}

void MainWindow::SetCredentialOnFolder() {
    TreeNode* folder = GetSelectedNode();
    if (!folder || !folder->IsFolder()) {
        return;
    }

    std::vector<TreeNode*> direct;
    ConnectionTreeModel::CollectSessions(*folder, false, direct);
    std::vector<TreeNode*> subtree;
    ConnectionTreeModel::CollectSessions(*folder, true, subtree);

    if (subtree.empty()) {
        MessageBoxW(hwnd_, L"This folder has no sessions.", L"Set Credential", MB_ICONINFORMATION);
        return;
    }

    const FolderScopeChoice scope = ShowFolderScopeDialog(hwnd_, folder->name);
    if (scope == FolderScopeChoice::Cancelled) {
        return;
    }

    std::vector<TreeNode*>& sessions =
        scope == FolderScopeChoice::DirectChildren ? direct : subtree;
    if (sessions.empty()) {
        MessageBoxW(hwnd_, L"No sessions in the selected scope.", L"Set Credential", MB_ICONINFORMATION);
        return;
    }

    if (sessions.size() >= 10) {
        const std::wstring msg = L"Assign credential to " + std::to_wstring(sessions.size()) + L" sessions?";
        if (MessageBoxW(hwnd_, msg.c_str(), L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
    }

    AssignCredentialResult dlg;
    if (!ShowAssignCredentialDialog(hwnd_, Model(), static_cast<int>(sessions.size()), dlg)) {
        return;
    }
    AssignCredentialToSessions(sessions, dlg.credentialId);
}

HTREEITEM MainWindow::GetItemForId(const std::wstring& id) const {
    auto it = idToItem_.find(id);
    return it != idToItem_.end() ? it->second : nullptr;
}

HTREEITEM MainWindow::HitTestItem(int x, int y) const {
    TVHITTESTINFO ht{};
    ht.pt.x = x;
    ht.pt.y = y;
    return TreeView_HitTest(tree_, &ht);
}

bool MainWindow::ReparentNode(const std::wstring& nodeId, const std::wstring& newParentId, int insertIndex) {
    if (nodeId.empty() || newParentId.empty() || nodeId == newParentId) {
        return false;
    }
    TreeNode* parent = Model().FindNode(newParentId);
    if (!parent || !parent->IsFolder()) {
        return false;
    }
    if (!Model().MoveNode(nodeId, *parent, insertIndex)) {
        return false;
    }
    RefreshTree();
    Save();
    return true;
}

void MainWindow::Save() {
    std::wstring error;
    if (!App::Instance().Save(error)) {
        LOG_ERROR(L"Save failed: " + error);
        return;
    }
    LOG_DEBUG(L"Save: ok");
}

void MainWindow::ConnectSelected() {
    TreeNode* node = GetSelectedNode();
    if (!node || !node->IsSession()) {
        MessageBoxW(hwnd_, L"Select an RDP session to connect.", L"Connect", MB_ICONINFORMATION);
        return;
    }
    std::wstring error;
    if (!launcher_->Connect(*node, error)) {
        MessageBoxW(hwnd_, error.c_str(), L"Connect failed", MB_ICONERROR);
        return;
    }
}

void MainWindow::NewSession() {
    const std::wstring parentId = GetSelectedNodeId();
    TreeNode* parent = Model().FindNode(parentId.empty() ? Model().Root().id : parentId);
    if (!parent) {
        parent = &Model().Root();
    }
    if (!parent->IsFolder()) {
        parent = Model().FindParent(parent->id);
    }
    if (!parent) {
        parent = &Model().Root();
    }

    SessionDialogResult dlg;
    if (!ShowSessionDialog(hwnd_, Model(), nullptr, dlg)) {
        return;
    }
    Model().AppendSession(*parent, dlg.name, dlg.host, dlg.port, dlg.credentialId);
    RefreshTree();
    Save();
}

void MainWindow::NewFolder() {
    LOG_INFO(L"NewFolder: start");
    const std::wstring parentId = GetSelectedNodeId();
    TreeNode* parent = Model().FindNode(parentId.empty() ? Model().Root().id : parentId);
    if (!parent) {
        parent = &Model().Root();
    }
    if (!parent->IsFolder()) {
        parent = Model().FindParent(parent->id);
    }
    if (!parent) {
        parent = &Model().Root();
    }

    FolderDialogResult dlg;
    if (!ShowFolderDialog(hwnd_, L"", dlg)) {
        LOG_INFO(L"NewFolder: cancelled");
        return;
    }

    TreeNode* created = Model().AppendFolder(*parent, dlg.name);
    if (!created) {
        LOG_ERROR(L"NewFolder: AppendFolder failed");
        return;
    }
    LOG_INFO(L"NewFolder: created '" + dlg.name + L"' under '" + parent->name + L"'");
    RefreshTree();
    Save();
}

void MainWindow::EditSelected() {
    TreeNode* node = GetSelectedNode();
    if (!node || node->id == Model().Root().id) {
        return;
    }
    if (node->IsFolder()) {
        FolderDialogResult dlg;
        if (!ShowFolderDialog(hwnd_, node->name, dlg)) {
            return;
        }
        node->name = dlg.name;
    } else {
        SessionDialogResult dlg;
        if (!ShowSessionDialog(hwnd_, Model(), node, dlg)) {
            return;
        }
        node->name = dlg.name;
        node->host = dlg.host;
        node->port = dlg.port;
        node->credentialId = dlg.credentialId;
    }
    RefreshTree();
    Save();
}

void MainWindow::DeleteSelected() {
    TreeNode* node = GetSelectedNode();
    if (!node || node->id == Model().Root().id) {
        return;
    }
    if (MessageBoxW(hwnd_, L"Delete selected item?", L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    Model().RemoveNode(node->id);
    RefreshTree();
    Save();
}

void MainWindow::DuplicateSelected() {
    TreeNode* node = GetSelectedNode();
    if (!node || node->id == Model().Root().id) {
        return;
    }
    TreeNode* parent = Model().FindParent(node->id);
    if (!parent) {
        return;
    }
    auto copy = Model().CloneSubtree(*node, true);
    Model().InsertSubtree(*parent, std::move(copy));
    RefreshTree();
    Save();
}

void MainWindow::CopySelected() {
    TreeNode* node = GetSelectedNode();
    if (!node) {
        return;
    }
    if (!ClipboardManager::CopySubtree(*node)) {
        MessageBoxW(hwnd_, L"Copy failed.", L"Copy", MB_ICONERROR);
    }
}

namespace {

bool BrowseXmlFile(HWND owner, bool save, std::wstring& path) {
    wchar_t buffer[MAX_PATH]{};
    if (!path.empty() && path.size() < MAX_PATH) {
        wcscpy_s(buffer, path.c_str());
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"mRemoteNG XML (*.xml)\0*.xml\0All files (*.*)\0*.*\0\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"xml";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (save) {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
    } else {
        ofn.Flags |= OFN_FILEMUSTEXIST;
    }

    if (save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn)) {
        path = buffer;
        return true;
    }
    return false;
}

TreeNode* ResolveImportTarget(ConnectionTreeModel& model, const std::wstring& selectedId) {
    TreeNode* target = model.FindNode(selectedId.empty() ? model.Root().id : selectedId);
    if (!target) {
        return &model.Root();
    }
    if (!target->IsFolder()) {
        target = model.FindParent(target->id);
    }
    return target ? target : &model.Root();
}

}  // namespace

void MainWindow::ImportConnections() {
    std::wstring path;
    if (!BrowseXmlFile(hwnd_, false, path)) {
        return;
    }

    TreeNode* target = ResolveImportTarget(Model(), GetSelectedNodeId());
    MRemoteNgImportStats stats{};
    std::wstring error;
    if (!MRemoteNgExchange::ImportFile(path, Model(), *target, stats, error)) {
        MessageBoxW(hwnd_, error.c_str(), L"Import failed", MB_ICONERROR);
        return;
    }

    RefreshTree();
    Save();

    std::wostringstream summary;
    summary << L"Imported " << stats.folders << L" folder(s) and " << stats.sessions << L" RDP session(s)";
    if (stats.skippedNonRdp > 0) {
        summary << L"\nSkipped " << stats.skippedNonRdp << L" non-RDP connection(s).";
    }
    summary << L"\n\nmRemoteNG passwords are encrypted and were not imported. "
               L"Assign passwords in Manage Credentials if needed.";
    MessageBoxW(hwnd_, summary.str().c_str(), L"Import complete", MB_ICONINFORMATION);
}

void MainWindow::ExportConnections() {
    TreeNode* selected = GetSelectedNode();
    const TreeNode& exportRoot = selected ? *selected : Model().Root();

    std::wstring path;
    if (!BrowseXmlFile(hwnd_, true, path)) {
        return;
    }

    std::wstring error;
    if (!MRemoteNgExchange::ExportFile(path, exportRoot, Model(), error)) {
        MessageBoxW(hwnd_, error.c_str(), L"Export failed", MB_ICONERROR);
        return;
    }

    MessageBoxW(hwnd_,
                L"Exported in mRemoteNG XML format.\nPasswords are omitted; usernames are included when a credential "
                L"profile is linked.",
                L"Export complete", MB_ICONINFORMATION);
}

void MainWindow::PasteToSelected() {
    auto pasted = ClipboardManager::PasteSubtree();
    if (!pasted) {
        return;
    }
    TreeNode* target = GetSelectedNode();
    if (!target || !target->IsFolder()) {
        target = Model().FindParent(GetSelectedNodeId());
    }
    if (!target) {
        target = &Model().Root();
    }
    Model().AssignNewIds(*pasted);
    Model().InsertSubtree(*target, std::move(pasted));
    RefreshTree();
    Save();
}

void MainWindow::ShowContextMenu(int screenX, int screenY) {
    POINT pt{screenX, screenY};
    ScreenToClient(tree_, &pt);
    TVHITTESTINFO ht{};
    ht.pt = pt;
    HTREEITEM hit = TreeView_HitTest(tree_, &ht);

    if (HandleAltClickRange(hit)) {
        return;
    }

    HMENU menu = CreatePopupMenu();

    if (hit) {
        bool alreadySelected = false;
        for (HTREEITEM item = nullptr;;) {
            item = TreeView_GetNextItem(tree_, item, TVGN_NEXTSELECTED);
            if (!item) {
                break;
            }
            if (item == hit) {
                alreadySelected = true;
                break;
            }
        }
        if (!alreadySelected) {
            TreeView_SelectItem(tree_, hit);
        }
    } else {
        TreeView_SelectItem(tree_, nullptr);
    }

    TreeNode* node = GetNodeForTreeItem(hit);
    const bool isSession = node && node->IsSession();
    const bool isFolder = node && node->IsFolder();
    const bool isRoot = node && node->id == Model().Root().id;
    const auto selectedSessions = GetSelectedSessionNodes();
    const bool canBulkCredential = selectedSessions.size() >= 2;

    std::vector<UiTheme::MenuItemData> items;
    std::vector<UiTheme::MenuItemData> seps;
    items.reserve(10);
    seps.reserve(6);
    UINT nextSepId = IDM_MENU_SEP_FIRST;
    auto addItem = [&](UINT id, const wchar_t* text) {
        items.push_back({});
        UiTheme::MenuInitItem(items.back(), text);
        UiTheme::MenuAppend(menu, id, items.back());
    };
    auto addSep = [&]() {
        if (nextSepId > IDM_MENU_SEP_LAST) {
            return;
        }
        seps.push_back({});
        UiTheme::MenuInitSeparator(seps.back());
        UiTheme::MenuAppendSeparator(menu, nextSepId++, seps.back());
    };

    if (isSession) {
        addItem(IDM_CONNECT, L"Connect");
        addSep();
    }
    if (canBulkCredential) {
        addItem(IDM_SET_CREDENTIAL, L"Set Credential...");
        addSep();
    }
    if (isFolder) {
        addItem(IDM_SET_CREDENTIAL_FOLDER, L"Set Credential...");
        addSep();
    }
    addItem(IDM_NEW_SESSION, L"New Session");
    addItem(IDM_NEW_FOLDER, L"New Folder");
    if (node && !isRoot) {
        addSep();
        addItem(IDM_EDIT, L"Edit");
        addItem(IDM_DUPLICATE, L"Duplicate");
        addItem(IDM_DELETE, L"Delete");
        addSep();
        addItem(IDM_COPY, L"Copy");
    }
    if (ClipboardManager::HasPasteData()) {
        addItem(IDM_PASTE, L"Paste");
    }

    UiTheme::ApplyMenuColors(menu);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, screenX, screenY, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::OnCommand(int id) {
    switch (id) {
        case IDM_CONNECT:
            ConnectSelected();
            break;
        case IDM_NEW_SESSION:
            NewSession();
            break;
        case IDM_NEW_FOLDER:
            NewFolder();
            break;
        case IDM_EDIT:
            EditSelected();
            break;
        case IDM_DELETE:
            DeleteSelected();
            break;
        case IDM_DUPLICATE:
            DuplicateSelected();
            break;
        case IDM_COPY:
            CopySelected();
            break;
        case IDM_PASTE:
            PasteToSelected();
            break;
        case IDM_MANAGE_CREDS:
            ShowCredentialManager(hwnd_, Model());
            break;
        case IDM_IMPORT:
            ImportConnections();
            break;
        case IDM_EXPORT:
            ExportConnections();
            break;
        case IDM_CHANGELOG:
            ShowChangelogDialog(hwnd_);
            break;
        case IDM_SET_CREDENTIAL:
            SetCredentialOnSelected();
            break;
        case IDM_SET_CREDENTIAL_FOLDER:
            SetCredentialOnFolder();
            break;
        case IDM_THEME_SYSTEM:
            SetThemePreference(UiTheme::ThemePreference::System);
            break;
        case IDM_THEME_LIGHT:
            SetThemePreference(UiTheme::ThemePreference::Light);
            break;
        case IDM_THEME_DARK:
            SetThemePreference(UiTheme::ThemePreference::Dark);
            break;
        case IDM_ABOUT:
            ShowAboutDialog(hwnd_);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd_);
            break;
    }
}

void MainWindow::OnNotify(LPNMHDR hdr) {
    if (hdr->hwndFrom != tree_) {
        return;
    }
    switch (hdr->code) {
        case TVN_SELCHANGED:
            EnforceSingleSelectionIfPlainClick(reinterpret_cast<LPNMTREEVIEW>(hdr));
            break;
        case TVN_BEGINDRAG: {
            auto* info = reinterpret_cast<LPNMTREEVIEW>(hdr);
            dragDrop_.BeginDrag(this, tree_, info->itemNew.hItem);
            break;
        }
        case TVN_ENDLABELEDIT: {
            auto* info = reinterpret_cast<LPNMTVDISPINFOW>(hdr);
            if (!info->item.pszText) {
                break;
            }
            TreeNode* node = Model().FindNode(GetSelectedNodeId());
            if (node && node->id != Model().Root().id) {
                node->name = info->item.pszText;
                RefreshTree();
                Save();
            }
            break;
        }
        case TVN_DELETEITEM: {
            auto* info = reinterpret_cast<LPNMTREEVIEW>(hdr);
            if (info->itemOld.lParam) {
                delete reinterpret_cast<std::wstring*>(info->itemOld.lParam);
            }
            break;
        }
        case NM_DBLCLK: {
            TreeNode* node = GetSelectedNode();
            if (node && node->IsSession()) {
                ConnectSelected();
            }
            break;
        }
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
        s_instance = self;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE:
            self->CreateControls();
            return 0;
        case WM_SIZE: {
            const int w = LOWORD(lParam);
            const int h = HIWORD(lParam);
            self->LayoutControls(w, h);
            if (UiTheme::IsDarkEffective()) {
                UiTheme::PaintMenuBarBand(hwnd);
            }
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == 0) {
                self->OnCommand(LOWORD(wParam));
            }
            return 0;
        case WM_ERASEBKGND: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, UiTheme::DialogBackgroundBrush());
            RECT treePad = rc;
            treePad.bottom = treePad.top + UiTheme::kMainWindowTreeTopPadding;
            if (treePad.bottom > rc.bottom) {
                treePad.bottom = rc.bottom;
            }
            FillRect(hdc, &treePad, UiTheme::InputBackgroundBrush());
            return 1;
        }
        case WM_PAINT: {
            const LRESULT result = DefWindowProcW(hwnd, msg, wParam, lParam);
            if (UiTheme::IsDarkEffective()) {
                UiTheme::PaintMenuBarBand(hwnd);
            }
            return result;
        }
        case WM_NCPAINT: {
            DefWindowProcW(hwnd, msg, wParam, lParam);
            UiTheme::PaintMenuBarBand(hwnd);
            return 0;
        }
        case WM_INITMENUPOPUP: {
            const HMENU popup = reinterpret_cast<HMENU>(wParam);
            self->UpdateMenuState(popup);
            if (popup == self->fileMenu_ || popup == self->themeMenu_) {
                self->UpdateThemeMenuChecks();
            }
            return 0;
        }
        case WM_SETTINGCHANGE:
            if (UiTheme::GetPreference() == UiTheme::ThemePreference::System) {
                UiTheme::RefreshPalette();
                self->ApplyTheme();
            }
            return 0;
        case WM_NOTIFY: {
            LPNMHDR hdr = reinterpret_cast<LPNMHDR>(lParam);
            if (hdr->hwndFrom == self->tree_ && hdr->code == TVN_SELCHANGING) {
                auto* info = reinterpret_cast<LPNMTREEVIEW>(hdr);
                if (self->HandleTreeSelChanging(info)) {
                    SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, 1);
                    return TRUE;
                }
            }
            self->OnNotify(hdr);
            return 0;
        }
        case WM_CONTEXTMENU: {
            POINT pt;
            if (lParam == static_cast<LPARAM>(-1)) {
                HTREEITEM sel = TreeView_GetSelection(self->tree_);
                RECT itemRect{};
                if (sel && TreeView_GetItemRect(self->tree_, sel, &itemRect, TRUE)) {
                    pt.x = itemRect.left;
                    pt.y = itemRect.bottom + 2;
                    ClientToScreen(self->tree_, &pt);
                } else {
                    GetCursorPos(&pt);
                }
            } else {
                pt.x = static_cast<short>(LOWORD(lParam));
                pt.y = static_cast<short>(HIWORD(lParam));
            }
            self->ShowContextMenu(pt.x, pt.y);
            return 0;
        }
        case WM_LBUTTONUP:
            if (self->dragDrop_.IsDragging()) {
                POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
                MapWindowPoints(self->hwnd_, self->tree_, &pt, 1);
                self->dragDrop_.OnLButtonUp(self->tree_, pt.x, pt.y);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (self->dragDrop_.IsDragging()) {
                POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
                MapWindowPoints(self->hwnd_, self->tree_, &pt, 1);
                self->dragDrop_.OnMouseMove(self->tree_, pt.x, pt.y);
            }
            return 0;
        case WM_MEASUREITEM:
            if (UiTheme::MenuOnMeasureItem(lParam)) {
                return TRUE;
            }
            break;
        case WM_DRAWITEM:
            if (UiTheme::MenuOnDrawItem(lParam)) {
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_F5) {
                self->RefreshTree();
            } else if (GetKeyState(VK_CONTROL) < 0 && wParam == 'C') {
                self->CopySelected();
            } else if (GetKeyState(VK_CONTROL) < 0 && wParam == 'V') {
                self->PasteToSelected();
            } else if (wParam == VK_RETURN) {
                self->ConnectSelected();
            } else if (wParam == VK_DELETE) {
                self->DeleteSelected();
            }
            return 0;
        case WM_CLOSE:
            self->Save();
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (self->imageList_) {
                ImageList_Destroy(self->imageList_);
            }
            UiTheme::ReleaseFont();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
