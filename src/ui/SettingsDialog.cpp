#include "ui/SettingsDialog.h"

#include "core/Strings.h"

#include <commctrl.h>

namespace nmf {
namespace {

constexpr int kWidth = 440;
constexpr int kHeight = 320;
constexpr int kDefaultModeId = 1001;
constexpr int kExtensionsId = 1002;
constexpr int kThemeId = 1003;
constexpr int kCustomCssId = 1004;
constexpr int kOkId = IDOK;
constexpr int kCancelId = IDCANCEL;
constexpr wchar_t kSettingsClass[] = L"NppMarkdownFeaturesSettingsDialog";

struct DialogState {
    MarkdownViewSettings* settings{};
    std::filesystem::path settingsPath;
    bool accepted{false};
};

HWND AddLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height) {
    return ::CreateWindowEx(0, WC_STATIC, text, WS_CHILD | WS_VISIBLE, x, y, width, height, parent, nullptr, ::GetModuleHandle(nullptr), nullptr);
}

HWND AddButton(HWND parent, const wchar_t* text, int id, int x, int y, int width, int height) {
    return ::CreateWindowEx(0, WC_BUTTON, text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), ::GetModuleHandle(nullptr), nullptr);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* state = reinterpret_cast<DialogState*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
            state = static_cast<DialogState*>(create->lpCreateParams);
            AddLabel(hwnd, L"Default Markdown display", 18, 20, 180, 22);
            HWND combo = ::CreateWindowEx(
                0,
                WC_COMBOBOX,
                L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                210,
                18,
                190,
                120,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDefaultModeId)),
                ::GetModuleHandle(nullptr),
                nullptr);
            ::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"raw"));
            ::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"rendered"));
            ::SendMessage(combo, CB_SETCURSEL, state->settings->defaultMode == DisplayMode::Rendered ? 1 : 0, 0);

            AddLabel(hwnd, L"Markdown extensions", 18, 64, 180, 22);
            HWND edit = ::CreateWindowEx(
                WS_EX_CLIENTEDGE,
                WC_EDIT,
                JoinExtensions(state->settings->extensions).c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                210,
                62,
                190,
                24,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kExtensionsId)),
                ::GetModuleHandle(nullptr),
                nullptr);
            ::SendMessage(edit, EM_SETLIMITTEXT, 512, 0);

            AddLabel(hwnd, L"Preview theme", 18, 108, 180, 22);
            HWND theme = ::CreateWindowEx(
                0,
                WC_COMBOBOX,
                L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                210,
                106,
                190,
                120,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kThemeId)),
                ::GetModuleHandle(nullptr),
                nullptr);
            ::SendMessage(theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"auto (follow system)"));
            ::SendMessage(theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"light"));
            ::SendMessage(theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"dark"));
            int themeIndex = 0;
            if (state->settings->theme == L"light") {
                themeIndex = 1;
            } else if (state->settings->theme == L"dark") {
                themeIndex = 2;
            }
            ::SendMessage(theme, CB_SETCURSEL, themeIndex, 0);

            AddLabel(hwnd, L"Custom CSS file (optional)", 18, 152, 180, 22);
            HWND customCss = ::CreateWindowEx(
                WS_EX_CLIENTEDGE,
                WC_EDIT,
                state->settings->customCssPath.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                210,
                150,
                190,
                24,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCustomCssId)),
                ::GetModuleHandle(nullptr),
                nullptr);
            ::SendMessage(customCss, EM_SETLIMITTEXT, 1024, 0);

            AddLabel(hwnd, L"Toggle scope: global · Refresh: on save", 18, 196, 390, 22);
            AddLabel(hwnd, state->settingsPath.wstring().c_str(), 18, 222, 390, 28);
            AddButton(hwnd, L"OK", kOkId, 235, 256, 78, 28);
            AddButton(hwnd, L"Cancel", kCancelId, 322, 256, 78, 28);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == kOkId && state != nullptr) {
                HWND combo = ::GetDlgItem(hwnd, kDefaultModeId);
                const auto selection = ::SendMessage(combo, CB_GETCURSEL, 0, 0);
                state->settings->defaultMode = selection == 1 ? DisplayMode::Rendered : DisplayMode::Raw;

                std::wstring extensions(512, L'\0');
                const int length = ::GetWindowText(::GetDlgItem(hwnd, kExtensionsId), extensions.data(), static_cast<int>(extensions.size()));
                extensions.resize(static_cast<size_t>(length));
                state->settings->extensions = SplitExtensions(extensions);

                const auto themeSelection = ::SendMessage(::GetDlgItem(hwnd, kThemeId), CB_GETCURSEL, 0, 0);
                state->settings->theme = themeSelection == 1 ? L"light" : themeSelection == 2 ? L"dark" : L"auto";

                std::wstring customCss(1024, L'\0');
                const int cssLength = ::GetWindowText(::GetDlgItem(hwnd, kCustomCssId), customCss.data(), static_cast<int>(customCss.size()));
                customCss.resize(static_cast<size_t>(cssLength));
                state->settings->customCssPath = Trim(customCss);

                state->settings->toggleScope = L"global";
                state->settings->refreshMode = L"onSave";
                state->accepted = true;
                ::DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == kCancelId) {
                ::DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            ::DestroyWindow(hwnd);
            return 0;
        default:
            break;
    }
    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

void RegisterSettingsClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kSettingsClass;
    ::RegisterClassEx(&wc);
    registered = true;
}

}  // namespace

bool ShowSettingsDialog(HWND owner, MarkdownViewSettings& settings, const std::filesystem::path& settingsPath) {
    RegisterSettingsClass();
    DialogState state{&settings, settingsPath, false};
    RECT ownerRect{};
    ::GetWindowRect(owner, &ownerRect);
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - kWidth) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - kHeight) / 2;
    HWND dialog = ::CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        kSettingsClass,
        L"Markdown Features Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x,
        y,
        kWidth,
        kHeight,
        owner,
        nullptr,
        ::GetModuleHandle(nullptr),
        &state);
    if (dialog == nullptr) {
        return false;
    }

    ::EnableWindow(owner, FALSE);
    ::ShowWindow(dialog, SW_SHOW);
    MSG msg{};
    while (::IsWindow(dialog) && ::GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (!::IsDialogMessage(dialog, &msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }
    ::EnableWindow(owner, TRUE);
    ::SetActiveWindow(owner);
    return state.accepted;
}

}  // namespace nmf
