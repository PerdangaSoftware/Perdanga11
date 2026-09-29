#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <uiautomation.h>
#include <string>
#include "Config.hpp"

#define WM_APP_TOGGLE_MENU         (WM_USER + 2)
#define WM_APP_CLOSE_MENU          (WM_USER + 3)
#define WM_APP_NATIVE_START_OPENED (WM_USER + 6)

class Hooks {
public:
    static inline HHOOK g_hKeyboardHook = nullptr;
    static inline HHOOK g_hMouseHook = nullptr;
    static inline HWINEVENTHOOK g_hWinEventHook = nullptr;
    static inline HWND g_hTargetWnd = nullptr;
    static inline RECT g_startButtonRect = { 0, 0, 0, 0 };
    static inline UINT g_taskbarEdge = ABE_BOTTOM;
    static inline DWORD g_taskbarAlignment = 1;
    static inline bool g_winKeyDown = false;
    static inline bool g_winCombinationPressed = false;
    static inline bool g_mouseClickIntercepted = false;

    static inline HKEY g_hKeyAdvanced = nullptr;
    static inline HANDLE g_hRegChangeEvent = nullptr;

    static inline HINSTANCE g_hInst = nullptr;
    static inline HANDLE g_hHookThread = nullptr;
    static inline DWORD g_hookThreadId = 0;

    static bool IsForegroundFullScreen(POINT pt) {
        HWND hFore = GetForegroundWindow();
        if (!hFore || hFore == GetDesktopWindow()) return false;

        wchar_t className[64] = { 0 };
        if (GetClassNameW(hFore, className, 64)) {
            if (wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0) {
                return false;
            }
        }

        DWORD foreStyle = (DWORD)GetWindowLongW(hFore, GWL_STYLE);
        if ((foreStyle & WS_CAPTION) != 0) return false;

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

    static bool IsWindowOwnedByProcess(HWND hwnd, const wchar_t* exeName) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (!pid) return false;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) return false;

        wchar_t path[MAX_PATH] = { 0 };
        DWORD size = MAX_PATH;
        BOOL ok = QueryFullProcessImageNameW(hProc, 0, path, &size);
        CloseHandle(hProc);
        if (!ok) return false;

        const wchar_t* base = wcsrchr(path, L'\\');
        base = base ? base + 1 : path;
        return _wcsicmp(base, exeName) == 0;
    }

    static bool IsNativeStartWindow(HWND hwnd) {
        return IsWindowOwnedByProcess(hwnd, L"StartMenuExperienceHost.exe");
    }

    static bool IsNativeSearchWindow(HWND hwnd) {
        return IsWindowOwnedByProcess(hwnd, L"SearchHost.exe");
    }

    static BOOL CALLBACK HideShellOverlayEnumProc(HWND hwnd, LPARAM) {
        if (!IsWindowVisible(hwnd)) return TRUE;

        if (IsNativeStartWindow(hwnd) || IsNativeSearchWindow(hwnd)) {
            ShowWindow(hwnd, SW_HIDE);
        }
        return TRUE;
    }

    static void DismissNativeShellOverlays() {
        HWND hFore = GetForegroundWindow();
        if (hFore && (IsNativeStartWindow(hFore) || IsNativeSearchWindow(hFore))) {
            keybd_event(VK_ESCAPE, 0, 0, 0);
            keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
        }

        EnumWindows(HideShellOverlayEnumProc, 0);
    }

    static void UpdateStartButtonRect() {
        HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!hTaskbar) return;

        APPBARDATA abd = { sizeof(APPBARDATA) };
        abd.hWnd = hTaskbar;
        if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
            g_taskbarEdge = abd.uEdge;
        } else {
            RECT rcT = { 0 };
            if (GetWindowRect(hTaskbar, &rcT)) {
                HMONITOR hMon = MonitorFromWindow(hTaskbar, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(MONITORINFO) };
                if (GetMonitorInfoW(hMon, &mi)) {
                    if (rcT.left <= mi.rcMonitor.left && rcT.right >= mi.rcMonitor.right) {
                        g_taskbarEdge = (rcT.top <= mi.rcMonitor.top) ? ABE_TOP : ABE_BOTTOM;
                    } else {
                        g_taskbarEdge = (rcT.left <= mi.rcMonitor.left) ? ABE_LEFT : ABE_RIGHT;
                    }
                }
            }
        }

        DWORD taskbarAl = 1;
        if (g_hKeyAdvanced) {
            DWORD sz = sizeof(taskbarAl);
            RegQueryValueExW(g_hKeyAdvanced, L"TaskbarAl", nullptr, nullptr, (LPBYTE)&taskbarAl, &sz);
        } else {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                    0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                DWORD type = 0;
                DWORD size = sizeof(DWORD);
                RegQueryValueExW(hKey, L"TaskbarAl", nullptr, &type, (LPBYTE)&taskbarAl, &size);
                RegCloseKey(hKey);
            }
        }
        g_taskbarAlignment = taskbarAl;

        HWND hStartBtn = FindWindowExW(hTaskbar, nullptr, L"Start", nullptr);
        if (!hStartBtn) {
            hStartBtn = FindWindowExW(hTaskbar, nullptr, L"Button", L"Start");
        }
        if (hStartBtn) {
            RECT rc;
            if (GetWindowRect(hStartBtn, &rc) && (rc.right - rc.left) > 0 && (rc.bottom - rc.top) > 0) {
                g_startButtonRect = rc;
                return;
            }
        }

        bool startButtonResolved = false;
        IUIAutomation* pAutomation = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);
        if (SUCCEEDED(hr) && pAutomation) {
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
                            if ((rect.right - rect.left) > 0 && (rect.bottom - rect.top) > 0) {
                                RECT rcTaskbar;
                                if (GetWindowRect(hTaskbar, &rcTaskbar)) {
                                    bool isUiaStale = false;
                                    int btnWidth = rect.right - rect.left;
                                    int barWidth = rcTaskbar.right - rcTaskbar.left;
                                    int distFromLeft = rect.left - rcTaskbar.left;

                                    if (g_taskbarEdge == ABE_BOTTOM || g_taskbarEdge == ABE_TOP) {
                                        if (g_taskbarAlignment == 0) {
                                            if (distFromLeft > btnWidth * 3) {
                                                isUiaStale = true;
                                            }
                                        } else if (g_taskbarAlignment == 1) {
                                            if (distFromLeft < btnWidth * 2 && barWidth > btnWidth * 5) {
                                                isUiaStale = true;
                                            }
                                        }
                                    }

                                    if (!isUiaStale) {
                                        g_startButtonRect = rect;
                                        startButtonResolved = true;
                                    }
                                }
                            }
                        }
                        pStartBtn->Release();
                    }
                    pCond->Release();
                }
                pTaskbarElem->Release();
            }
            pAutomation->Release();
        }

        if (!startButtonResolved) {
            RECT rcTaskbar;
            if (GetWindowRect(hTaskbar, &rcTaskbar)) {
                if (g_taskbarEdge == ABE_BOTTOM || g_taskbarEdge == ABE_TOP) {
                    int btnSize = rcTaskbar.bottom - rcTaskbar.top;
                    if (btnSize <= 0) btnSize = 48;

                    if (g_taskbarAlignment == 1) {
                        int centerX = (rcTaskbar.left + rcTaskbar.right) / 2;
                        g_startButtonRect = { centerX - btnSize / 2, rcTaskbar.top, centerX + btnSize / 2, rcTaskbar.bottom };
                    } else {
                        g_startButtonRect = { rcTaskbar.left, rcTaskbar.top, rcTaskbar.left + btnSize, rcTaskbar.bottom };
                    }
                } else {
                    int btnSize = rcTaskbar.right - rcTaskbar.left;
                    if (btnSize <= 0) btnSize = 48;

                    if (g_taskbarAlignment == 1) {
                        int centerY = (rcTaskbar.top + rcTaskbar.bottom) / 2;
                        g_startButtonRect = { rcTaskbar.left, centerY - btnSize / 2, rcTaskbar.right, centerY + btnSize / 2 };
                    } else {
                        g_startButtonRect = { rcTaskbar.left, rcTaskbar.top, rcTaskbar.right, rcTaskbar.top + btnSize };
                    }
                }
            }
        }
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

    static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD);
    static void CALLBACK NativeStartWatchdogTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static BOOL CALLBACK NativeStartWatchdogEnumProc(HWND hwnd, LPARAM lParam);

    static DWORD WINAPI HookThreadProc(LPVOID) {
        g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInst, 0);
        g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, g_hInst, 0);

        HANDLE hWaitEvents[1] = { g_hRegChangeEvent };
        DWORD dwCount = g_hRegChangeEvent ? 1 : 0;
        bool bRunning = true;

        while (bRunning) {
            DWORD dwWait = MsgWaitForMultipleObjectsEx(dwCount, hWaitEvents, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE);

            if (dwWait == WAIT_OBJECT_0) {
                UpdateStartButtonRect();
                if (g_hKeyAdvanced && g_hRegChangeEvent) {
                    RegNotifyChangeKeyValue(g_hKeyAdvanced, FALSE, REG_NOTIFY_CHANGE_LAST_SET, g_hRegChangeEvent, TRUE);
                }
            } else if (dwWait == (WAIT_OBJECT_0 + dwCount)) {
                MSG msg;
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        bRunning = false;
                        break;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
        }

        if (g_hKeyboardHook) { UnhookWindowsHookEx(g_hKeyboardHook); g_hKeyboardHook = nullptr; }
        if (g_hMouseHook) { UnhookWindowsHookEx(g_hMouseHook); g_hMouseHook = nullptr; }
        return 0;
    }

    static void Install(HWND targetWnd, HINSTANCE hInst) {
        g_hTargetWnd = targetWnd;
        g_hInst = hInst;

        RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
            0, KEY_READ | KEY_NOTIFY, &g_hKeyAdvanced);

        g_hRegChangeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (g_hKeyAdvanced && g_hRegChangeEvent) {
            RegNotifyChangeKeyValue(g_hKeyAdvanced, FALSE, REG_NOTIFY_CHANGE_LAST_SET, g_hRegChangeEvent, TRUE);
        }

        UpdateStartButtonRect();

        g_hHookThread = CreateThread(nullptr, 0, HookThreadProc, nullptr, 0, &g_hookThreadId);

        g_hWinEventHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, WinEventProc, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    static void Uninstall() {
        if (g_hWinEventHook) { UnhookWinEvent(g_hWinEventHook); g_hWinEventHook = nullptr; }
        if (g_hookThreadId) {
            PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
        }
        if (g_hHookThread) {
            WaitForSingleObject(g_hHookThread, 3000);
            CloseHandle(g_hHookThread);
            g_hHookThread = nullptr;
        }
        g_hookThreadId = 0;

        if (g_hKeyAdvanced) {
            RegCloseKey(g_hKeyAdvanced);
            g_hKeyAdvanced = nullptr;
        }
        if (g_hRegChangeEvent) {
            CloseHandle(g_hRegChangeEvent);
            g_hRegChangeEvent = nullptr;
        }
    }
};