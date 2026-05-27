#include "DragDrop.h"

#include "MainWindow.h"

#include <commctrl.h>

std::wstring DragDropController::ResolveDropParentId(HWND tree, int x, int y) const {
    if (!owner_ || !tree) {
        return L"";
    }

    TVHITTESTINFO ht{};
    ht.pt.x = x;
    ht.pt.y = y;
    HTREEITEM targetItem = TreeView_HitTest(tree, &ht);

    std::wstring parentId = owner_->Model().Root().id;
    if (!targetItem) {
        return parentId;
    }

    TVITEMW tv{};
    tv.mask = TVIF_PARAM;
    tv.hItem = targetItem;
    if (!TreeView_GetItem(tree, &tv) || !tv.lParam) {
        return parentId;
    }

    const std::wstring targetNodeId = *reinterpret_cast<std::wstring*>(tv.lParam);
    TreeNode* targetNode = owner_->Model().FindNode(targetNodeId);
    if (!targetNode) {
        return parentId;
    }
    if (targetNode->IsFolder()) {
        return targetNodeId;
    }
    if (TreeNode* parent = owner_->Model().FindParent(targetNodeId)) {
        return parent->id;
    }
    return parentId;
}

bool DragDropController::IsDropAllowed(const std::wstring& parentId) const {
    if (!owner_) {
        return false;
    }
    const std::wstring& rootId = owner_->Model().Root().id;
    if (dragNodeId_.empty() || parentId.empty()) {
        return false;
    }
    // Disallow dragging the root itself.
    // Dropping onto root is allowed for non-root items.
    if (dragNodeId_ == rootId) {
        return false;
    }
    return dragNodeId_ != parentId;
}

void DragDropController::UpdateDropTargetHighlight(const std::wstring& parentId) {
    if (!owner_ || !tree_) {
        return;
    }
    if (highlightedParentId_ == parentId) {
        return;
    }

    HTREEITEM highlightItem = nullptr;
    if (!parentId.empty()) {
        highlightItem = owner_->GetItemForId(parentId);
    }
    TreeView_SelectDropTarget(tree_, highlightItem);
    highlightedParentId_ = parentId;
}

void DragDropController::ClearDropTargetHighlight() {
    if (tree_) {
        TreeView_SelectDropTarget(tree_, nullptr);
    }
    highlightedParentId_.clear();
}

void DragDropController::BeginDrag(MainWindow* owner, HWND tree, HTREEITEM item) {
    owner_ = owner;
    tree_ = tree;
    dragItem_ = item;
    dragNodeId_.clear();
    highlightedParentId_.clear();

    TVITEMW tv{};
    tv.mask = TVIF_PARAM;
    tv.hItem = item;
    if (TreeView_GetItem(tree, &tv) && tv.lParam) {
        dragNodeId_ = *reinterpret_cast<std::wstring*>(tv.lParam);
    }
    dragging_ = !dragNodeId_.empty();
    if (dragging_ && owner_ && dragNodeId_ == owner_->Model().Root().id) {
        dragging_ = false;
    }
    if (dragging_) {
        SetCapture(owner->Hwnd());
    }
}

void DragDropController::OnMouseMove(HWND tree, int x, int y) {
    if (!dragging_ || !owner_) {
        return;
    }
    const std::wstring parentId = ResolveDropParentId(tree, x, y);
    if (IsDropAllowed(parentId)) {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        UpdateDropTargetHighlight(parentId);
    } else {
        SetCursor(LoadCursorW(nullptr, IDC_NO));
        ClearDropTargetHighlight();
    }
}

void DragDropController::OnLButtonUp(HWND tree, int x, int y) {
    if (!dragging_ || !owner_) {
        return;
    }
    ReleaseCapture();
    dragging_ = false;
    ClearDropTargetHighlight();
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));

    const std::wstring parentId = ResolveDropParentId(tree, x, y);

    if (IsDropAllowed(parentId)) {
        owner_->ReparentNode(dragNodeId_, parentId);
    }

    dragNodeId_.clear();
    dragItem_ = nullptr;
    owner_ = nullptr;
    tree_ = nullptr;
}

void DragDropController::Cancel() {
    if (dragging_) {
        ReleaseCapture();
    }
    ClearDropTargetHighlight();
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    dragging_ = false;
    dragNodeId_.clear();
    dragItem_ = nullptr;
    owner_ = nullptr;
    tree_ = nullptr;
}
