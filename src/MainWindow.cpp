#include "MainWindow.h"

#include "App.h"
#include "Clipboard.h"
#include "Dialogs/CredentialDialog.h"
#include "Dialogs/FolderDialog.h"
#include "Dialogs/SessionDialog.h"
#include "Logger.h"
#include "MRemoteNgExchange.h"
#include "Resource.h"
#include "UiTheme.h"
#include "Version.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <sstream>
#include <vector>
#include <windows.h>

namespace {

struct MainMenuItems {
    UiTheme::MenuItemData fileLabel{};
    UiTheme::MenuItemData editLabel{};
    UiTheme::MenuItemData connect{};
    UiTheme::MenuItemData newSession{};
    UiTheme::MenuItemData newFolder{};
    UiTheme::MenuItemData manageCreds{};
    UiTheme::MenuItemData import{};
    UiTheme::MenuItemData exportItem{};
    UiTheme::MenuItemData exit{};
    UiTheme::MenuItemData edit{};
    UiTheme::MenuItemData del{};
    UiTheme::MenuItemData duplicate{};
    UiTheme::MenuItemData copy{};
    UiTheme::MenuItemData paste{};

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
        UiTheme::MenuInitItem(exit, L"E&xit");
        UiTheme::MenuInitItem(edit, L"&Edit");
        UiTheme::MenuInitItem(del, L"&Delete");
        UiTheme::MenuInitItem(duplicate, L"Dupli&cate");
        UiTheme::MenuInitItem(copy, L"&Copy\tCtrl+C");
        UiTheme::MenuInitItem(paste, L"&Paste\tCtrl+V");
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
    wc.lpszClassName = L"SecureRdpMainWindow";
    wc.hIconSm = wc.hIcon;
    return RegisterClassExW(&wc) != 0;
}

bool MainWindow::CreateWindowInstance(HINSTANCE hInstance) {
    hwnd_ = CreateWindowExW(0, L"SecureRdpMainWindow", SECURE_RDP_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
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
    UiTheme::ApplyTree(tree_);

    status_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
                              WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hwnd_,
                              reinterpret_cast<HMENU>(IDC_STATUS), hInstance_, nullptr);

    int parts[] = {-1, 120};
    SendMessageW(status_, SB_SETPARTS, 2, reinterpret_cast<LPARAM>(parts));
    SetStatusText(L"Ready");
    SendMessageW(status_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(SECURE_RDP_VERSION));

    UiTheme::Apply(hwnd_);
}

void MainWindow::CreateMenus() {
    MainMenuItems& m = MainMenu();
    HMENU menuBar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    HMENU editMenu = CreatePopupMenu();

    UiTheme::MenuAppend(fileMenu, IDM_CONNECT, m.connect);
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    UiTheme::MenuAppend(fileMenu, IDM_NEW_SESSION, m.newSession);
    UiTheme::MenuAppend(fileMenu, IDM_NEW_FOLDER, m.newFolder);
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    UiTheme::MenuAppend(fileMenu, IDM_MANAGE_CREDS, m.manageCreds);
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    UiTheme::MenuAppend(fileMenu, IDM_IMPORT, m.import);
    UiTheme::MenuAppend(fileMenu, IDM_EXPORT, m.exportItem);
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    UiTheme::MenuAppend(fileMenu, IDM_EXIT, m.exit);

    UiTheme::MenuAppend(editMenu, IDM_EDIT, m.edit);
    UiTheme::MenuAppend(editMenu, IDM_DELETE, m.del);
    UiTheme::MenuAppend(editMenu, IDM_DUPLICATE, m.duplicate);
    AppendMenuW(editMenu, MF_SEPARATOR, 0, nullptr);
    UiTheme::MenuAppend(editMenu, IDM_COPY, m.copy);
    UiTheme::MenuAppend(editMenu, IDM_PASTE, m.paste);

    UiTheme::MenuAppend(menuBar, reinterpret_cast<UINT_PTR>(fileMenu), m.fileLabel, MF_POPUP);
    UiTheme::MenuAppend(menuBar, reinterpret_cast<UINT_PTR>(editMenu), m.editLabel, MF_POPUP);
    SetMenu(hwnd_, menuBar);
}

void MainWindow::SetStatusText(const std::wstring& text) {
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
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
    const int pad = 12;
    RECT statusRect{};
    SendMessageW(status_, WM_SIZE, 0, 0);
    GetWindowRect(status_, &statusRect);
    const int statusHeight = statusRect.bottom - statusRect.top;

    const int treeX = pad;
    const int treeY = pad;
    const int treeW = width - pad * 2;
    const int treeH = height - pad * 2 - statusHeight;
    SetWindowPos(tree_, nullptr, treeX, treeY, treeW > 0 ? treeW : 0, treeH > 0 ? treeH : 0, SWP_NOZORDER);
    SetWindowPos(status_, nullptr, 0, height - statusHeight, width, statusHeight, SWP_NOZORDER);
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
        SetStatusText(L"Save error: " + error);
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
    SetStatusText(L"Launched: " + node->name);
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
    SetStatusText(L"Folder created: " + dlg.name);
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
    SetStatusText(L"Imported from " + path);
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
    SetStatusText(L"Exported to " + path);
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

void MainWindow::ShowContextMenu(int x, int y) {
    HMENU menu = CreatePopupMenu();
    TreeNode* node = GetSelectedNode();
    const bool isSession = node && node->IsSession();
    const bool isRoot = node && node->id == Model().Root().id;

    std::vector<UiTheme::MenuItemData> items;
    items.reserve(10);
    auto addItem = [&](UINT id, const wchar_t* text) {
        items.push_back({});
        UiTheme::MenuInitItem(items.back(), text);
        UiTheme::MenuAppend(menu, id, items.back());
    };

    if (isSession) {
        addItem(IDM_CONNECT, L"Connect");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    addItem(IDM_NEW_SESSION, L"New Session");
    addItem(IDM_NEW_FOLDER, L"New Folder");
    if (node && !isRoot) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        addItem(IDM_EDIT, L"Edit");
        addItem(IDM_DUPLICATE, L"Duplicate");
        addItem(IDM_DELETE, L"Delete");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        addItem(IDM_COPY, L"Copy");
    }
    if (ClipboardManager::HasPasteData()) {
        addItem(IDM_PASTE, L"Paste");
    }

    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, x, y, 0, hwnd_, nullptr);
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
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == 0) {
                self->OnCommand(LOWORD(wParam));
            }
            return 0;
        case WM_NOTIFY:
            self->OnNotify(reinterpret_cast<LPNMHDR>(lParam));
            return 0;
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
