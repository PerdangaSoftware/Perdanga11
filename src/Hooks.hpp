#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <string>
#include "Config.hpp"

#define WM_APP_TOGGLE_MENU (WM_USER + 2)
#define WM_APP_CLOSE_MENU  (WM_USER + 3)

class Hooks {
public:
    static inline HHOOK g_hKeyboardHook = nullptr;
    static inline HHOOK g_hMouseHook = nullptr;
    static inline HWINEVENTHOOK g_hWinEventHook = nullptr;
    static inline HWND g_hTargetWnd = nullptr;
    static inline RECT g_startButtonRect = { 0, 0, 0, 0 };
    static inline bool g_winKeyDown = false;
    static inline bool g_winCombinationPressed = false;
    static inline bool g_mouseClickIntercepted = false;

    // Checks if the foreground application is running in full-screen mode (e.g. video games, media players)
    static bool IsForegroundFullScreen(POINT pt) {
        HWND hFore = GetForegroundWindow();
        if (!hFore || hFore == GetDesktopWindow()) return false;

        HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (hFore == hTaskbar) return false;

        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfoW(hMon, &mi)) {
            RECT rcFore;
            if (GetWindowRect(hFore, &rcFore)) {
                if (rcFore.left <= mi.rcMonitor.left &&
                    rcFore.top <= mi.rcMonitor.top &&
                    rcFore.right >= mi.rcMonitor.right &&
                    rcFore.bottom >= mi.rcMonitor.bottom) {
                    return true;
                }
            }
        }
        return false;
    }

    // Checks if the mouse cursor physically hovers over the actual Windows taskbar hierarchy
    static bool IsCursorDirectlyOnTaskbar(POINT pt) {
        HWND hUnder = WindowFromPoint(pt);
        if (!hUnder) return false;

        HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (hUnder == hTaskbar || (hTaskbar && IsChild(hTaskbar, hUnder))) {
            return true;
        }

        HWND hSecTaskbar = FindWindowW(L"Shell_SecondaryTrayWnd", nullptr);
        if (hUnder == hSecTaskbar || (hSecTaskbar && IsChild(hSecTaskbar, hUnder))) {
            return true;
        }

        return false;
    }

    static void DismissWindowsSearch() {
        HWND hSearch = FindWindowW(L"Windows.UI.Core.CoreWindow", L"Search");
        if (!hSearch) hSearch = FindWindowW(L"Windows.UI.Core.CoreWindow", L"\x041F\x043E\x0438\x0441\x043A");

        if (hSearch && IsWindowVisible(hSearch)) {
            keybd_event(VK_ESCAPE, 0, 0, 0);
            keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
        }
    }

    static void UpdateStartButtonRect() {
        HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!hTaskbar) return;

        HWND hStartBtn = FindWindowExW(hTaskbar, nullptr, L"Start", nullptr);
        if (!hStartBtn) {
            hStartBtn = FindWindowExW(hTaskbar, nullptr, L"Button", L"Start");
        }
        if (hStartBtn) {
            RECT rc;
            if (GetWindowRect(hStartBtn, &rc) && (rc.right - rc.left) > 0) {
                g_startButtonRect = rc;
                return;
            }
        }

        IUIAutomation* pAutomation = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);
        if (FAILED(hr) || !pAutomation) return;

        IUIAutomationElement* pTaskbarElem = nullptr;
        if (SUCCEEDED(pAutomation->ElementFromHandle(hTaskbar, &pTaskbarElem)) && pTaskbarElem) {
            VARIANT var;
            var.vt = VT_BSTR;
            var.bstrVal = SysAllocString(L"StartButton");

            IUIAutomationCondition* pCond = nullptr;
            pAutomation->CreatePropertyCondition(UIA_AutomationIdPropertyId, var, &pCond);
            SysFreeString(var.bstrVal);

            if (pCond) {
                IUIAutomationElement* pStartBtn = nullptr;
                if (SUCCEEDED(pTaskbarElem->FindFirst(TreeScope_Descendants, pCond, &pStartBtn)) && pStartBtn) {
                    tagRECT rect;
                    if (SUCCEEDED(pStartBtn->get_CurrentBoundingRectangle(&rect))) {
                        g_startButtonRect = rect;
                    }
                    pStartBtn->Release();
                }
                pCond->Release();
            }
            pTaskbarElem->Release();
        }
        pAutomation->Release();
    }

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION) {
            KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

            if (pKey->flags & LLKHF_INJECTED) {
                return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
            }

            if (pKey->vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    PostMessageW(g_hTargetWnd, WM_APP_TOGGLE_MENU, 0, 0);
                    return 1;
                }
            }

            if (pKey->vkCode == VK_LWIN || pKey->vkCode == VK_RWIN) {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    g_winKeyDown = true;
                    g_winCombinationPressed = false;
                    return 1;
                } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                    if (g_winKeyDown && !g_winCombinationPressed) {
                        PostMessageW(g_hTargetWnd, WM_APP_TOGGLE_MENU, 0, 0);
                        keybd_event(0xE8, 0, KEYEVENTF_KEYUP, 0);
                    } else if (g_winCombinationPressed) {
                        keybd_event((BYTE)pKey->vkCode, 0, KEYEVENTF_KEYUP, 0);
                    }
                    g_winKeyDown = false;
                    g_winCombinationPressed = false;
                    return 1;
                }
            } else if (g_winKeyDown) {
                if (!g_winCombinationPressed) {
                    g_winCombinationPressed = true;
                    keybd_event(VK_LWIN, 0, 0, 0);
                }
            }
        }
        return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
    }

    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD) {
        if (event == EVENT_SYSTEM_FOREGROUND && hwnd && hwnd != g_hTargetWnd) {
            wchar_t className[256] = { 0 };
            GetClassNameW(hwnd, className, 256);

            if (wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0) {
                wchar_t title[256] = { 0 };
                GetWindowTextW(hwnd, title, 256);

                if (wcscmp(title, L"Search") == 0 || wcscmp(title, L"\x041F\x043E\x0438\x0441\x043A") == 0) {
                    PostMessageW(g_hTargetWnd, WM_APP_CLOSE_MENU, 0, 0);
                    return;
                }

                if (wcscmp(title, L"Start") == 0 || wcscmp(title, L"\x041F\x0443\x0441\x043A") == 0) {
                    ShowWindow(hwnd, SW_HIDE);
                    PostMessageW(g_hTargetWnd, WM_APP_TOGGLE_MENU, 0, 0);
                    return;
                }
            }
        }
    }

    static void Install(HWND targetWnd, HINSTANCE hInst) {
        g_hTargetWnd = targetWnd;
        g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
        g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, hInst, 0);
        g_hWinEventHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, WinEventProc, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
        );
        UpdateStartButtonRect();
    }

    static void Uninstall() {
        if (g_hKeyboardHook) { UnhookWindowsHookEx(g_hKeyboardHook); g_hKeyboardHook = nullptr; }
        if (g_hMouseHook) { UnhookWindowsHookEx(g_hMouseHook); g_hMouseHook = nullptr; }
        if (g_hWinEventHook) { UnhookWinEvent(g_hWinEventHook); g_hWinEventHook = nullptr; }
    }
};