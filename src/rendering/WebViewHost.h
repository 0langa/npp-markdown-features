#pragma once

#include <string>
#include "core/ScrollSync.h"
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

    bool ShowHtml(HWND scintillaParent, const std::string& html, const std::wstring& sourcePath, const ScrollTarget& scrollTarget);
    ScrollTarget Hide();
    ScrollTarget LastScrollTarget() const;
    void ScrollToSourceLine(double sourceLine);
    void PrintRendered();     // opens the WebView2 print UI (browser dialog)
    void SetPrintPending();   // print once the next navigation completes
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
    ScrollTarget pendingScrollTarget_{};
    ScrollTarget lastScrollTarget_{};
    bool webViewErrorShown_{false};
    bool webViewCreating_{false};
    bool printPending_{false};
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;
};

}  // namespace nmf
