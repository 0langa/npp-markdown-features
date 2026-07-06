#include "ui/OutlinePanel.h"

#include "core/Strings.h"
#include "plugin/NppApi.h"

#include <commctrl.h>
#include <cwctype>
#include <vector>
#include <windowsx.h>

namespace nmf {
namespace {

constexpr wchar_t kPanelClass[] = L"NppMarkdownFeaturesOutlinePanel";
constexpr wchar_t kPanelTitle[] = L"Markdown Outline";
constexpr wchar_t kModuleName[] = L"NppMarkdownFeatures.dll";
constexpr UINT_PTR kFilterId = 100;
constexpr UINT kCueBannerMessage = 0x1501;  // EM_SETCUEBANNER

void RegisterPanelClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OutlinePanel::WindowProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kPanelClass;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    ::RegisterClassEx(&wc);
    registered = true;
}

HFONT CreateMessageFont() {
    NONCLIENTMETRICS metrics{};
    metrics.cbSize = sizeof(metrics);
    if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        return ::CreateFontIndirect(&metrics.lfMessageFont);
    }
    return nullptr;
}

std::wstring ToLowerCopy(std::wstring value) {
    for (auto& ch : value) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return value;
}

}  // namespace

OutlinePanel::~OutlinePanel() {
    Destroy();
}

void OutlinePanel::Initialize(HWND nppHandle, int commandIndex) {
    npp_ = nppHandle;
    commandIndex_ = commandIndex;
}

bool OutlinePanel::IsCreated() const {
    return panel_ != nullptr;
}

bool OutlinePanel::IsVisible() const {
    return visible_;
}

void OutlinePanel::SetJumpCallback(JumpCallback callback) {
    jump_ = std::move(callback);
}

void OutlinePanel::SetVisibilityCallback(VisibilityCallback callback) {
    visibility_ = std::move(callback);
}

bool OutlinePanel::EnsureCreated() {
    if (panel_ != nullptr) {
        return true;
    }
    if (npp_ == nullptr) {
        return false;
    }
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_TREEVIEW_CLASSES};
    ::InitCommonControlsEx(&icc);
    RegisterPanelClass();
    panel_ = ::CreateWindowEx(
        0,
        kPanelClass,
        kPanelTitle,
        WS_CHILD | WS_CLIPCHILDREN,
        0,
        0,
        260,
        480,
        npp_,
        nullptr,
        ::GetModuleHandle(nullptr),
        this);
    return panel_ != nullptr;
}

void OutlinePanel::RegisterWithDockingManager() {
    if (registered_ || panel_ == nullptr) {
        return;
    }
    npp::DockingData data{};
    data.hClient = panel_;
    data.pszName = kPanelTitle;
    data.dlgID = commandIndex_;
    data.uMask = npp::DWS_DF_CONT_RIGHT;
    data.pszModuleName = kModuleName;
    ::SendMessage(npp_, npp::NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&data));
    registered_ = true;
    // Theme the panel's child controls to match the current Notepad++ mode.
    ::SendMessage(npp_, npp::NPPM_DARKMODESUBCLASSANDTHEME, npp::kDarkModeSubclassInit, reinterpret_cast<LPARAM>(panel_));
}

void OutlinePanel::Show() {
    if (!EnsureCreated()) {
        return;
    }
    RegisterWithDockingManager();
    ::SendMessage(npp_, npp::NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(panel_));
    visible_ = true;
    if (visibility_) {
        visibility_(true);
    }
}

void OutlinePanel::Hide() {
    if (panel_ == nullptr || !registered_) {
        return;
    }
    ::SendMessage(npp_, npp::NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(panel_));
    visible_ = false;
    if (visibility_) {
        visibility_(false);
    }
}

void OutlinePanel::SetOutline(const MarkdownOutline& outline, const std::wstring& documentName, bool isMarkdown) {
    if (panel_ == nullptr) {
        return;
    }
    const bool sameContent = isMarkdown_ == isMarkdown && documentName_ == documentName && outline_.Headings() == outline.Headings();
    if (sameContent) {
        return;
    }
    outline_ = outline;
    documentName_ = documentName;
    isMarkdown_ = isMarkdown;
    RebuildTree();
}

void OutlinePanel::HandleDarkModeChanged() {
    if (panel_ != nullptr && registered_) {
        ::SendMessage(npp_, npp::NPPM_DARKMODESUBCLASSANDTHEME, npp::kDarkModeSubclassHandleChange, reinterpret_cast<LPARAM>(panel_));
    }
}

void OutlinePanel::Destroy() {
    if (panel_ != nullptr) {
        ::DestroyWindow(panel_);
        panel_ = nullptr;
        filter_ = nullptr;
        tree_ = nullptr;
    }
    if (font_ != nullptr) {
        ::DeleteObject(font_);
        font_ = nullptr;
    }
    registered_ = false;
    visible_ = false;
}

void OutlinePanel::Layout() {
    if (panel_ == nullptr || filter_ == nullptr || tree_ == nullptr) {
        return;
    }
    RECT rect{};
    ::GetClientRect(panel_, &rect);
    UINT dpi = ::GetDpiForWindow(panel_);
    if (dpi == 0) {
        dpi = 96;
    }
    const int margin = ::MulDiv(4, dpi, 96);
    const int filterHeight = ::MulDiv(22, dpi, 96);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    ::MoveWindow(filter_, margin, margin, width - 2 * margin, filterHeight, TRUE);
    ::MoveWindow(tree_, margin, margin * 2 + filterHeight, width - 2 * margin, height - filterHeight - 3 * margin, TRUE);
}

void OutlinePanel::RebuildTree() {
    if (tree_ == nullptr) {
        return;
    }
    ::SendMessage(tree_, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(tree_);

    const auto filterLower = ToLowerCopy(filterText_);
    std::vector<HTREEITEM> parents;  // parents[i] = last item inserted at heading level i+1
    std::vector<HTREEITEM> allItems;

    if (!isMarkdown_) {
        TVINSERTSTRUCT insert{};
        insert.hParent = TVI_ROOT;
        insert.hInsertAfter = TVI_LAST;
        insert.item.mask = TVIF_TEXT | TVIF_PARAM;
        wchar_t info[] = L"No Markdown document active";
        insert.item.pszText = info;
        insert.item.lParam = -1;
        TreeView_InsertItem(tree_, &insert);
    } else {
        for (const auto& heading : outline_.Headings()) {
            auto text = Utf8ToWide(heading.text);
            if (!filterLower.empty() && ToLowerCopy(text).find(filterLower) == std::wstring::npos) {
                continue;
            }
            HTREEITEM parent = TVI_ROOT;
            if (filterLower.empty()) {
                // Nest under the closest previous heading with a smaller level.
                for (int level = heading.level - 1; level >= 1; --level) {
                    if (level <= static_cast<int>(parents.size()) && parents[level - 1] != nullptr) {
                        parent = parents[level - 1];
                        break;
                    }
                }
            }
            TVINSERTSTRUCT insert{};
            insert.hParent = parent;
            insert.hInsertAfter = TVI_LAST;
            insert.item.mask = TVIF_TEXT | TVIF_PARAM;
            insert.item.pszText = text.data();
            insert.item.lParam = heading.line;
            HTREEITEM item = TreeView_InsertItem(tree_, &insert);
            allItems.push_back(item);
            if (filterLower.empty()) {
                if (static_cast<int>(parents.size()) < heading.level) {
                    parents.resize(heading.level, nullptr);
                }
                parents[heading.level - 1] = item;
                // Deeper levels restart under this heading.
                for (size_t level = heading.level; level < parents.size(); ++level) {
                    parents[level] = nullptr;
                }
            }
        }
        if (allItems.empty() && filterLower.empty()) {
            TVINSERTSTRUCT insert{};
            insert.hParent = TVI_ROOT;
            insert.hInsertAfter = TVI_LAST;
            insert.item.mask = TVIF_TEXT | TVIF_PARAM;
            wchar_t info[] = L"No headings found";
            insert.item.pszText = info;
            insert.item.lParam = -1;
            TreeView_InsertItem(tree_, &insert);
        }
    }

    for (HTREEITEM item : allItems) {
        TreeView_Expand(tree_, item, TVE_EXPAND);
    }
    ::SendMessage(tree_, WM_SETREDRAW, TRUE, 0);
    ::InvalidateRect(tree_, nullptr, TRUE);
}

void OutlinePanel::JumpToTreeItem(HTREEITEM item) {
    if (item == nullptr || tree_ == nullptr) {
        return;
    }
    TVITEM tvItem{};
    tvItem.mask = TVIF_PARAM;
    tvItem.hItem = item;
    if (!TreeView_GetItem(tree_, &tvItem)) {
        return;
    }
    const auto line = static_cast<int>(tvItem.lParam);
    if (line >= 0 && jump_) {
        jump_(line);
    }
}

void OutlinePanel::OnFilterChanged() {
    if (filter_ == nullptr) {
        return;
    }
    const int length = ::GetWindowTextLength(filter_);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    ::GetWindowText(filter_, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    if (text == filterText_) {
        return;
    }
    filterText_ = std::move(text);
    RebuildTree();
}

LRESULT OutlinePanel::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            font_ = CreateMessageFont();
            filter_ = ::CreateWindowEx(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0, 0, 100, 22,
                hwnd,
                reinterpret_cast<HMENU>(kFilterId),
                ::GetModuleHandle(nullptr),
                nullptr);
            tree_ = ::CreateWindowEx(
                0,
                WC_TREEVIEW,
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT,
                0, 24, 100, 200,
                hwnd,
                nullptr,
                ::GetModuleHandle(nullptr),
                nullptr);
            if (font_ != nullptr) {
                ::SendMessage(filter_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
                ::SendMessage(tree_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
            }
            ::SendMessage(filter_, kCueBannerMessage, TRUE, reinterpret_cast<LPARAM>(L"Filter headings"));
            return 0;
        }
        case WM_SIZE:
            Layout();
            return 0;
        case WM_COMMAND:
            if (reinterpret_cast<HWND>(lParam) == filter_ && HIWORD(wParam) == EN_CHANGE) {
                OnFilterChanged();
                return 0;
            }
            break;
        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (header == nullptr) {
                break;
            }
            if (header->hwndFrom == tree_) {
                if (header->code == NM_CLICK) {
                    TVHITTESTINFO hit{};
                    ::GetCursorPos(&hit.pt);
                    ::ScreenToClient(tree_, &hit.pt);
                    if (HTREEITEM item = TreeView_HitTest(tree_, &hit)) {
                        TreeView_SelectItem(tree_, item);
                        JumpToTreeItem(item);
                    }
                    return 0;
                }
                if (header->code == NM_RETURN) {
                    JumpToTreeItem(TreeView_GetSelection(tree_));
                    return 0;
                }
            } else if (header->code == npp::DMN_CLOSE) {
                visible_ = false;
                if (visibility_) {
                    visibility_(false);
                }
                return 0;
            }
            break;
        }
        default:
            break;
    }
    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK OutlinePanel::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<OutlinePanel*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        return self->HandleMessage(hwnd, message, wParam, lParam);
    }
    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

}  // namespace nmf
