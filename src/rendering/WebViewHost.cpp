#include "rendering/WebViewHost.h"

#include "core/Strings.h"
#include "rendering/WebViewUserDataFolder.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <shellapi.h>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace nmf {
namespace {

constexpr wchar_t kHostWindowClass[] = L"NppMarkdownFeaturesWebViewHost";

std::wstring Utf8HtmlToWide(const std::string& html) {
    return Utf8ToWide(html);
}

// Anchor ids are generated slugs; keep the embedded JS literal safe regardless.
std::wstring SanitizeAnchorForScript(const std::string& anchorId) {
    std::string sanitized;
    sanitized.reserve(anchorId.size());
    for (const char ch : anchorId) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '_') {
            sanitized.push_back(ch);
        }
    }
    return Utf8ToWide(sanitized);
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

bool WebViewHost::ShowHtml(HWND scintillaParent, const std::string& html, const std::wstring& sourcePath, const ScrollTarget& scrollTarget) {
    parentScintilla_ = scintillaParent;
    sourcePath_ = sourcePath;
    pendingHtml_ = Utf8HtmlToWide(html);
    pendingScrollTarget_ = scrollTarget;
    pendingScrollTarget_.ratio = std::clamp(pendingScrollTarget_.ratio, 0.0, 1.0);
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

ScrollTarget WebViewHost::Hide() {
    CaptureScrollRatio();
    if (hostWindow_ != nullptr) {
        ::KillTimer(hostWindow_, 1);
        ::ShowWindow(hostWindow_, SW_HIDE);
    }
    return lastScrollTarget_;
}

ScrollTarget WebViewHost::LastScrollTarget() const {
    return lastScrollTarget_;
}

void WebViewHost::ScrollToSourceLine(double sourceLine) {
    if (!webView_) {
        return;
    }
    pendingScrollTarget_ = ScrollTarget{lastScrollTarget_.ratio, {}, sourceLine};
    ApplyPendingScroll();
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
    const auto userDataFolder = DefaultWebViewUserDataFolder();
    std::filesystem::create_directories(userDataFolder);
    userDataFolder_ = userDataFolder.wstring();
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder_.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                webViewCreating_ = false;
                if (FAILED(result) || environment == nullptr || hostWindow_ == nullptr) {
                    SetWebViewCreationErrorShown();
                    return S_OK;
                }
                environment_ = environment;
                environment_->CreateCoreWebView2Controller(
                    hostWindow_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || controller == nullptr) {
                                SetWebViewCreationErrorShown();
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
                                EventRegistrationToken navigationCompletedToken{};
                                webView_->add_NavigationCompleted(
                                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                            ApplyPendingScroll();
                                            return S_OK;
                                        })
                                        .Get(),
                                    &navigationCompletedToken);
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
        SetWebViewCreationErrorShown();
        return false;
    }
    return true;
}

void WebViewHost::NavigatePendingHtml() {
    if (webView_ && !pendingHtml_.empty()) {
        webView_->NavigateToString(pendingHtml_.c_str());
    }
}

void WebViewHost::ApplyPendingScroll() {
    if (!webView_) {
        return;
    }
    // Priority: fractional source line against data-sourcepos blocks, then
    // heading anchor, then plain viewport ratio.
    std::wstring script = L"(() => {\n";
    script += L"const line = " + std::to_wstring(pendingScrollTarget_.sourceLine) + L";\n";
    script += L"const anchorId = '" + SanitizeAnchorForScript(pendingScrollTarget_.anchorId) + L"';\n";
    script += L"const ratio = " + std::to_wstring(std::clamp(pendingScrollTarget_.ratio, 0.0, 1.0)) + L";\n";
    script += LR"JS(const pad = 8;
if (line >= 1) {
  let best = null;
  for (const el of document.querySelectorAll('[data-sourcepos]')) {
    const m = /^(\d+):\d+-(\d+):\d+$/.exec(el.getAttribute('data-sourcepos'));
    if (!m) continue;
    const s = +m[1];
    const e = +m[2];
    if (s <= line && (!best || s >= best.s)) best = { el, s, e };
  }
  if (best) {
    const rect = best.el.getBoundingClientRect();
    const span = best.e - best.s + 1;
    const frac = Math.min(1, Math.max(0, (line - best.s) / span));
    const top = window.scrollY + rect.top + rect.height * frac;
    window.scrollTo(0, Math.max(0, top - pad));
    return true;
  }
}
if (anchorId) {
  const anchor = document.getElementById(anchorId);
  if (anchor) { anchor.scrollIntoView({ block: 'start' }); return true; }
}
const max = Math.max(1, document.documentElement.scrollHeight - window.innerHeight);
window.scrollTo(0, max * ratio);
return true;
})();)JS";
    webView_->ExecuteScript(script.c_str(), nullptr);
    lastScrollTarget_ = pendingScrollTarget_;
}

void WebViewHost::CaptureScrollRatio() {
    if (!webView_) {
        return;
    }
    // Capture the innermost data-sourcepos block crossing the viewport top and
    // interpolate a fractional source line inside it; keep anchor and ratio as
    // fallback channels for documents without sourcepos data.
    webView_->ExecuteScript(
        LR"JS((() => {
const pad = 8;
const max = Math.max(1, document.documentElement.scrollHeight - window.innerHeight);
let line = -1;
let bestTop = -1e9;
for (const el of document.querySelectorAll('[data-sourcepos]')) {
  const rect = el.getBoundingClientRect();
  if (rect.height <= 0) continue;
  const m = /^(\d+):\d+-(\d+):\d+$/.exec(el.getAttribute('data-sourcepos'));
  if (!m) continue;
  if (rect.top <= pad && rect.bottom > pad && rect.top >= bestTop) {
    bestTop = rect.top;
    const s = +m[1];
    const e = +m[2];
    const frac = Math.min(1, Math.max(0, (pad - rect.top) / rect.height));
    line = s + frac * (e - s + 1);
  }
}
let anchorId = '';
for (const h of document.querySelectorAll('h1[id],h2[id],h3[id],h4[id],h5[id],h6[id]')) {
  if (h.getBoundingClientRect().top <= pad) anchorId = h.id || anchorId;
}
return { ratio: window.scrollY / max, line: line, anchorId: anchorId };
})();)JS",
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this](HRESULT result, LPCWSTR jsonResult) -> HRESULT {
                if (SUCCEEDED(result) && jsonResult != nullptr) {
                    try {
                        const auto parsed = nlohmann::json::parse(WideToUtf8(jsonResult));
                        if (parsed.is_object()) {
                            lastScrollTarget_.ratio = std::clamp(parsed.value("ratio", 0.0), 0.0, 1.0);
                            lastScrollTarget_.sourceLine = parsed.value("line", -1.0);
                            lastScrollTarget_.anchorId = parsed.value("anchorId", std::string{});
                        }
                    } catch (...) {
                        // Keep the previous capture on malformed results.
                    }
                }
                return S_OK;
            })
            .Get());
}

void WebViewHost::SetWebViewCreationErrorShown() {
    if (!webViewErrorShown_) {
        webViewErrorShown_ = true;
        std::wstring message = L"Microsoft Edge WebView2 could not start.\n\nData folder:\n";
        message += userDataFolder_.empty() ? L"(unknown)" : userDataFolder_;
        message += L"\n\nCheck WebView2 Runtime installation and folder permissions.";
        ::MessageBox(
            hostWindow_,
            message.c_str(),
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
        if (message == WM_TIMER) {
            self->CaptureScrollRatio();
        }
    }
    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

}  // namespace nmf
