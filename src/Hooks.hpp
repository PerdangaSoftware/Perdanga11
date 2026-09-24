#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <string>
#include "Config.hpp"

#define WM_APP_TOGGLE_MENU         (WM_USER + 2)
#define WM_APP_CLOSE_MENU          (WM_USER + 3)
// WM_USER + 4 / + 5 are reserved by Config.hpp (WM_APP_INDEX_READY / WM_APP_SEARCH_COMPLETE)
#define WM_APP_NATIVE_START_OPENED (WM_USER + 6)

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

    // Dedicated thread that owns the low-level hooks. Keeping them off the UI
    // thread is what makes the native-Start swallow reliable (see HookThreadProc).
    static inline HINSTANCE g_hInst = nullptr;
    static inline HANDLE g_hHookThread = nullptr;
    static inline DWORD g_hookThreadId = 0;

    // Checks if the foreground application is running in full-screen mode (e.g. video games, media players)
    static bool IsForegroundFullScreen(POINT pt) {
        HWND hFore = GetForegroundWindow();
        if (!hFore || hFore == GetDesktopWindow()) return false;

        // Clicking the desktop makes the shell desktop window (Progman / WorkerW)
        // foreground; its rect spans the whole monitor and would falsely count as
        // fullscreen, disabling Start interception. The same happens with any
        // maximized overlapped window (e.g. Explorer).
        wchar_t className[64] = { 0 };
        if (GetClassNameW(hFore, className, 64)) {
            if (wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0) {
                return false;
            }
        }

        // True exclusive-fullscreen windows (games, media players) are borderless.
        // Any window that keeps its caption bar is a regular app, even maximized.
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

    // True when the window belongs to a process with the given image file name.
    // Process-based detection survives window title/class changes across Win11 builds.
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

    // Forcibly dismisses native Start / Search overlays so they can never
    // overlap the Perdanga11 menu (fixes "two windows hanging" on top of each other)
    static void DismissNativeShellOverlays() {
        // Graceful path first: ESC closes the overlay when it currently has focus
        HWND hFore = GetForegroundWindow();
        if (hFore && (IsNativeStartWindow(hFore) || IsNativeSearchWindow(hFore))) {
            keybd_event(VK_ESCAPE, 0, 0, 0);
            keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
        }

        // Then hide any overlay windows still left on screen
        EnumWindows(HideShellOverlayEnumProc, 0);
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

        bool startButtonResolved = false;
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
                        startButtonResolved = true;
                    }
                    pStartBtn->Release();
                }
                pCond->Release();
            }
            pTaskbarElem->Release();
        }
        pAutomation->Release();

        // Final fallback: derive the Start button position from the taskbar
        // alignment setting. The Windows 11 taskbar renders inside a XAML island
        // (no child HWND to find), so when the UIA "StartButton" automation id is
        // missing on newer shell builds this keeps click interception alive.
        if (!startButtonResolved) {
            RECT rcTaskbar;
            if (GetWindowRect(hTaskbar, &rcTaskbar)) {
                DWORD taskbarAl = 1; // 1 = centered (Windows 11 default), 0 = left
                HKEY hKey = nullptr;
                if (RegOpenKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                    DWORD type = 0;
                    DWORD size = sizeof(DWORD);
                    RegQueryValueExW(hKey, L"TaskbarAl", nullptr, &type, (LPBYTE)&taskbarAl, &size);
                    RegCloseKey(hKey);
                }

                int btnSize = rcTaskbar.bottom - rcTaskbar.top;
                if (btnSize > 0) {
                    if (taskbarAl == 1) {
                        int centerX = (rcTaskbar.left + rcTaskbar.right) / 2;
                        g_startButtonRect = { centerX - btnSize / 2, rcTaskbar.top, centerX + btnSize / 2, rcTaskbar.bottom };
                    } else {
                        g_startButtonRect = { rcTaskbar.left, rcTaskbar.top, rcTaskbar.left + btnSize, rcTaskbar.bottom };
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

    // Defined in MenuWindow.hpp: needs MenuWindow visibility state to decide
    // whether to open or keep our menu when native Start surfaces
    static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD);

    // Defined in MenuWindow.hpp: periodic guarantee sweep that hides any native
    // Start / Search overlay, no matter which input path created it
    static void CALLBACK NativeStartWatchdogTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static BOOL CALLBACK NativeStartWatchdogEnumProc(HWND hwnd, LPARAM lParam);

    // Low-level hooks MUST live on a dedicated, otherwise-idle thread. When they
    // share the UI thread, any heavy work there (GDI+ painting, the UIA start-button
    // lookup, search-result handling) delays the hook callback past the system
    // LowLevelHooksTimeout, and Windows then silently passes the input straight to
    // the taskbar. That passthrough is exactly how the native Start menu kept
    // leaking past our swallow on rapid clicks. An idle thread always answers the
    // hook in time, so the Start button / Win key are blocked every single time.
    // This thread handles ONLY the two low-level hooks: the foreground watcher
    // (WinEventProc) was moved to the UI thread because its per-event process
    // verification would otherwise delay these very callbacks.
    static DWORD WINAPI HookThreadProc(LPVOID) {
        g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInst, 0);
        g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, g_hInst, 0);

        // The hook procs are dispatched through this loop, so it must keep pumping.
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Unhook from the same thread that installed the hooks.
        if (g_hKeyboardHook) { UnhookWindowsHookEx(g_hKeyboardHook); g_hKeyboardHook = nullptr; }
        if (g_hMouseHook) { UnhookWindowsHookEx(g_hMouseHook); g_hMouseHook = nullptr; }
        return 0;
    }

    static void Install(HWND targetWnd, HINSTANCE hInst) {
        g_hTargetWnd = targetWnd;
        g_hInst = hInst;

        // Resolve the Start button rectangle once here on the UI thread, where COM
        // (UIA) is already initialized. The hook thread only ever reads the cached
        // value, so it never performs the expensive UIA lookup on the hot path.
        UpdateStartButtonRect();

        g_hHookThread = CreateThread(nullptr, 0, HookThreadProc, nullptr, 0, &g_hookThreadId);

        // The foreground watcher is installed from the UI thread (Install is called
        // on it): WinEventProc performs process image queries per foreground change,
        // and that work must never run on the hook thread. Event delivery is just a
        // posted message to this thread's queue, and the TIMER_BLOCK_START watchdog
        // is the guarantee layer underneath it.
        g_hWinEventHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, WinEventProc, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    static void Uninstall() {
        // WinEvent hook was installed on the calling (UI) thread, so unhook it here.
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
    }
};