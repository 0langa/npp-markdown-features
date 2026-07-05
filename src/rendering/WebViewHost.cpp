#include "rendering/WebViewHost.h"

#include "core/Strings.h"

#include <shellapi.h>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace nmf {
namespace {

constexpr wchar_t kHostWindowClass[] = L"NppMarkdownFeaturesWebViewHost";

std::wstring Utf8HtmlToWide(const std::string& html) {
    return Utf8ToWide(html);
}

bool IsExternalUri(const std::wstring& uri) {
    const auto lower = ToLower(uri);
    return lower.rfind(L"http://", 0) == 0 || lower.rfind(L"https://", 0) == 0 || lower.rfind(L"mailto:", 0) == 0;
}

void RegisterHostWindowClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WebViewHost::WindowProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kHostWindowClass;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    ::RegisterClassEx(&wc);
    registered = true;
}

}  // namespace

WebViewHost::WebViewHost() = default;

WebViewHost::~WebViewHost() {
    Destroy();
}

bool WebViewHost::ShowHtml(HWND scintillaParent, const std::string& html, const std::wstring& sourcePath) {
    parentScintilla_ = scintillaParent;
    sourcePath_ = sourcePath;
    pendingHtml_ = Utf8HtmlToWide(html);
    if (!EnsureHostWindow(scintillaParent)) {
        return false;
    }
    ::ShowWindow(hostWindow_, SW_SHOW);
    ::SetTimer(hostWindow_, 1, 250, nullptr);
    Resize();
    if (!EnsureWebView()) {
        return false;
    }
    NavigatePendingHtml();
    return true;
}

void WebViewHost::Hide() {
    if (hostWindow_ != nullptr) {
        ::KillTimer(hostWindow_, 1);
        ::ShowWindow(hostWindow_, SW_HIDE);
    }
}

void WebViewHost::Resize() {
    if (hostWindow_ == nullptr || parentScintilla_ == nullptr) {
        return;
    }
    RECT rect{};
    ::GetClientRect(parentScintilla_, &rect);
    ::SetWindowPos(hostWindow_, HWND_TOP, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
    if (controller_) {
        controller_->put_Bounds(rect);
    }
}

void WebViewHost::Destroy() {
    if (controller_) {
        controller_->Close();
    }
    webView_.Reset();
    controller_.Reset();
    environment_.Reset();
    if (hostWindow_ != nullptr) {
        ::KillTimer(hostWindow_, 1);
        ::DestroyWindow(hostWindow_);
        hostWindow_ = nullptr;
    }
}

bool WebViewHost::EnsureHostWindow(HWND scintillaParent) {
    if (scintillaParent == nullptr) {
        return false;
    }
    RegisterHostWindowClass();
    if (hostWindow_ != nullptr) {
        if (::GetParent(hostWindow_) != scintillaParent) {
            ::SetParent(hostWindow_, scintillaParent);
        }
        return true;
    }
    hostWindow_ = ::CreateWindowEx(
        0,
        kHostWindowClass,
        L"",
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        0,
        0,
        scintillaParent,
        nullptr,
        ::GetModuleHandle(nullptr),
        this);
    return hostWindow_ != nullptr;
}

bool WebViewHost::EnsureWebView() {
    if (webView_) {
        return true;
    }
    if (webViewCreating_) {
        return true;
    }
    webViewCreating_ = true;
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        nullptr,
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                webViewCreating_ = false;
                if (FAILED(result) || environment == nullptr || hostWindow_ == nullptr) {
                    SetRuntimeMissingErrorShown();
                    return S_OK;
                }
                environment_ = environment;
                environment_->CreateCoreWebView2Controller(
                    hostWindow_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || controller == nullptr) {
                                SetRuntimeMissingErrorShown();
                                return S_OK;
                            }
                            controller_ = controller;
                            controller_->get_CoreWebView2(&webView_);
                            if (webView_) {
                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(webView_->get_Settings(&settings)) && settings) {
                                    settings->put_IsScriptEnabled(FALSE);
                                    settings->put_AreDefaultContextMenusEnabled(TRUE);
                                    settings->put_AreDevToolsEnabled(TRUE);
                                }
                                EventRegistrationToken token{};
                                webView_->add_NavigationStarting(
                                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                            LPWSTR uriRaw = nullptr;
                                            if (SUCCEEDED(args->get_Uri(&uriRaw)) && uriRaw != nullptr) {
                                                std::wstring uri(uriRaw);
                                                ::CoTaskMemFree(uriRaw);
                                                if (IsExternalUri(uri)) {
                                                    args->put_Cancel(TRUE);
                                                    ::ShellExecute(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                                }
                                            }
                                            return S_OK;
                                        })
                                        .Get(),
                                    &token);
                            }
                            Resize();
                            NavigatePendingHtml();
                            return S_OK;
                        })
                        .Get());
                return S_OK;
            })
            .Get());
    if (FAILED(hr)) {
        webViewCreating_ = false;
        SetRuntimeMissingErrorShown();
        return false;
    }
    return true;
}

void WebViewHost::NavigatePendingHtml() {
    if (webView_ && !pendingHtml_.empty()) {
        webView_->NavigateToString(pendingHtml_.c_str());
    }
}

void WebViewHost::SetRuntimeMissingErrorShown() {
    if (!runtimeMissingErrorShown_) {
        runtimeMissingErrorShown_ = true;
        ::MessageBox(
            hostWindow_,
            L"Microsoft Edge WebView2 Runtime is required for rendered Markdown view.",
            L"Markdown Features",
            MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK WebViewHost::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<WebViewHost*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self != nullptr && (message == WM_SIZE || message == WM_TIMER)) {
        if (message == WM_TIMER && wParam != 1) {
            return ::DefWindowProc(hwnd, message, wParam, lParam);
        }
        self->Resize();
    }
    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

}  // namespace nmf
