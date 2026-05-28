#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <string>
#include <windows.h>
#include <commctrl.h>

class MainWindow;

class DragDropController {
public:
    void BeginDrag(MainWindow* owner, HWND tree, HTREEITEM item);
    void OnMouseMove(HWND tree, int x, int y);
    void OnLButtonUp(HWND tree, int x, int y);
    void Cancel();
    bool IsDragging() const { return dragging_; }

private:
    std::wstring ResolveDropParentId(HWND tree, int x, int y) const;
    bool IsDropAllowed(const std::wstring& parentId) const;
    void UpdateDropTargetHighlight(const std::wstring& parentId);
    void ClearDropTargetHighlight();

    MainWindow* owner_ = nullptr;
    HWND tree_ = nullptr;
    HTREEITEM dragItem_ = nullptr;
    std::wstring dragNodeId_;
    std::wstring highlightedParentId_;
    bool dragging_ = false;
    HIMAGELIST dragImage_ = nullptr;
};
