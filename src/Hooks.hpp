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
    static inline HWINEVENTHOOK g_hWinEventShowHook = nullptr;
    static inline std::wstring g_lastSearchItemName = L"";
    static inline DWORD g_lastSearchItemTick = 0;
    static inline IUIAutomation* g_pUIAutomation = nullptr;
    static inline HANDLE g_hStartWatcherThread = nullptr;
    static inline bool g_startWatcherRunning = false;
    static inline HWND g_hTargetWnd = nullptr;
    static inline RECT g_startButtonRect = { 0, 0, 0, 0 };
    static inline RECT g_searchButtonRect = { 0, 0, 0, 0 };
    static inline RECT g_lastTaskbarRect = { 0, 0, 0, 0 };
    static inline DWORD g_lastUpdateTick = 0;
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

        if (IsNativeStartWindow(hwnd)) {
            ShowWindow(hwnd, SW_HIDE);
            SetWindowPos(hwnd, nullptr, -32000, -32000, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        }
        return TRUE;
    }

    static void DismissNativeShellOverlays() {
        HWND hFore = GetForegroundWindow();
        if (hFore) {
            if (IsNativeStartWindow(hFore)) {
                ShowWindow(hFore, SW_HIDE);
                SetWindowPos(hFore, nullptr, -32000, -32000, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
                keybd_event(VK_ESCAPE, 0, 0, 0);
                keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
            } else if (IsNativeSearchWindow(hFore)) {
                keybd_event(VK_ESCAPE, 0, 0, 0);
                keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
            }
        }

        EnumWindows(HideShellOverlayEnumProc, 0);
    }

    static void UpdateStartButtonRect(bool force = false) {
        HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!hTaskbar) return;

        DWORD now = GetTickCount();
        RECT rcCurrentTaskbar = { 0 };
        GetWindowRect(hTaskbar, &rcCurrentTaskbar);

        if (!force && (now - g_lastUpdateTick < 1000) &&
            EqualRect(&rcCurrentTaskbar, &g_lastTaskbarRect) &&
            (g_startButtonRect.right - g_startButtonRect.left) > 0) {
            return;
        }

        g_lastTaskbarRect = rcCurrentTaskbar;
        g_lastUpdateTick = now;

        APPBARDATA abd = { sizeof(APPBARDATA) };
        abd.hWnd = hTaskbar;
        if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
            g_taskbarEdge = abd.uEdge;
        } else {
            HMONITOR hMon = MonitorFromWindow(hTaskbar, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(MONITORINFO) };
            if (GetMonitorInfoW(hMon, &mi)) {
                if (rcCurrentTaskbar.left <= mi.rcMonitor.left && rcCurrentTaskbar.right >= mi.rcMonitor.right) {
                    g_taskbarEdge = (rcCurrentTaskbar.top <= mi.rcMonitor.top) ? ABE_TOP : ABE_BOTTOM;
                } else {
                    g_taskbarEdge = (rcCurrentTaskbar.left <= mi.rcMonitor.left) ? ABE_LEFT : ABE_RIGHT;
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

        bool needCoUninit = false;
        HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (hrCom == S_OK || hrCom == S_FALSE) {
            needCoUninit = true;
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
                                bool isUiaStale = false;
                                int btnWidth = rect.right - rect.left;
                                int barWidth = rcCurrentTaskbar.right - rcCurrentTaskbar.left;
                                int distFromLeft = rect.left - rcCurrentTaskbar.left;

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
                        pStartBtn->Release();
                    }
                    pCond->Release();
                }

                VARIANT vSearchBtn, vSearchBox;
                vSearchBtn.vt = VT_BSTR;
                vSearchBtn.bstrVal = SysAllocString(L"SearchButton");
                vSearchBox.vt = VT_BSTR;
                vSearchBox.bstrVal = SysAllocString(L"SearchTextBox");

                IUIAutomationCondition* pCond1 = nullptr;
                IUIAutomationCondition* pCond2 = nullptr;
                IUIAutomationCondition* pSearchCond = nullptr;

                pAutomation->CreatePropertyCondition(UIA_AutomationIdPropertyId, vSearchBtn, &pCond1);
                pAutomation->CreatePropertyCondition(UIA_AutomationIdPropertyId, vSearchBox, &pCond2);
                SysFreeString(vSearchBtn.bstrVal);
                SysFreeString(vSearchBox.bstrVal);

                if (pCond1 && pCond2) {
                    pAutomation->CreateOrCondition(pCond1, pCond2, &pSearchCond);
                } else if (pCond1) {
                    pSearchCond = pCond1;
                    pCond1 = nullptr;
                } else if (pCond2) {
                    pSearchCond = pCond2;
                    pCond2 = nullptr;
                }

                if (pCond1) pCond1->Release();
                if (pCond2) pCond2->Release();

                if (pSearchCond) {
                    IUIAutomationElement* pSearchElem = nullptr;
                    if (SUCCEEDED(pTaskbarElem->FindFirst(TreeScope_Descendants, pSearchCond, &pSearchElem)) && pSearchElem) {
                        tagRECT rcSearch;
                        if (SUCCEEDED(pSearchElem->get_CurrentBoundingRectangle(&rcSearch))) {
                            if ((rcSearch.right - rcSearch.left) > 0 && (rcSearch.bottom - rcSearch.top) > 0) {
                                g_searchButtonRect = rcSearch;
                            }
                        }
                        pSearchElem->Release();
                    }
                    pSearchCond->Release();
                }

                pTaskbarElem->Release();
            }
            pAutomation->Release();
        }

        if (needCoUninit) {
            CoUninitialize();
        }

        if (!startButtonResolved) {
            if (g_taskbarEdge == ABE_BOTTOM || g_taskbarEdge == ABE_TOP) {
                int btnSize = rcCurrentTaskbar.bottom - rcCurrentTaskbar.top;
                if (btnSize <= 0) btnSize = 48;

                if (g_taskbarAlignment == 1) {
                    int centerX = (rcCurrentTaskbar.left + rcCurrentTaskbar.right) / 2;
                    int startLeft = centerX - (btnSize * 4);
                    if (startLeft < rcCurrentTaskbar.left + btnSize) startLeft = rcCurrentTaskbar.left + btnSize;
                    g_startButtonRect = { startLeft, rcCurrentTaskbar.top, startLeft + btnSize, rcCurrentTaskbar.bottom };
                } else {
                    g_startButtonRect = { rcCurrentTaskbar.left, rcCurrentTaskbar.top, rcCurrentTaskbar.left + btnSize, rcCurrentTaskbar.bottom };
                }
            } else {
                int btnSize = rcCurrentTaskbar.right - rcCurrentTaskbar.left;
                if (btnSize <= 0) btnSize = 48;

                if (g_taskbarAlignment == 1) {
                    int centerY = (rcCurrentTaskbar.top + rcCurrentTaskbar.bottom) / 2;
                    int startTop = centerY - (btnSize * 4);
                    if (startTop < rcCurrentTaskbar.top + btnSize) startTop = rcCurrentTaskbar.top + btnSize;
                    g_startButtonRect = { rcCurrentTaskbar.left, startTop, rcCurrentTaskbar.right, startTop + btnSize };
                } else {
                    g_startButtonRect = { rcCurrentTaskbar.left, rcCurrentTaskbar.top, rcCurrentTaskbar.right, rcCurrentTaskbar.top + btnSize };
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
                        keybd_event(0xE8, 0, 0, 0);
                        keybd_event(0xE8, 0, KEYEVENTF_KEYUP, 0);
                        PostMessageW(g_hTargetWnd, WM_APP_TOGGLE_MENU, 0, 0);
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
    static DWORD WINAPI StartMenuWatcherThreadProc(LPVOID);

    static std::wstring CleanAppName(std::wstring name) {
        size_t dash = name.find(L" - ");
        if (dash != std::wstring::npos) name = name.substr(0, dash);
        size_t nl = name.find_first_of(L"\r\n\t");
        if (nl != std::wstring::npos) name = name.substr(0, nl);

        if (name.find(L',') != std::wstring::npos) {
            std::vector<std::wstring> parts;
            size_t start = 0;
            while (start < name.size()) {
                size_t c = name.find(L',', start);
                if (c == std::wstring::npos) c = name.size();
                std::wstring p = name.substr(start, c - start);
                while (!p.empty() && p.front() == L' ') p.erase(p.begin());
                while (!p.empty() && p.back() == L' ') p.pop_back();
                if (!p.empty()) parts.push_back(p);
                start = c + 1;
            }
            std::wstring candidate = L"";
            for (const auto& p : parts) {
                std::wstring lp = Config::ToLower(p);
                if (lp == L"\x043F\x0440\x0438\x043B\x043E\x0436\x0435\x043D\x0438\x0435" || lp == L"application" || lp == L"app" ||
                    lp == L"\x0441\x0438\x0441\x0442\x0435\x043C\x043D\x044B\x0435" || lp == L"system" ||
                    lp.find(L"\x0433\x0440\x0443\x043F\x043F\x0430") != std::wstring::npos || lp.find(L"group") != std::wstring::npos ||
                    lp.find(L"\x0440\x0435\x043A\x043E\x043C\x0435\x043D\x0434\x0430\x0446\x0438\x0438") != std::wstring::npos || lp.find(L"recommendations") != std::wstring::npos) {
                    continue;
                }
                candidate = p;
                break;
            }
            if (!candidate.empty()) name = candidate;
            else if (!parts.empty()) name = parts[0];
        }

        while (!name.empty() && name.back() == L' ') name.pop_back();
        while (!name.empty() && name.front() == L' ') name.erase(name.begin());
        return name;
    }

    static bool IsPinToStartAction(const std::wstring& name) {
        std::wstring lower = Config::ToLower(name);
        for (auto& ch : lower) {
            if (ch == L'&' || ch == 0x00A0) ch = L' ';
        }
        if (lower.find(L"\x0437\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C") != std::wstring::npos && 
           (lower.find(L"\x044D\x043A\x0440\x0430\x043D") != std::wstring::npos || lower.find(L"\x043D\x0430\x0447\x0430\x043B\x044C\x043D") != std::wstring::npos)) {
            return true;
        }
        if (lower.find(L"pin") != std::wstring::npos && lower.find(L"start") != std::wstring::npos && lower.find(L"unpin") == std::wstring::npos) {
            return true;
        }
        return false;
    }

    static bool IsUnpinFromStartAction(const std::wstring& name) {
        std::wstring lower = Config::ToLower(name);
        for (auto& ch : lower) {
            if (ch == L'&' || ch == 0x00A0) ch = L' ';
        }
        if (lower.find(L"\x043E\x0442\x043A\x0440\x0435\x043F\x0438\x0442\x044C") != std::wstring::npos && 
           (lower.find(L"\x044D\x043A\x0440\x0430\x043D") != std::wstring::npos || lower.find(L"\x043D\x0430\x0447\x0430\x043B\x044C\x043D") != std::wstring::npos)) {
            return true;
        }
        if (lower.find(L"unpin") != std::wstring::npos && lower.find(L"start") != std::wstring::npos) {
            return true;
        }
        return false;
    }

    static std::wstring GetSearchElementAtPoint(HWND hwndSearch, POINT pt) {
        HDESK hDesk = OpenDesktopW(L"Default", 0, FALSE, GENERIC_ALL);
        if (hDesk) {
            SetThreadDesktop(hDesk);
            CloseDesktop(hDesk);
        }
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool needUninit = (hrCo == S_OK || hrCo == S_FALSE);

        IUIAutomation* pAutomation = nullptr;
        if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation)) || !pAutomation) {
            if (needUninit) CoUninitialize();
            return L"";
        }

        std::wstring bestName = L"";

        if (!hwndSearch) {
            hwndSearch = WindowFromPoint(pt);
            if (hwndSearch) {
                HWND hRoot = GetAncestor(hwndSearch, GA_ROOT);
                if (hRoot) hwndSearch = hRoot;
            }
        }

        if (hwndSearch) {
            IUIAutomationElement* pSearchElem = nullptr;
            if (SUCCEEDED(pAutomation->ElementFromHandle(hwndSearch, &pSearchElem)) && pSearchElem) {
                IUIAutomationCondition* pCond = nullptr;
                pAutomation->CreateTrueCondition(&pCond);
                if (pCond) {
                    IUIAutomationElementArray* pArr = nullptr;
                    int smallestArea = 99999999;
                    if (SUCCEEDED(pSearchElem->FindAll(TreeScope_Descendants, pCond, &pArr)) && pArr) {
                        int len = 0;
                        pArr->get_Length(&len);
                        for (int i = 0; i < len; ++i) {
                            IUIAutomationElement* pItem = nullptr;
                            if (SUCCEEDED(pArr->GetElement(i, &pItem)) && pItem) {
                                RECT rc = {};
                                if (SUCCEEDED(pItem->get_CurrentBoundingRectangle(&rc)) && PtInRect(&rc, pt)) {
                                    BSTR bName = nullptr;
                                    pItem->get_CurrentName(&bName);
                                    int area = (rc.right - rc.left) * (rc.bottom - rc.top);
                                    if (bName && SysStringLen(bName) > 0) {
                                        std::wstring s = bName;
                                        if (s != L"CoreInput" && s != L"Поиск" && s != L"Search" && s != L"Контекстное меню" && s != L"Context Menu") {
                                            if (area > 0 && area < smallestArea) {
                                                smallestArea = area;
                                                bestName = s;
                                            }
                                        }
                                    }
                                    if (bName) SysFreeString(bName);
                                }
                                pItem->Release();
                            }
                        }
                        pArr->Release();
                    }
                    pCond->Release();
                }
                pSearchElem->Release();
            }
        }

        if (bestName.empty()) {
            IUIAutomationElement* pElem = nullptr;
            if (SUCCEEDED(pAutomation->ElementFromPoint(pt, &pElem)) && pElem) {
                BSTR bn = nullptr;
                pElem->get_CurrentName(&bn);
                if (bn && SysStringLen(bn) > 0) {
                    std::wstring s = bn;
                    if (s != L"CoreInput" && s != L"Поиск" && s != L"Search") bestName = s;
                }
                if (bn) SysFreeString(bn);
                pElem->Release();
            }
        }

        pAutomation->Release();
        if (needUninit) CoUninitialize();
        return bestName;
    }

    static DWORD WINAPI HookThreadProc(LPVOID) {
        HDESK hDefaultDesk = OpenDesktopW(L"Default", 0, FALSE, GENERIC_ALL);
        if (hDefaultDesk) {
            SetThreadDesktop(hDefaultDesk);
            CloseDesktop(hDefaultDesk);
        }
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&g_pUIAutomation);

        g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInst, 0);
        g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, g_hInst, 0);

        HANDLE hWaitEvents[1] = { g_hRegChangeEvent };
        DWORD dwCount = g_hRegChangeEvent ? 1 : 0;
        bool bRunning = true;

        while (bRunning) {
            DWORD dwWait = MsgWaitForMultipleObjectsEx(dwCount, hWaitEvents, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE);

            if (dwWait == WAIT_OBJECT_0) {
                UpdateStartButtonRect(true);
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
        if (g_pUIAutomation) {
            g_pUIAutomation->Release();
            g_pUIAutomation = nullptr;
        }

        CoUninitialize();
        return 0;
    }

    static void Install(HWND targetWnd, HINSTANCE hInst) {
        g_startWatcherRunning = true;
        g_hStartWatcherThread = CreateThread(nullptr, 0, StartMenuWatcherThreadProc, nullptr, 0, nullptr);
        g_hTargetWnd = targetWnd;
        g_hInst = hInst;

        RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
            0, KEY_READ | KEY_NOTIFY, &g_hKeyAdvanced);

        g_hRegChangeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (g_hKeyAdvanced && g_hRegChangeEvent) {
            RegNotifyChangeKeyValue(g_hKeyAdvanced, FALSE, REG_NOTIFY_CHANGE_LAST_SET, g_hRegChangeEvent, TRUE);
        }

        UpdateStartButtonRect(true);

        g_hHookThread = CreateThread(nullptr, 0, HookThreadProc, nullptr, 0, &g_hookThreadId);

        g_hWinEventHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, WinEventProc, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

        g_hWinEventShowHook = SetWinEventHook(
            EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,
            nullptr, WinEventProc, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    static void Uninstall() {
        g_startWatcherRunning = false;
        if (g_hStartWatcherThread) {
            WaitForSingleObject(g_hStartWatcherThread, 1500);
            CloseHandle(g_hStartWatcherThread);
            g_hStartWatcherThread = nullptr;
        }
        if (g_hWinEventHook) { UnhookWinEvent(g_hWinEventHook); g_hWinEventHook = nullptr; }
        if (g_hWinEventShowHook) { UnhookWinEvent(g_hWinEventShowHook); g_hWinEventShowHook = nullptr; }
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
