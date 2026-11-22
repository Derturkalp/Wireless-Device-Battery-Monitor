// Kisisel: card-style battery panel (see flyout.h).
#include "flyout.h"

#include <algorithm>
#include <cwchar>

namespace {

const wchar_t* kClass = L"BatteryHubFlyout";

HINSTANCE g_inst;
HWND g_wnd;
HWND g_owner;
UINT g_cmdMsg;
bool g_light;
UINT g_dpi = 96;
POINT g_anchor;
ULONGLONG g_closedAt;
FlyoutContent g_c;
int g_hover = -1;  // card index, or kButtonBase + button
bool g_tracking;
bool g_hold;

constexpr int kButtonBase = 1000;
// Layout in 96-dpi pixels.
constexpr int W = 340, PAD = 12, HEADER = 40, CARD = 74, GAP = 8, BTN = 34, MARGIN = 12;

int S(int v) { return MulDiv(v, (int)g_dpi, 96); }

struct Palette {
    COLORREF bg, card, cardHover, cardBorder, text, sub, track, btn, btnHover;
};

Palette Colors() {
    if (g_light)
        return { RGB(243, 243, 243), RGB(251, 251, 251), RGB(240, 240, 240), RGB(222, 222, 222),
                 RGB(26, 26, 26), RGB(96, 96, 96), RGB(220, 220, 220), RGB(251, 251, 251), RGB(235, 235, 235) };
    return { RGB(32, 32, 32), RGB(43, 43, 43), RGB(52, 52, 52), RGB(58, 58, 58),
             RGB(255, 255, 255), RGB(165, 165, 165), RGB(72, 72, 72), RGB(45, 45, 45), RGB(60, 60, 60) };
}

int CardCount() { return std::max<int>(1, (int)g_c.items.size()); }

RECT CardRect(int i) {
    int y = PAD + HEADER + i * (CARD + GAP);
    return { S(PAD), S(y), S(W - PAD), S(y + CARD) };
}

int ButtonsTop() { return PAD + HEADER + CardCount() * (CARD + GAP) + 4; }

RECT ButtonRect(int b) {
    int w = (W - 2 * PAD - 2 * GAP) / 3;
    int x = PAD + b * (w + GAP);
    int y = ButtonsTop();
    return { S(x), S(y), S(x + w), S(y + BTN) };
}

int HeightPx() { return S(ButtonsTop() + BTN + PAD); }

int HitTest(POINT p) {
    for (int i = 0; i < (int)g_c.items.size(); i++) {
        RECT r = CardRect(i);
        if (PtInRect(&r, p)) return i;
    }
    for (int b = 0; b < 3; b++) {
        RECT r = ButtonRect(b);
        if (PtInRect(&r, p)) return kButtonBase + b;
    }
    return -1;
}

wchar_t Glyph(DeviceKind k, bool bluetooth) {
    switch (k) {
        case DeviceKind::Headset:    return 0xE7F6;
        case DeviceKind::Mouse:      return 0xE962;
        case DeviceKind::Keyboard:   return 0xE765;
        case DeviceKind::Gamepad:    return 0xE7FC;
        case DeviceKind::Phone:      return 0xE8EA;
        case DeviceKind::Microphone: return 0xE720;
        default:                     return bluetooth ? 0xE702 : 0xE772;
    }
}

HFONT Font(int px, int weight, const wchar_t* face, const wchar_t* fallback = nullptr) {
    HFONT f = CreateFontW(-S(px), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    if (fallback) {  // a missing face silently maps to another font; check the name that was picked
        HDC dc = GetDC(nullptr);
        HGDIOBJ old = SelectObject(dc, f);
        wchar_t got[LF_FACESIZE] = {};
        GetTextFaceW(dc, LF_FACESIZE, got);
        SelectObject(dc, old);
        ReleaseDC(nullptr, dc);
        if (_wcsicmp(got, face) != 0) {
            DeleteObject(f);
            f = CreateFontW(-S(px), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fallback);
        }
    }
    return f;
}

void Text(HDC dc, const std::wstring& s, RECT r, HFONT f, COLORREF c, UINT fmt) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, fmt | DT_NOPREFIX | DT_SINGLELINE);
}

void RoundBox(HDC dc, RECT r, int radius, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

COLORREF LevelColor(const FlyoutItem& it) {
    if (it.charging) return RGB(0, 150, 255);
    if (it.percent >= 0 && it.percent <= 20) return RGB(232, 32, 48);
    if (it.percent >= 0 && it.percent <= 40) return RGB(255, 150, 0);
    return RGB(34, 197, 94);
}

void Paint(HDC dc, int w, int h) {
    Palette pal = Colors();
    RECT all{ 0, 0, w, h };
    HBRUSH bg = CreateSolidBrush(pal.bg);
    FillRect(dc, &all, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    HFONT title = Font(15, FW_SEMIBOLD, L"Segoe UI Variable Display", L"Segoe UI");
    HFONT smallF = Font(11, FW_NORMAL, L"Segoe UI Variable Text", L"Segoe UI");
    HFONT name = Font(13, FW_SEMIBOLD, L"Segoe UI Variable Text", L"Segoe UI");
    HFONT pct = Font(18, FW_BOLD, L"Segoe UI Variable Display", L"Segoe UI");
    HFONT icon = Font(20, FW_NORMAL, L"Segoe Fluent Icons", L"Segoe MDL2 Assets");
    HFONT bolt = Font(13, FW_NORMAL, L"Segoe Fluent Icons", L"Segoe MDL2 Assets");
    HFONT btn = Font(12, FW_NORMAL, L"Segoe UI Variable Text", L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, title);

    RECT head{ S(PAD + 4), S(PAD), S(W - PAD - 4), S(PAD + HEADER - 6) };
    Text(dc, g_c.title, head, title, pal.text, DT_LEFT | DT_VCENTER);
    Text(dc, g_c.count, head, smallF, pal.sub, DT_RIGHT | DT_VCENTER);

    if (g_c.items.empty()) {
        RECT r = CardRect(0);
        RoundBox(dc, r, S(12), pal.card, pal.cardBorder);
        Text(dc, g_c.empty, r, smallF, pal.sub, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
    for (int i = 0; i < (int)g_c.items.size(); i++) {
        const FlyoutItem& it = g_c.items[i];
        RECT r = CardRect(i);
        RoundBox(dc, r, S(12), g_hover == i ? pal.cardHover : pal.card, pal.cardBorder);
        COLORREF mainC = it.dim ? pal.sub : pal.text;

        RECT ic{ r.left + S(10), r.top + S(8), r.left + S(46), r.top + S(48) };
        Text(dc, std::wstring(1, Glyph(it.kind, it.bluetooth)), ic, icon, mainC, DT_CENTER | DT_VCENTER);

        // Percentage (right), with a bolt while charging or a check mark once full.
        std::wstring p = !it.text.empty() ? it.text : it.percent >= 0 ? std::to_wstring(it.percent) + L"%" : L"?";
        RECT pr{ r.right - S(110), r.top + S(10), r.right - S(14), r.top + S(42) };
        COLORREF pc = it.low ? RGB(240, 60, 70) : mainC;
        Text(dc, p, pr, pct, pc, DT_RIGHT | DT_VCENTER);
        SelectObject(dc, pct);
        SIZE ext{};
        GetTextExtentPoint32W(dc, p.c_str(), (int)p.size(), &ext);
        int nameRight = pr.right - ext.cx - S(10);
        if (it.charging || it.full) {
            RECT br{ pr.right - ext.cx - S(22), pr.top, pr.right - ext.cx - S(4), pr.bottom };
            Text(dc, std::wstring(1, it.full ? (wchar_t)0xE73E : (wchar_t)0xE945), br, bolt,
                 it.full ? RGB(34, 197, 94) : RGB(0, 150, 255), DT_CENTER | DT_VCENTER);
            nameRight = br.left - S(6);
        }

        RECT nr{ r.left + S(52), r.top + S(11), nameRight, r.top + S(30) };
        Text(dc, it.name, nr, name, mainC, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        RECT sr{ r.left + S(52), r.top + S(30), nameRight, r.top + S(46) };
        Text(dc, it.sub, sr, smallF, pal.sub, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        // Level bar.
        RECT track{ r.left + S(14), r.bottom - S(18), r.right - S(14), r.bottom - S(12) };
        int rad = track.bottom - track.top;
        RoundBox(dc, track, rad, pal.track, pal.track);
        if (it.percent > 0) {
            RECT fill = track;
            fill.right = track.left + std::max<int>(rad, (track.right - track.left) * std::min(it.percent, 100) / 100);
            COLORREF c = LevelColor(it);
            if (it.dim) c = pal.sub;
            RoundBox(dc, fill, rad, c, c);
        }
    }
    for (int b = 0; b < 3; b++) {
        RECT r = ButtonRect(b);
        RoundBox(dc, r, S(8), g_hover == kButtonBase + b ? pal.btnHover : pal.btn, pal.cardBorder);
        RECT t = r;
        InflateRect(&t, -S(4), 0);
        Text(dc, g_c.buttons[b], t, btn, pal.text, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }

    SelectObject(dc, oldFont);
    for (HFONT f : { title, smallF, name, pct, icon, bolt, btn }) DeleteObject(f);
}

void ApplyCorners() {
    // Windows 11: rounded corners with the system border and shadow. Windows 10: a rounded region.
    typedef HRESULT(WINAPI * SetAttr)(HWND, DWORD, LPCVOID, DWORD);
    static SetAttr set = (SetAttr)(void*)GetProcAddress(LoadLibraryW(L"dwmapi.dll"), "DwmSetWindowAttribute");
    int round = 2;  // DWMWCP_ROUND
    if (set && set(g_wnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof(round)) == S_OK) {
        BOOL dark = !g_light;
        set(g_wnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
        return;
    }
    RECT r;
    GetWindowRect(g_wnd, &r);
    SetWindowRgn(g_wnd, CreateRoundRectRgn(0, 0, r.right - r.left + 1, r.bottom - r.top + 1, S(16), S(16)), TRUE);
}

void Place() {
    if (!g_wnd) return;
    UINT d = GetDpiForWindow(g_wnd);
    g_dpi = d ? d : 96;
    int w = S(W), h = HeightPx();
    HMONITOR mon = MonitorFromPoint(g_anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    RECT wa = mi.rcWork, mr = mi.rcMonitor;
    int x = g_anchor.x - w / 2, y;
    if (wa.bottom < mr.bottom) y = wa.bottom - h - S(MARGIN);  // taskbar at the bottom
    else if (wa.top > mr.top) y = wa.top + S(MARGIN);           // at the top
    else y = g_anchor.y - h - S(MARGIN);                        // left / right / auto-hide
    if (wa.left > mr.left) x = wa.left + S(MARGIN);
    if (wa.right < mr.right) x = wa.right - w - S(MARGIN);
    x = std::max<int>(wa.left + S(MARGIN), std::min<int>(x, wa.right - w - S(MARGIN)));
    y = std::max<int>(wa.top + S(MARGIN), std::min<int>(y, wa.bottom - h - S(MARGIN)));
    SetWindowPos(g_wnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    ApplyCorners();
    InvalidateRect(g_wnd, nullptr, FALSE);
}

void Send(WPARAM cmd, LPARAM arg) { PostMessageW(g_owner, g_cmdMsg, cmd, arg); }

LRESULT CALLBACK Proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT r;
            GetClientRect(h, &r);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, r.right, r.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            Paint(mem, r.right, r.bottom);
            BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            if (!g_tracking) {
                TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, h, 0 };
                TrackMouseEvent(&t);
                g_tracking = true;
            }
            POINT p{ (short)LOWORD(lp), (short)HIWORD(lp) };
            int hit = HitTest(p);
            if (hit != g_hover) { g_hover = hit; InvalidateRect(h, nullptr, FALSE); }
            SetCursor(LoadCursor(nullptr, hit >= 0 ? IDC_HAND : IDC_ARROW));
            return 0;
        }
        case WM_MOUSELEAVE:
            g_tracking = false;
            if (g_hover != -1) { g_hover = -1; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        case WM_LBUTTONUP: {
            POINT p{ (short)LOWORD(lp), (short)HIWORD(lp) };
            int hit = HitTest(p);
            if (hit == kButtonBase) { Send(flyout::CMD_REFRESH, 0); return 0; }  // stays open, updates in place
            if (hit < 0) return 0;
            // The panel stays open; the owner shows its menu on top and refreshes the cards afterwards.
            if (hit == kButtonBase + 1) Send(flyout::CMD_HISTORY, 0);
            else if (hit == kButtonBase + 2) Send(flyout::CMD_LANGUAGE, 0);
            else Send(flyout::CMD_DEVICE, hit);
            return 0;
        }
        case WM_RBUTTONUP:
            Send(flyout::CMD_MENU, 0);
            DestroyWindow(h);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) DestroyWindow(h);
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE && !g_hold) PostMessageW(h, WM_CLOSE, 0, 0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DPICHANGED:
            Place();
            return 0;
        case WM_DESTROY:
            g_wnd = nullptr;
            g_hover = -1;
            g_tracking = false;
            g_closedAt = GetTickCount64();
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

namespace flyout {

bool Visible() { return g_wnd != nullptr; }

HWND Window() { return g_wnd; }

void Hold(bool hold) {
    g_hold = hold;
    // Released while something else took the focus (the user clicked away from the menu): close.
    if (!hold && g_wnd && GetForegroundWindow() != g_wnd) PostMessageW(g_wnd, WM_CLOSE, 0, 0);
}

void Close() {
    if (g_wnd) DestroyWindow(g_wnd);
}

void SetContent(const FlyoutContent& c) {
    g_c = c;
    if (g_wnd) {
        if (g_hover >= 0 && g_hover < kButtonBase && g_hover >= (int)g_c.items.size()) g_hover = -1;
        Place();
    }
}

void Toggle(HINSTANCE inst, HWND owner, UINT cmdMsg, POINT anchor, bool light) {
    if (g_wnd) { Close(); return; }
    if (GetTickCount64() - g_closedAt < 300) return;  // this click is the one that just closed it
    g_inst = inst;
    g_owner = owner;
    g_cmdMsg = cmdMsg;
    g_anchor = anchor;
    g_light = light;
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = Proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        registered = RegisterClassExW(&wc) != 0;
        if (!registered) return;
    }
    g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kClass, L"BatteryHub", WS_POPUP,
                            anchor.x, anchor.y, 1, 1, nullptr, nullptr, inst, nullptr);
    if (!g_wnd) return;
    Place();
    ShowWindow(g_wnd, SW_SHOW);
    SetForegroundWindow(g_wnd);
    SetFocus(g_wnd);
}

}  // namespace flyout
