#pragma once
// Kisisel: a card-style panel that opens above the taskbar when the widget (or a tray icon) is
// left-clicked. One card per shown device with its icon, name, connection, percentage and a level
// bar, plus "Refresh now", "Battery history..." and "Language" buttons. Right-click keeps the
// classic menu with every setting.
#include <windows.h>
#include <string>
#include <vector>
#include "devices.h"

struct FlyoutItem {
    DeviceKind kind = DeviceKind::Other;
    std::wstring name;
    std::wstring sub;        // connection ("2.4 GHz receiver"), plus "asleep" etc.
    int percent = -1;        // -1 = unknown
    std::wstring text;       // shown instead of "NN%" when set (coarse levels, "×" for offline)
    bool charging = false;
    bool full = false;
    bool dim = false;
    bool low = false;
    bool bluetooth = false;
};

struct FlyoutContent {
    std::wstring title;      // "BatteryHub"
    std::wstring count;      // "3 devices"
    std::wstring empty;      // shown when there is no device
    std::wstring buttons[3]; // refresh, history, language
    std::vector<FlyoutItem> items;
};

namespace flyout {
enum Command : WPARAM { CMD_REFRESH = 1, CMD_HISTORY, CMD_LANGUAGE, CMD_DEVICE, CMD_MENU };
// Commands are posted to `owner` as `cmdMsg` with wParam = Command and lParam = device index (CMD_DEVICE).
void Toggle(HINSTANCE inst, HWND owner, UINT cmdMsg, POINT anchor, bool light);
void SetContent(const FlyoutContent& c);  // also updates an open panel
void Close();
bool Visible();
HWND Window();          // the open panel, or nullptr
void Hold(bool hold);   // while true the panel stays open even if it loses focus (menus on top of it)
}
