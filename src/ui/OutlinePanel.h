#pragma once

#include "core/MarkdownOutline.h"

#include <functional>
#include <string>
#include <windows.h>

#include <commctrl.h>

namespace nmf {

// Docked "Markdown Outline" panel: filter box above a heading tree.
// Registered with the Notepad++ docking manager on first Show().
class OutlinePanel {
public:
    using JumpCallback = std::function<void(int docLine)>;
    using VisibilityCallback = std::function<void(bool visible)>;

    OutlinePanel() = default;
    ~OutlinePanel();

    OutlinePanel(const OutlinePanel&) = delete;
    OutlinePanel& operator=(const OutlinePanel&) = delete;

    void Initialize(HWND nppHandle, int commandIndex);
    bool IsCreated() const;
    bool IsVisible() const;
    void Show();
    void Hide();
    void SetOutline(const MarkdownOutline& outline, const std::wstring& documentName, bool isMarkdown);
    void SetJumpCallback(JumpCallback callback);
    void SetVisibilityCallback(VisibilityCallback callback);
    void HandleDarkModeChanged();
    void Destroy();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    bool EnsureCreated();
    void RegisterWithDockingManager();
    void Layout();
    void RebuildTree();
    void JumpToTreeItem(HTREEITEM item);
    void OnFilterChanged();
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND npp_{};
    HWND panel_{};
    HWND filter_{};
    HWND tree_{};
    HFONT font_{};
    int commandIndex_{-1};
    bool registered_{false};
    bool visible_{false};
    MarkdownOutline outline_{};
    bool isMarkdown_{false};
    std::wstring documentName_{};
    std::wstring filterText_{};
    JumpCallback jump_{};
    VisibilityCallback visibility_{};
};

}  // namespace nmf
