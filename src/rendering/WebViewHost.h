#pragma once

#include <string>
#include <windows.h>
#include <unknwn.h>
#include <EventToken.h>
#include <WebView2.h>
#include <wrl.h>

namespace nmf {

class WebViewHost {
public:
    WebViewHost();
    ~WebViewHost();

    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    bool ShowHtml(HWND scintillaParent, const std::string& html, const std::wstring& sourcePath, double scrollRatio);
    double Hide();
    double LastScrollRatio() const;
    void Resize();
    void Destroy();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    bool EnsureHostWindow(HWND scintillaParent);
    bool EnsureWebView();
    void NavigatePendingHtml();
    void ApplyPendingScroll();
    void CaptureScrollRatio();
    void SetWebViewCreationErrorShown();

    HWND parentScintilla_{};
    HWND hostWindow_{};
    std::wstring sourcePath_{};
    std::wstring pendingHtml_{};
    std::wstring userDataFolder_{};
    double pendingScrollRatio_{0.0};
    double lastScrollRatio_{0.0};
    bool webViewErrorShown_{false};
    bool webViewCreating_{false};
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;
};

}  // namespace nmf
