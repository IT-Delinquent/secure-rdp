#include "DragDrop.h"

#include "MainWindow.h"

#include <commctrl.h>

void DragDropController::BeginDrag(MainWindow* owner, HWND tree, HTREEITEM item) {
    owner_ = owner;
    tree_ = tree;
    dragItem_ = item;
    dragNodeId_.clear();

    TVITEMW tv{};
    tv.mask = TVIF_PARAM;
    tv.hItem = item;
    if (TreeView_GetItem(tree, &tv) && tv.lParam) {
        dragNodeId_ = *reinterpret_cast<std::wstring*>(tv.lParam);
    }
    dragging_ = !dragNodeId_.empty();
    if (dragging_) {
        SetCapture(owner->Hwnd());
    }
}

void DragDropController::OnMouseMove(HWND tree, int x, int y) {
    (void)tree;
    (void)x;
    (void)y;
}

void DragDropController::OnLButtonUp(HWND tree, int x, int y) {
    if (!dragging_ || !owner_) {
        return;
    }
    ReleaseCapture();
    dragging_ = false;

    TVHITTESTINFO ht{};
    ht.pt.x = x;
    ht.pt.y = y;
    HTREEITEM targetItem = TreeView_HitTest(tree, &ht);

    std::wstring parentId = owner_->Model().Root().id;
    if (targetItem) {
        TVITEMW tv{};
        tv.mask = TVIF_PARAM;
        tv.hItem = targetItem;
        if (TreeView_GetItem(tree, &tv) && tv.lParam) {
            const std::wstring targetNodeId = *reinterpret_cast<std::wstring*>(tv.lParam);
            TreeNode* targetNode = owner_->Model().FindNode(targetNodeId);
            if (targetNode) {
                if (targetNode->IsFolder()) {
                    parentId = targetNodeId;
                } else if (TreeNode* parent = owner_->Model().FindParent(targetNodeId)) {
                    parentId = parent->id;
                }
            }
        }
    }

    if (!dragNodeId_.empty() && dragNodeId_ != parentId) {
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
    dragging_ = false;
    dragNodeId_.clear();
    dragItem_ = nullptr;
    owner_ = nullptr;
    tree_ = nullptr;
}
