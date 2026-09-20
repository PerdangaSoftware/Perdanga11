#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include "resource.h"
#include "MenuState.hpp"

class MenuWindow : public MenuState {
public:
    static void UpdateThemeAttributes(HWND hwnd) {
        BOOL isDark = Config::IsDarkMode();
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDark, sizeof(isDark));

        DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

        int backdrop = 3;
        DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));

        COLORREF borderColor = isDark ? RGB(54, 54, 54) : RGB(215, 215, 220);
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_CREATE: {
                g_hWnd = hwnd;

                g_hFontSearch = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

                MenuRenderer::InitFonts();
                Config::LoadTabs();
                UpdateThemeAttributes(hwnd);

                g_hSearchEdit = CreateWindowExW(
                    0, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
                    78, 28, MENU_WIDTH - 136, 22,
                    hwnd, (HMENU)ID_SEARCH_BOX, GetModuleHandle(nullptr), nullptr
                );

                SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_hFontSearch, TRUE);
                SetWindowSubclass(g_hSearchEdit, SearchEditSubclass, 0, 0);

                SetTimer(hwnd, TIMER_UPDATE_RECT, 2500, nullptr);
                return 0;
            }

            case WM_SETTINGCHANGE:
            case WM_THEMECHANGED: {
                Config::UpdateThemeCache();
                UpdateThemeAttributes(hwnd);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }

            case WM_ERASEBKGND:
                return 1;

            case WM_CHAR: {
                if (wParam >= 32 && GetFocus() != g_hSearchEdit) {
                    SetFocus(g_hSearchEdit);
                    SendMessageW(g_hSearchEdit, WM_CHAR, wParam, lParam);
                    return 0;
                }
                break;
            }

            case WM_MOUSEWHEEL: {
                short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);

                g_showTooltip = false;
                KillTimer(hwnd, TIMER_HOVER_TOOLTIP);

                if (pt.y >= 60 && pt.y <= 104 && pt.x >= 36 && pt.x <= (g_toggleViewRect.left - 8)) {
                    if (g_maxTabScrollX > 0) {
                        g_tabScrollX -= (zDelta > 0 ? 48 : -48);
                        g_tabScrollX = (std::max)(0, (std::min)(g_maxTabScrollX, g_tabScrollX));
                        RecalculateTabLayout();
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                }

                if (g_maxScrollY > 0) {
                    if (zDelta > 0) {
                        g_scrollY -= CELL_HEIGHT;
                    } else {
                        g_scrollY += CELL_HEIGHT;
                    }
                    g_scrollY = (std::max)(0, (std::min)(g_maxScrollY, g_scrollY));
                    InvalidateGridArea();
                }
                return 0;
            }

            case WM_APP_INDEX_READY: {
                ReloadPinnedData();
                return 0;
            }

            case WM_APP_SEARCH_COMPLETE: {
                uint64_t gen = (uint64_t)wParam;
                auto* pResults = (std::vector<AppItem>*)lParam;
                if (pResults) {
                    if (gen == g_searchGeneration.load()) {
                        g_displayItems = std::move(*pResults);
                        int totalRows = ((int)g_displayItems.size() + GRID_COLS - 1) / GRID_COLS;
                        g_maxScrollY = (std::max)(0, (totalRows * CELL_HEIGHT) - VIEWPORT_HEIGHT);
                        g_scrollY = (std::max)(0, (std::min)(g_maxScrollY, g_scrollY));
                        g_hoveredIndex = -1;
                        g_showTooltip = false;
                        InvalidateGridArea();
                    }
                    delete pResults;
                }
                return 0;
            }

            case WM_CTLCOLOREDIT: {
                HDC hdcEdit = (HDC)wParam;
                bool isDark = Config::IsDarkMode();
                SetTextColor(hdcEdit, isDark ? RGB(255, 255, 255) : RGB(20, 20, 20));
                SetBkColor(hdcEdit, isDark ? RGB(38, 38, 38) : RGB(255, 255, 255));
                static HBRUSH hbrDark = CreateSolidBrush(RGB(38, 38, 38));
                static HBRUSH hbrLight = CreateSolidBrush(RGB(255, 255, 255));
                return (LRESULT)(isDark ? hbrDark : hbrLight);
            }

            case WM_TIMER: {
                if (wParam == TIMER_ANIMATION) {
                    StepAnimation();
                } else if (wParam == TIMER_UPDATE_RECT) {
                    Hooks::UpdateStartButtonRect();
                } else if (wParam == TIMER_SEARCH_DEBOUNCE) {
                    KillTimer(hwnd, TIMER_SEARCH_DEBOUNCE);
                    ExecuteAsyncSearch();
                } else if (wParam == TIMER_HOVER_TOOLTIP) {
                    KillTimer(hwnd, TIMER_HOVER_TOOLTIP);
                    if (g_hoveredIndex >= 0 && g_hoveredIndex < (int)g_displayItems.size() && !g_isDragging && !MenuInteraction::g_isPowerMenuOpen) {
                        g_showTooltip = true;
                        g_tooltipItemIndex = g_hoveredIndex;
                        InvalidateGridArea();
                    }
                } else if (wParam == TIMER_LAUNCH_FLASH) {
                    KillTimer(hwnd, TIMER_LAUNCH_FLASH);
                    g_justLaunchedIndex = -1;
                    InvalidateGridArea();
                } else if (wParam == TIMER_RESET_DEACTIVATE) {
                    KillTimer(hwnd, TIMER_RESET_DEACTIVATE);
                    g_suppressCloseOnDeactivate = false;
                }
                return 0;
            }

            case WM_APP_TOGGLE_MENU: {
                Toggle(!g_isVisible);
                return 0;
            }

            case WM_APP_CLOSE_MENU: {
                CloseImmediately();
                return 0;
            }

            case WM_COMMAND: {
                if (LOWORD(wParam) == ID_SEARCH_BOX && HIWORD(wParam) == EN_CHANGE) {
                    g_scrollY = 0;
                    g_showTooltip = false;
                    KillTimer(hwnd, TIMER_HOVER_TOOLTIP);
                    TriggerSearch();
                }
                return 0;
            }

            // Dismiss menu when losing focus, unless an app was just launched via Shift+Click
            case WM_ACTIVATE: {
                if (LOWORD(wParam) == WA_INACTIVE && !g_isDragging && !MenuInteraction::g_isModalDialogOpen) {
                    if (g_suppressCloseOnDeactivate) {
                        return 0;
                    }
                    if (GetTickCount() - g_openTimestamp > 250) {
                        CloseImmediately();
                    }
                }
                return 0;
            }

            case WM_LBUTTONDOWN: {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                POINT pt = { x, y };

                g_showTooltip = false;
                KillTimer(hwnd, TIMER_HOVER_TOOLTIP);

                if (pt.x >= 36 && pt.x <= (MENU_WIDTH - 36) && pt.y >= 18 && pt.y <= 56) {
                    if (GetFocus() != g_hSearchEdit) {
                        SetFocus(g_hSearchEdit);
                    }
                } else {
                    if (GetFocus() == g_hSearchEdit) {
                        SetFocus(hwnd);
                    }
                }

                if (MenuInteraction::g_isPowerMenuOpen) {
                    if (PtInRect(&g_powerFlyoutRect, pt)) {
                        int itemH = 38;
                        int idx = (y - (g_powerFlyoutRect.top + 6)) / itemH;
                        if (idx == 0) { MenuInteraction::DoLock(); CloseImmediately(); }
                        else if (idx == 1) { MenuInteraction::DoSleep(); CloseImmediately(); }
                        else if (idx == 2) { MenuInteraction::DoShutdown(); CloseImmediately(); }
                        else if (idx == 3) { MenuInteraction::DoRestart(); CloseImmediately(); }
                        return 0;
                    } else if (!PtInRect(&g_powerButtonRect, pt)) {
                        MenuInteraction::g_isPowerMenuOpen = false;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }

                if (g_currentMode == MODE_TABS) {
                    wchar_t searchContent[64] = { 0 };
                    GetWindowTextW(g_hSearchEdit, searchContent, 64);

                    int relY = y - GRID_START_Y + g_scrollY;
                    if (wcslen(searchContent) == 0 && x >= GRID_START_X && x < GRID_START_X + (GRID_COLS * CELL_WIDTH) && relY >= 0 && y < (MENU_HEIGHT - 52)) {
                        int col = (x - GRID_START_X) / CELL_WIDTH;
                        int row = relY / CELL_HEIGHT;
                        int idx = row * GRID_COLS + col;
                        const auto& activeTab = Config::GetActiveTab();
                        if (idx >= 0 && idx < (int)activeTab.items.size()) {
                            g_dragPotentialIndex = idx;
                            g_dragStartPt = { x, y };
                            g_isDragging = false;
                            SetCapture(hwnd);
                        }
                    }
                }
                return 0;
            }

            case WM_MOUSEMOVE: {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                POINT pt = { x, y };

                if (MenuInteraction::g_isPowerMenuOpen && PtInRect(&g_powerFlyoutRect, pt)) {
                    int itemH = 38;
                    int idx = (y - (g_powerFlyoutRect.top + 6)) / itemH;
                    if (idx >= 0 && idx < 4 && idx != MenuInteraction::g_powerMenuHoverIndex) {
                        MenuInteraction::g_powerMenuHoverIndex = idx;
                        InvalidateRect(hwnd, &g_powerFlyoutRect, FALSE);
                    }
                    return 0;
                } else if (MenuInteraction::g_powerMenuHoverIndex != -1) {
                    MenuInteraction::g_powerMenuHoverIndex = -1;
                    InvalidateRect(hwnd, &g_powerFlyoutRect, FALSE);
                }

                if (g_dragPotentialIndex != -1) {
                    int dx = abs(x - g_dragStartPt.x);
                    int dy = abs(y - g_dragStartPt.y);

                    if (!g_isDragging && (dx > 5 || dy > 5)) {
                        g_isDragging = true;
                        g_dragItemIndex = g_dragPotentialIndex;
                        g_showTooltip = false;
                        KillTimer(hwnd, TIMER_HOVER_TOOLTIP);
                    }

                    if (g_isDragging) {
                        g_dragCurrentPt = pt;
                        int relY = y - GRID_START_Y + g_scrollY;
                        int col = (std::max)(0, (std::min)(GRID_COLS - 1, (x - GRID_START_X) / CELL_WIDTH));
                        int row = (std::max)(0, relY / CELL_HEIGHT);
                        int targetIdx = row * GRID_COLS + col;
                        const auto& activeTab = Config::GetActiveTab();
                        targetIdx = (std::max)(0, (std::min)((int)activeTab.items.size() - 1, targetIdx));

                        if (targetIdx != g_dragTargetIndex) g_dragTargetIndex = targetIdx;
                        InvalidateGridArea();
                        return 0;
                    }
                }

                if (g_hasTabOverflow) {
                    bool lHov = PtInRect(&g_scrollLeftRect, pt);
                    if (lHov != g_scrollLeftHovered) {
                        g_scrollLeftHovered = lHov;
                        InvalidateRect(hwnd, &g_scrollLeftRect, FALSE);
                    }
                    bool rHov = PtInRect(&g_scrollRightRect, pt);
                    if (rHov != g_scrollRightHovered) {
                        g_scrollRightHovered = rHov;
                        InvalidateRect(hwnd, &g_scrollRightRect, FALSE);
                    }
                }

                bool inTabStrip = (pt.x >= g_tabClipLeft && pt.x <= g_tabClipRight && pt.y >= 60 && pt.y <= 104);

                for (auto& tab : Config::g_tabs) {
                    if (!tab.visible) continue;
                    bool hov = inTabStrip && PtInRect(&tab.rect, pt);
                    if (hov != tab.hovered) {
                        tab.hovered = hov;
                        InvalidateRect(hwnd, &tab.rect, FALSE);
                    }
                }

                bool addHov = inTabStrip && PtInRect(&g_addTabRect, pt);
                if (addHov != g_addTabHovered) { g_addTabHovered = addHov; InvalidateRect(hwnd, &g_addTabRect, FALSE); }

                bool togHov = PtInRect(&g_toggleViewRect, pt);
                if (togHov != g_toggleViewHovered) { g_toggleViewHovered = togHov; InvalidateRect(hwnd, &g_toggleViewRect, FALSE); }

                bool pwrHov = PtInRect(&g_powerButtonRect, pt);
                if (pwrHov != g_powerHovered) { g_powerHovered = pwrHov; InvalidateRect(hwnd, &g_powerButtonRect, FALSE); }

                bool setHov = PtInRect(&g_settingsButtonRect, pt);
                if (setHov != g_settingsHovered) { g_settingsHovered = setHov; InvalidateRect(hwnd, &g_settingsButtonRect, FALSE); }

                bool expHov = PtInRect(&g_explorerButtonRect, pt);
                if (expHov != g_explorerHovered) { g_explorerHovered = expHov; InvalidateRect(hwnd, &g_explorerButtonRect, FALSE); }

                bool dwnHov = PtInRect(&g_downloadsButtonRect, pt);
                if (dwnHov != g_downloadsHovered) { g_downloadsHovered = dwnHov; InvalidateRect(hwnd, &g_downloadsButtonRect, FALSE); }

                int newHover = -1;
                int relY = y - GRID_START_Y + g_scrollY;
                if (x >= GRID_START_X && x < GRID_START_X + (GRID_COLS * CELL_WIDTH) && y >= GRID_START_Y && y <= GRID_START_Y + VIEWPORT_HEIGHT && relY >= 0) {
                    int col = (x - GRID_START_X) / CELL_WIDTH;
                    int row = relY / CELL_HEIGHT;
                    int idx = row * GRID_COLS + col;
                    if (idx >= 0 && idx < (int)g_displayItems.size()) newHover = idx;
                }

                if (newHover != g_hoveredIndex) {
                    g_hoveredIndex = newHover;
                    g_showTooltip = false;
                    KillTimer(hwnd, TIMER_HOVER_TOOLTIP);
                    if (g_hoveredIndex >= 0 && !g_isDragging) {
                        SetTimer(hwnd, TIMER_HOVER_TOOLTIP, 320, nullptr);
                    }
                    InvalidateGridArea();
                }

                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                return 0;
            }

            case WM_MOUSELEAVE: {
                if (!g_isDragging) {
                    g_hoveredIndex = -1;
                    g_showTooltip = false;
                    KillTimer(hwnd, TIMER_HOVER_TOOLTIP);
                    for (auto& tab : Config::g_tabs) tab.hovered = false;
                    g_addTabHovered = false;
                    g_scrollLeftHovered = false;
                    g_scrollRightHovered = false;
                    g_toggleViewHovered = false;
                    g_powerHovered = false;
                    g_settingsHovered = false;
                    g_explorerHovered = false;
                    g_downloadsHovered = false;
                    InvalidateGridArea();
                }
                return 0;
            }

            // Middle Click: Launches app and keeps menu open (convenient alternative)
            case WM_MBUTTONUP: {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                int relY = y - GRID_START_Y + g_scrollY;

                if (x >= GRID_START_X && x < GRID_START_X + (GRID_COLS * CELL_WIDTH) && y >= GRID_START_Y && y <= GRID_START_Y + VIEWPORT_HEIGHT && relY >= 0) {
                    int col = (x - GRID_START_X) / CELL_WIDTH;
                    int row = relY / CELL_HEIGHT;
                    int idx = row * GRID_COLS + col;
                    if (idx >= 0 && idx < (int)g_displayItems.size()) {
                        MenuInteraction::ExecuteApp(g_displayItems[idx], false);
                        g_suppressCloseOnDeactivate = true;
                        g_justLaunchedIndex = idx;
                        g_justLaunchedTime = GetTickCount();
                        SetTimer(hwnd, TIMER_LAUNCH_FLASH, 500, nullptr);
                        SetTimer(hwnd, TIMER_RESET_DEACTIVATE, 600, nullptr);
                        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        SetForegroundWindow(g_hWnd);
                        SetFocus(g_hWnd);
                        InvalidateGridArea();
                    }
                }
                return 0;
            }

            case WM_RBUTTONUP: {
                if (g_isDragging) return 0;
                POINT pt;
                GetCursorPos(&pt);
                POINT clientPt = pt;
                ScreenToClient(hwnd, &clientPt);

                g_showTooltip = false;
                KillTimer(hwnd, TIMER_HOVER_TOOLTIP);

                RECT rc;
                GetClientRect(hwnd, &rc);

                if (clientPt.y >= 60 && clientPt.y <= 104 && clientPt.x >= 36 && clientPt.x <= (g_toggleViewRect.left - 8)) {
                    int clickedTab = -1;
                    if (clientPt.x >= g_tabClipLeft && clientPt.x <= g_tabClipRight) {
                        for (size_t i = 0; i < Config::g_tabs.size(); ++i) {
                            if (Config::g_tabs[i].visible && PtInRect(&Config::g_tabs[i].rect, clientPt)) {
                                clickedTab = (int)i;
                                break;
                            }
                        }
                    }

                    int cmd = MenuInteraction::ShowTabHeaderContextMenu(hwnd, pt, clickedTab);
                    if (cmd == ID_SETTINGS_RENAME_CURRENT && clickedTab >= 0) {
                        std::wstring name = Config::g_tabs[clickedTab].name;
                        const wchar_t* title = Config::IsRussian() ? L"\x041F\x0435\x0440\x0435\x0438\x043C\x0435\x043D\x043E\x0432\x0430\x0442\x044C \x0432\x043A\x043B\x0430\x0434\x043A\x0443" : L"Rename Tab";
                        if (MenuInteraction::PromptTabNameDialog(hwnd, title, name)) {
                            Config::RenameTab(clickedTab, name);
                            ReloadPinnedData();
                        }
                    } else if (cmd == ID_SETTINGS_DELETE_CURRENT && clickedTab >= 0) {
                        Config::DeleteTab(clickedTab);
                        ReloadPinnedData();
                    } else if (cmd == ID_SETTINGS_ADD_TAB) {
                        std::wstring newName = Config::IsRussian() ? L"\x041D\x043E\x0432\x0430\x044F \x0432\x043A\x043B\x0430\x0434\x043A\x0430" : L"New Tab";
                        const wchar_t* title = Config::IsRussian() ? L"\x0421\x043E\x0437\x0434\x0430\x0442\x044C \x043D\x043E\x0432\x0443\x044E \x0432\x043A\x043B\x0430\x0434\x043A\x0443" : L"Create New Tab";
                        if (MenuInteraction::PromptTabNameDialog(hwnd, title, newName)) {
                            Config::AddTab(newName);
                            ReloadPinnedData();
                            EnsureTabVisible(Config::g_activeTabIndex);
                        }
                    } else if (cmd == ID_LANG_AUTO) {
                        Config::SetLanguage(AppLanguage::Auto);
                        ReloadPinnedData();
                    } else if (cmd == ID_LANG_EN) {
                        Config::SetLanguage(AppLanguage::English);
                        ReloadPinnedData();
                    } else if (cmd == ID_LANG_RU) {
                        Config::SetLanguage(AppLanguage::Russian);
                        ReloadPinnedData();
                    }
                    return 0;
                }

                if (clientPt.y >= rc.bottom - 52) {
                    if (PtInRect(&g_powerButtonRect, clientPt)) {
                        MenuInteraction::g_isPowerMenuOpen = !MenuInteraction::g_isPowerMenuOpen;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                }

                if (clientPt.y < 60) {
                    return 0;
                }

                auto& activeTab = Config::GetActiveTab();

                if (g_hoveredIndex >= 0 && g_hoveredIndex < (int)g_displayItems.size()) {
                    g_contextItemIndex = g_hoveredIndex;
                    const AppItem& targetItem = g_displayItems[g_contextItemIndex];

                    int cmd = MenuInteraction::ShowItemContextMenu(hwnd, pt, targetItem, Config::g_activeTabIndex);

                    if (cmd == ID_MENU_PIN_ACTION) {
                        if (Config::IsAppPinned(targetItem.name, activeTab.items)) {
                            Config::DeleteTabItem(activeTab, targetItem.name);
                        } else {
                            Config::SaveTabItem(activeTab, targetItem.name, targetItem.target);
                        }
                        ReloadPinnedData();
                    } else if (cmd >= ID_PIN_TO_TAB_BASE && cmd < ID_PIN_TO_TAB_BASE + 50) {
                        int tabIdx = cmd - ID_PIN_TO_TAB_BASE;
                        if (tabIdx >= 0 && tabIdx < (int)Config::g_tabs.size()) {
                            auto& targetTab = Config::g_tabs[tabIdx];
                            if (Config::IsAppPinned(targetItem.name, targetTab.items)) {
                                Config::DeleteTabItem(targetTab, targetItem.name);
                            } else {
                                Config::SaveTabItem(targetTab, targetItem.name, targetItem.target);
                            }
                            ReloadPinnedData();
                        }
                    } else if (cmd == ID_MENU_RUN_ADMIN) {
                        MenuInteraction::ExecuteApp(targetItem, true);
                        CloseImmediately();
                    } else if (cmd == ID_MENU_OPEN_LOCATION) {
                        MenuInteraction::OpenItemLocation(targetItem);
                        CloseImmediately();
                    }
                } else {
                    int cmd = MenuInteraction::ShowBackgroundContextMenu(hwnd, pt);
                    std::wstring name, path;
                    if (cmd == ID_MENU_PIN_CUSTOM_FILE && MenuInteraction::PromptAddFile(hwnd, name, path)) {
                        Config::SaveTabItem(activeTab, name, path);
                        ReloadPinnedData();
                    } else if (cmd == ID_MENU_PIN_CUSTOM_DIR && MenuInteraction::PromptAddFolder(hwnd, name, path)) {
                        Config::SaveTabItem(activeTab, name, path);
                        ReloadPinnedData();
                    }
                }
                return 0;
            }

            case WM_LBUTTONUP: {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                POINT pt = { x, y };

                ReleaseCapture();
                g_showTooltip = false;
                KillTimer(hwnd, TIMER_HOVER_TOOLTIP);

                if (g_isDragging) {
                    auto& activeTab = Config::GetActiveTab();
                    if (g_dragItemIndex >= 0 && g_dragTargetIndex >= 0 && g_dragItemIndex != g_dragTargetIndex) {
                        AppItem moved = activeTab.items[g_dragItemIndex];
                        activeTab.items.erase(activeTab.items.begin() + g_dragItemIndex);
                        activeTab.items.insert(activeTab.items.begin() + g_dragTargetIndex, moved);
                        Config::SaveAllTabItemsOrder(activeTab, activeTab.items);
                        TriggerSearch();
                    }
                    g_isDragging = false;
                    g_dragPotentialIndex = -1;
                    g_dragItemIndex = -1;
                    g_dragTargetIndex = -1;
                    InvalidateGridArea();
                    return 0;
                }
                g_dragPotentialIndex = -1;

                if (g_hasTabOverflow) {
                    if (PtInRect(&g_scrollLeftRect, pt)) {
                        g_tabScrollX -= 120;
                        g_tabScrollX = (std::max)(0, (std::min)(g_maxTabScrollX, g_tabScrollX));
                        RecalculateTabLayout();
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    }
                    if (PtInRect(&g_scrollRightRect, pt)) {
                        g_tabScrollX += 120;
                        g_tabScrollX = (std::max)(0, (std::min)(g_maxTabScrollX, g_tabScrollX));
                        RecalculateTabLayout();
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    }
                }

                if (pt.x >= g_tabClipLeft && pt.x <= g_tabClipRight && pt.y >= 60 && pt.y <= 104) {
                    for (size_t i = 0; i < Config::g_tabs.size(); ++i) {
                        if (Config::g_tabs[i].visible && PtInRect(&Config::g_tabs[i].rect, pt)) {
                            Config::g_activeTabIndex = (int)i;
                            g_currentMode = MODE_TABS;
                            g_scrollY = 0;
                            EnsureTabVisible((int)i);
                            TriggerSearch();
                            return 0;
                        }
                    }

                    if (PtInRect(&g_addTabRect, pt)) {
                        std::wstring newName = Config::IsRussian() ? L"\x041D\x043E\x0432\x0430\x044F \x0432\x043A\x043B\x0430\x0434\x043A\x0430" : L"New Tab";
                        const wchar_t* title = Config::IsRussian() ? L"\x0421\x043E\x0437\x0434\x0430\x0442\x044C \x043D\x043E\x0432\x0443\x044E \x0432\x043A\x043B\x0430\x0434\x043A\x0443" : L"Create New Tab";
                        if (MenuInteraction::PromptTabNameDialog(hwnd, title, newName)) {
                            Config::AddTab(newName);
                            ReloadPinnedData();
                            EnsureTabVisible(Config::g_activeTabIndex);
                        }
                        return 0;
                    }
                }

                if (PtInRect(&g_toggleViewRect, pt)) {
                    g_currentMode = (g_currentMode == MODE_ALL_APPS) ? MODE_TABS : MODE_ALL_APPS;
                    g_scrollY = 0;
                    TriggerSearch();
                    return 0;
                }

                if (PtInRect(&g_powerButtonRect, pt)) {
                    MenuInteraction::g_isPowerMenuOpen = !MenuInteraction::g_isPowerMenuOpen;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                // Single Click vs Shift+Click logic for footer buttons
                bool isShiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

                if (PtInRect(&g_settingsButtonRect, pt)) {
                    ShellExecuteW(nullptr, L"open", L"ms-settings:", nullptr, nullptr, SW_SHOWNORMAL);
                    if (isShiftHeld) {
                        g_suppressCloseOnDeactivate = true;
                        SetTimer(hwnd, TIMER_RESET_DEACTIVATE, 600, nullptr);
                        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        SetForegroundWindow(g_hWnd);
                    } else {
                        CloseImmediately();
                    }
                    return 0;
                }

                if (PtInRect(&g_explorerButtonRect, pt)) {
                    ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
                    if (isShiftHeld) {
                        g_suppressCloseOnDeactivate = true;
                        SetTimer(hwnd, TIMER_RESET_DEACTIVATE, 600, nullptr);
                        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        SetForegroundWindow(g_hWnd);
                    } else {
                        CloseImmediately();
                    }
                    return 0;
                }

                if (PtInRect(&g_downloadsButtonRect, pt)) {
                    ShellExecuteW(nullptr, L"open", L"shell:Downloads", nullptr, nullptr, SW_SHOWNORMAL);
                    if (isShiftHeld) {
                        g_suppressCloseOnDeactivate = true;
                        SetTimer(hwnd, TIMER_RESET_DEACTIVATE, 600, nullptr);
                        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        SetForegroundWindow(g_hWnd);
                    } else {
                        CloseImmediately();
                    }
                    return 0;
                }

                // Normal Click: Launches and closes menu immediately
                // Shift + Click: Launches program, flashes tile, and keeps menu open for consecutive launches
                if (g_hoveredIndex >= 0 && g_hoveredIndex < (int)g_displayItems.size()) {
                    MenuInteraction::ExecuteApp(g_displayItems[g_hoveredIndex], false);
                    if (isShiftHeld) {
                        g_suppressCloseOnDeactivate = true;
                        g_justLaunchedIndex = g_hoveredIndex;
                        g_justLaunchedTime = GetTickCount();
                        SetTimer(hwnd, TIMER_LAUNCH_FLASH, 500, nullptr);
                        SetTimer(hwnd, TIMER_RESET_DEACTIVATE, 600, nullptr);
                        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        SetForegroundWindow(g_hWnd);
                        SetFocus(g_hWnd);
                        InvalidateGridArea();
                    } else {
                        CloseImmediately();
                    }
                    return 0;
                }
                return 0;
            }

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                RECT rc;
                GetClientRect(hwnd, &rc);

                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
                HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

                Gdiplus::Graphics g(memDC);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

                MenuRenderer::DrawBackground(g, rc);

                wchar_t searchContent[64] = { 0 };
                GetWindowTextW(g_hSearchEdit, searchContent, 64);
                MenuRenderer::DrawSearchBar(g, rc, wcslen(searchContent) > 0, GetFocus() == g_hSearchEdit);

                RecalculateTabLayout();

                MenuRenderer::DrawSubHeader(g, rc, Config::g_tabs, Config::g_activeTabIndex,
                                            g_tabScrollX, g_maxTabScrollX, g_hasTabOverflow,
                                            g_tabClipLeft, g_tabClipRight,
                                            g_scrollLeftRect, g_scrollLeftHovered,
                                            g_scrollRightRect, g_scrollRightHovered,
                                            g_addTabRect, g_addTabHovered,
                                            g_toggleViewRect, g_currentMode == MODE_ALL_APPS, g_toggleViewHovered);

                if (g_displayItems.empty()) {
                    MenuRenderer::DrawEmptyCard(g, rc);
                } else {
                    MenuRenderer::DrawGrid(g, memDC, rc, g_displayItems,
                                           GRID_START_X, GRID_START_Y, GRID_COLS, CELL_WIDTH, CELL_HEIGHT,
                                           VIEWPORT_HEIGHT, g_scrollY, g_hoveredIndex,
                                           g_isDragging, g_dragItemIndex, g_dragTargetIndex,
                                           g_justLaunchedIndex, g_justLaunchedTime);

                    MenuRenderer::DrawScrollBar(g, rc, GRID_START_Y, VIEWPORT_HEIGHT, g_scrollY, g_maxScrollY);

                    const auto& activeTab = Config::GetActiveTab();
                    if (g_isDragging && g_dragItemIndex >= 0 && g_dragItemIndex < (int)activeTab.items.size()) {
                        MenuRenderer::DrawFloatingDragItem(g, memDC, activeTab.items[g_dragItemIndex], g_dragCurrentPt, CELL_WIDTH, CELL_HEIGHT);
                    }

                    if (g_showTooltip && g_tooltipItemIndex >= 0 && g_tooltipItemIndex < (int)g_displayItems.size() && !g_isDragging) {
                        int col = g_tooltipItemIndex % GRID_COLS;
                        int row = g_tooltipItemIndex / GRID_COLS;
                        int tLeft = GRID_START_X + (col * CELL_WIDTH);
                        int tTop = GRID_START_Y + (row * CELL_HEIGHT) - g_scrollY;
                        MenuRenderer::DrawItemPathTooltip(g, g_displayItems[g_tooltipItemIndex], tLeft, tTop, CELL_WIDTH, CELL_HEIGHT, rc.right);
                    }
                }

                int footerY = rc.bottom - 52;
                g_powerButtonRect     = { rc.right - 62,  footerY + 6, rc.right - 24,  footerY + 44 };
                g_settingsButtonRect  = { rc.right - 106, footerY + 6, rc.right - 68,  footerY + 44 };
                g_explorerButtonRect  = { rc.right - 150, footerY + 6, rc.right - 112, footerY + 44 };
                g_downloadsButtonRect = { rc.right - 194, footerY + 6, rc.right - 156, footerY + 44 };

                MenuRenderer::DrawFooterBar(g, rc,
                                            g_powerButtonRect, g_powerHovered,
                                            g_settingsButtonRect, g_settingsHovered,
                                            g_explorerButtonRect, g_explorerHovered,
                                            g_downloadsButtonRect, g_downloadsHovered);

                if (MenuInteraction::g_isPowerMenuOpen) {
                    g_powerFlyoutRect = { rc.right - 236, footerY - 168, rc.right - 24, footerY - 4 };
                    MenuRenderer::DrawPowerFlyoutUpward(g, g_powerFlyoutRect, MenuInteraction::g_powerMenuHoverIndex);
                }

                BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

                SelectObject(memDC, oldBitmap);
                DeleteObject(memBitmap);
                DeleteDC(memDC);
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_DESTROY: {
                KillTimer(hwnd, TIMER_ANIMATION);
                KillTimer(hwnd, TIMER_UPDATE_RECT);
                KillTimer(hwnd, TIMER_SEARCH_DEBOUNCE);
                KillTimer(hwnd, TIMER_HOVER_TOOLTIP);
                KillTimer(hwnd, TIMER_LAUNCH_FLASH);
                KillTimer(hwnd, TIMER_RESET_DEACTIVATE);
                if (g_hFontSearch) DeleteObject(g_hFontSearch);

                MenuRenderer::FreeFonts();
                PostQuitMessage(0);
                return 0;
            }
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    static HWND Create(HINSTANCE hInstance) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Perdanga11WindowClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hbrBackground = nullptr;
        RegisterClassExW(&wc);

        HWND hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            wc.lpszClassName,
            L"Perdanga11",
            WS_POPUP,
            0, 0, MENU_WIDTH, MENU_HEIGHT,
            nullptr, nullptr, hInstance, nullptr
        );

        UpdateThemeAttributes(hwnd);

        return hwnd;
    }
};

inline LRESULT CALLBACK Hooks::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;
        POINT pt = pMouse->pt;

        // Dismiss immediately if user clicks anywhere outside the Start menu window
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_NCLBUTTONDOWN || wParam == WM_NCRBUTTONDOWN) {
            if (MenuWindow::g_isVisible) {
                if (MenuInteraction::g_isModalDialogOpen) {
                    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
                }

                RECT menuRect = { MenuWindow::g_targetX, MenuWindow::g_targetY, 
                                  MenuWindow::g_targetX + MenuWindow::MENU_WIDTH, 
                                  MenuWindow::g_targetY + MenuWindow::MENU_HEIGHT };

                if (!PtInRect(&menuRect, pt)) {
                    RECT startHit = g_startButtonRect;
                    startHit.left -= 4;
                    startHit.top -= 4;
                    startHit.bottom += 4;

                    if (PtInRect(&startHit, pt) && IsCursorDirectlyOnTaskbar(pt)) {
                        g_mouseClickIntercepted = true;
                        MenuWindow::CloseImmediately();
                        return 1;
                    }

                    MenuWindow::CloseImmediately();
                }
            }
        }

        if (IsCursorDirectlyOnTaskbar(pt) && !IsForegroundFullScreen(pt)) {
            RECT hitArea = g_startButtonRect;
            hitArea.left -= 4;
            hitArea.top -= 4;
            hitArea.bottom += 4;

            if (wParam == WM_LBUTTONDOWN || wParam == WM_NCLBUTTONDOWN) {
                if (PtInRect(&hitArea, pt)) {
                    g_mouseClickIntercepted = true;
                    PostMessageW(g_hTargetWnd, WM_APP_TOGGLE_MENU, 0, 0);
                    return 1;
                }
            } else if (wParam == WM_LBUTTONUP || wParam == WM_NCLBUTTONUP) {
                if (g_mouseClickIntercepted || PtInRect(&hitArea, pt)) {
                    g_mouseClickIntercepted = false;
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}