#include "ui/AboutDialog.h"

namespace nmf {

void ShowAboutDialog(HWND owner) {
    ::MessageBox(
        owner,
        L"NppMarkdownFeatures v0.1.0\n\nMarkdown raw/rendered view toggle for Notepad++.\n\nLicense: MIT",
        L"About Markdown Features",
        MB_OK | MB_ICONINFORMATION);
}

}  // namespace nmf
