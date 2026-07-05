#include "ui/ToolbarIcon.h"

#include <array>

namespace nmf {
namespace {

HICON CreateIconFromPixels(COLORREF foreground, COLORREF background) {
    constexpr int size = 16;
    HDC screen = ::GetDC(nullptr);
    HDC colorDc = ::CreateCompatibleDC(screen);
    HDC maskDc = ::CreateCompatibleDC(screen);
    HBITMAP color = ::CreateCompatibleBitmap(screen, size, size);
    HBITMAP mask = ::CreateBitmap(size, size, 1, 1, nullptr);
    auto* oldColor = static_cast<HBITMAP>(::SelectObject(colorDc, color));
    auto* oldMask = static_cast<HBITMAP>(::SelectObject(maskDc, mask));

    HBRUSH bg = ::CreateSolidBrush(background);
    RECT rect{0, 0, size, size};
    ::FillRect(colorDc, &rect, bg);
    ::DeleteObject(bg);

    HPEN fgPen = ::CreatePen(PS_SOLID, 2, foreground);
    auto* oldPen = static_cast<HPEN>(::SelectObject(colorDc, fgPen));
    ::MoveToEx(colorDc, 4, 3, nullptr);
    ::LineTo(colorDc, 4, 13);
    ::MoveToEx(colorDc, 4, 3, nullptr);
    ::LineTo(colorDc, 8, 9);
    ::LineTo(colorDc, 12, 3);
    ::MoveToEx(colorDc, 12, 3, nullptr);
    ::LineTo(colorDc, 12, 13);
    ::SelectObject(colorDc, oldPen);
    ::DeleteObject(fgPen);

    ::PatBlt(maskDc, 0, 0, size, size, WHITENESS);
    for (int y = 1; y < 15; ++y) {
        for (int x = 1; x < 15; ++x) {
            ::SetPixel(maskDc, x, y, RGB(0, 0, 0));
        }
    }

    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    HICON icon = ::CreateIconIndirect(&info);

    ::SelectObject(colorDc, oldColor);
    ::SelectObject(maskDc, oldMask);
    ::DeleteObject(color);
    ::DeleteObject(mask);
    ::DeleteDC(colorDc);
    ::DeleteDC(maskDc);
    ::ReleaseDC(nullptr, screen);
    return icon;
}

HBITMAP CreateBitmapFromPixels(COLORREF foreground, COLORREF background) {
    constexpr int size = 16;
    HDC screen = ::GetDC(nullptr);
    HDC dc = ::CreateCompatibleDC(screen);
    HBITMAP bitmap = ::CreateCompatibleBitmap(screen, size, size);
    auto* oldBitmap = static_cast<HBITMAP>(::SelectObject(dc, bitmap));

    HBRUSH bg = ::CreateSolidBrush(background);
    RECT rect{0, 0, size, size};
    ::FillRect(dc, &rect, bg);
    ::DeleteObject(bg);

    HPEN borderPen = ::CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    auto* oldPen = static_cast<HPEN>(::SelectObject(dc, borderPen));
    ::Rectangle(dc, 0, 0, size, size);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(borderPen);

    HPEN fgPen = ::CreatePen(PS_SOLID, 2, foreground);
    oldPen = static_cast<HPEN>(::SelectObject(dc, fgPen));
    ::MoveToEx(dc, 4, 3, nullptr);
    ::LineTo(dc, 4, 13);
    ::MoveToEx(dc, 4, 3, nullptr);
    ::LineTo(dc, 8, 9);
    ::LineTo(dc, 12, 3);
    ::MoveToEx(dc, 12, 3, nullptr);
    ::LineTo(dc, 12, 13);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(fgPen);

    ::SelectObject(dc, oldBitmap);
    ::DeleteDC(dc);
    ::ReleaseDC(nullptr, screen);
    return bitmap;
}

}  // namespace

HICON CreateToolbarIcon(bool darkMode) {
    return CreateIconFromPixels(darkMode ? RGB(245, 245, 245) : RGB(20, 70, 140), darkMode ? RGB(38, 38, 38) : RGB(255, 255, 255));
}

HBITMAP CreateToolbarBitmap() {
    return CreateBitmapFromPixels(RGB(20, 70, 140), RGB(255, 255, 255));
}

}  // namespace nmf
