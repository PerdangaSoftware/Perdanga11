#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ole2.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "Config.hpp"
#include "AppIndexer.hpp"

class MenuRenderer {
public:
    static inline Gdiplus::FontFamily* g_pFontDisplay = nullptr;
    static inline Gdiplus::FontFamily* g_pFontText = nullptr;
    static inline Gdiplus::FontFamily* g_pFontSmall = nullptr;
    static inline Gdiplus::FontFamily* g_pGlyphFamily = nullptr;

    static inline Gdiplus::Font* g_pTitleFont = nullptr;
    static inline Gdiplus::Font* g_pTabInactiveFont = nullptr;
    static inline Gdiplus::Font* g_pTileFont = nullptr;
    static inline Gdiplus::Font* g_pAllAppsFont = nullptr;
    static inline Gdiplus::Font* g_pHintFont = nullptr;
    static inline Gdiplus::Font* g_pTooltipFont = nullptr;
    static inline Gdiplus::Font* g_pTooltipBoldFont = nullptr;
    static inline Gdiplus::Font* g_pMenuFont = nullptr;
    static inline Gdiplus::Font* g_pGlyphFont = nullptr;
    static inline Gdiplus::Font* g_pTooltipGlyphFont = nullptr;
    static inline Gdiplus::Font* g_pFooterGlyphFont = nullptr;

    static void InitFonts() {
        g_pFontDisplay = new Gdiplus::FontFamily(L"Segoe UI Variable Display");
        if (!g_pFontDisplay->IsAvailable()) {
            delete g_pFontDisplay;
            g_pFontDisplay = new Gdiplus::FontFamily(L"Segoe UI");
        }

        g_pFontText = new Gdiplus::FontFamily(L"Segoe UI Variable Text");
        if (!g_pFontText->IsAvailable()) {
            delete g_pFontText;
            g_pFontText = new Gdiplus::FontFamily(L"Segoe UI");
        }

        g_pFontSmall = new Gdiplus::FontFamily(L"Segoe UI Variable Small");
        if (!g_pFontSmall->IsAvailable()) {
            delete g_pFontSmall;
            g_pFontSmall = new Gdiplus::FontFamily(L"Segoe UI");
        }

        g_pGlyphFamily = new Gdiplus::FontFamily(L"Segoe Fluent Icons");
        if (!g_pGlyphFamily->IsAvailable()) {
            delete g_pGlyphFamily;
            g_pGlyphFamily = new Gdiplus::FontFamily(L"Segoe MDL2 Assets");
        }

        g_pTitleFont = new Gdiplus::Font(g_pFontDisplay, 12.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        g_pTabInactiveFont = new Gdiplus::Font(g_pFontDisplay, 11.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        g_pAllAppsFont = new Gdiplus::Font(g_pFontDisplay, 10.2f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

        g_pTileFont = new Gdiplus::Font(g_pFontText, 9.6f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        g_pHintFont = new Gdiplus::Font(g_pFontText, 10.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        g_pMenuFont = new Gdiplus::Font(g_pFontText, 10.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

        g_pTooltipFont = new Gdiplus::Font(g_pFontSmall, 8.8f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        g_pTooltipBoldFont = new Gdiplus::Font(g_pFontSmall, 9.2f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

        g_pGlyphFont = new Gdiplus::Font(g_pGlyphFamily, 11.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        g_pTooltipGlyphFont = new Gdiplus::Font(g_pGlyphFamily, 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        g_pFooterGlyphFont = new Gdiplus::Font(g_pGlyphFamily, 13.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    }

    static void FreeFonts() {
        delete g_pTitleFont;
        delete g_pTabInactiveFont;
        delete g_pAllAppsFont;
        delete g_pTileFont;
        delete g_pHintFont;
        delete g_pMenuFont;
        delete g_pTooltipFont;
        delete g_pTooltipBoldFont;
        delete g_pGlyphFont;
        delete g_pTooltipGlyphFont;
        delete g_pFooterGlyphFont;

        delete g_pFontDisplay;
        delete g_pFontText;
        delete g_pFontSmall;
        delete g_pGlyphFamily;
    }

    static void AddRoundedRectToPath(Gdiplus::GraphicsPath& path, Gdiplus::RectF rect, float radius) {
        float d = radius * 2.0f;
        path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
        path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
        path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
        path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
        path.CloseFigure();
    }

    static Gdiplus::Color GetThemeBgColor() {
        return Config::IsDarkMode() ? Gdiplus::Color(255, 32, 32, 32) : Gdiplus::Color(255, 243, 243, 246);
    }
    static Gdiplus::Color GetSearchBgColor() {
        return Config::IsDarkMode() ? Gdiplus::Color(255, 38, 38, 38) : Gdiplus::Color(255, 255, 255, 255);
    }
    static Gdiplus::Color GetCardBgColor() {
        return Config::IsDarkMode() ? Gdiplus::Color(255, 42, 42, 42) : Gdiplus::Color(255, 255, 255, 255);
    }
    static Gdiplus::Color GetTextPrimaryColor() {
        return Config::IsDarkMode() ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 25, 25, 25);
    }
    static Gdiplus::Color GetTextSecondaryColor() {
        return Config::IsDarkMode() ? Gdiplus::Color(255, 160, 160, 160) : Gdiplus::Color(255, 115, 115, 122);
    }
    static Gdiplus::Color GetHoverColor() {
        return Config::IsDarkMode() ? Gdiplus::Color(255, 50, 50, 50) : Gdiplus::Color(255, 230, 230, 234);
    }
    static Gdiplus::Color GetBorderColor() {
        return Config::IsDarkMode() ? Gdiplus::Color(255, 52, 52, 52) : Gdiplus::Color(160, 215, 215, 220);
    }

    static void DrawBackground(Gdiplus::Graphics& g, const RECT& rc) {
        Gdiplus::SolidBrush bgBrush(GetThemeBgColor());
        g.FillRectangle(&bgBrush, 0, 0, rc.right, rc.bottom);
    }

    static void DrawSearchBar(Gdiplus::Graphics& g, const RECT& rc, bool hasSearchText, bool editHasFocus) {
        float padX = 36.0f;
        float padY = 18.0f;
        float h = 38.0f;
        Gdiplus::RectF searchRect(padX, padY, (float)rc.right - padX * 2.0f, h);
        Gdiplus::GraphicsPath searchPath;
        AddRoundedRectToPath(searchPath, searchRect, h / 2.0f);

        Gdiplus::SolidBrush searchBg(GetSearchBgColor());
        g.FillPath(&searchBg, &searchPath);

        Gdiplus::Color borderCol = editHasFocus
            ? (Config::IsDarkMode() ? Gdiplus::Color(220, 0, 120, 215) : Gdiplus::Color(220, 0, 103, 192))
            : (Config::IsDarkMode() ? Gdiplus::Color(180, 56, 56, 56) : Gdiplus::Color(160, 215, 215, 220));

        Gdiplus::Pen searchPen(borderCol, editHasFocus ? 1.4f : 1.0f);
        g.DrawPath(&searchPen, &searchPath);

        float magD = 11.5f;
        float magX = 52.0f;
        float magY = searchRect.Y + (searchRect.Height - magD) / 2.0f;

        Gdiplus::Pen magPen(Gdiplus::Color(255, 0, 0, 0), 1.8f);
        magPen.SetStartCap(Gdiplus::LineCapRound);
        magPen.SetEndCap(Gdiplus::LineCapRound);

        g.DrawEllipse(&magPen, magX, magY, magD, magD);

        float r = magD / 2.0f;
        float cx = magX + r;
        float cy = magY + r;
        float startX = cx + r * 0.7071f;
        float startY = cy + r * 0.7071f;
        float handleLen = 4.5f;
        g.DrawLine(&magPen, startX, startY, startX + handleLen, startY + handleLen);
    }

    static void DrawSubHeader(Gdiplus::Graphics& g, const RECT& rc,
                              std::vector<TabDefinition>& tabs, int activeIndex,
                              int tabScrollX, int maxTabScrollX, bool hasTabOverflow,
                              int tabClipLeft, int tabClipRight,
                              const RECT& scrollLeftRect, bool scrollLeftHovered,
                              const RECT& scrollRightRect, bool scrollRightHovered,
                              const RECT& addTabRect, bool addTabHovered,
                              const RECT& toggleRect, bool isAllAppsMode, bool toggleHov) {

        Gdiplus::StringFormat centerFormat;
        centerFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        centerFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        centerFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        centerFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        if (hasTabOverflow && scrollLeftRect.right > scrollLeftRect.left) {
            bool canScrollLeft = (tabScrollX > 0);
            Gdiplus::RectF leftF((float)scrollLeftRect.left, (float)scrollLeftRect.top, (float)(scrollLeftRect.right - scrollLeftRect.left), (float)(scrollLeftRect.bottom - scrollLeftRect.top));
            Gdiplus::SolidBrush arrBrush(canScrollLeft ? (scrollLeftHovered ? GetTextPrimaryColor() : GetTextSecondaryColor()) : Gdiplus::Color(60, 128, 128, 128));
            g.DrawString(L"<", -1, g_pTitleFont, leftF, &centerFormat, &arrBrush);
        }

        if (hasTabOverflow && scrollRightRect.right > scrollRightRect.left) {
            bool canScrollRight = (tabScrollX < maxTabScrollX);
            Gdiplus::RectF rightF((float)scrollRightRect.left, (float)scrollRightRect.top, (float)(scrollRightRect.right - scrollRightRect.left), (float)(scrollRightRect.bottom - scrollRightRect.top));
            Gdiplus::SolidBrush arrBrush(canScrollRight ? (scrollRightHovered ? GetTextPrimaryColor() : GetTextSecondaryColor()) : Gdiplus::Color(60, 128, 128, 128));
            g.DrawString(L">", -1, g_pTitleFont, rightF, &centerFormat, &arrBrush);
        }

        for (size_t i = 0; i < tabs.size(); ++i) {
            if (!tabs[i].visible) continue;

            bool active = (!isAllAppsMode && (int)i == activeIndex);
            bool hovered = tabs[i].hovered;

            Gdiplus::RectF rectF((float)tabs[i].rect.left, (float)tabs[i].rect.top,
                                (float)(tabs[i].rect.right - tabs[i].rect.left),
                                (float)(tabs[i].rect.bottom - tabs[i].rect.top));

            if (rectF.X + rectF.Width < (float)tabClipLeft || rectF.X > (float)tabClipRight) continue;

            std::wstring label = Config::GetTabDisplayName(tabs[i]);

            Gdiplus::Color textColor;
            if (active) {
                textColor = GetTextPrimaryColor();
            } else if (hovered) {
                textColor = Config::IsDarkMode() ? Gdiplus::Color(255, 235, 235, 235) : Gdiplus::Color(255, 30, 30, 30);
            } else {
                textColor = GetTextSecondaryColor();
            }

            Gdiplus::SolidBrush textBrush(textColor);
            g.DrawString(label.c_str(), -1, active ? g_pTitleFont : g_pTabInactiveFont, rectF, &centerFormat, &textBrush);
        }

        if (addTabRect.right > addTabRect.left) {
            Gdiplus::RectF addRectF((float)addTabRect.left, (float)addTabRect.top, (float)(addTabRect.right - addTabRect.left), (float)(addTabRect.bottom - addTabRect.top));
            Gdiplus::SolidBrush addGlyphBrush(addTabHovered ? GetTextPrimaryColor() : GetTextSecondaryColor());
            g.DrawString(L"+", -1, g_pTitleFont, addRectF, &centerFormat, &addGlyphBrush);
        }

        Gdiplus::RectF togRectF((float)toggleRect.left, (float)toggleRect.top, (float)(toggleRect.right - toggleRect.left), (float)(toggleRect.bottom - toggleRect.top));

        const wchar_t* btnText = isAllAppsMode
            ? (Config::IsRussian() ? L"< \x041D\x0430\x0437\x0430\x0434" : L"< Back")
            : (Config::IsRussian() ? L"\x0412\x0441\x0435 \x043F\x0440\x0438\x043B\x043E\x0436\x0435\x043D\x0438\x044F >" : L"All apps >");

        Gdiplus::SolidBrush toggleBtnBrush(toggleHov ? GetTextPrimaryColor() : GetTextSecondaryColor());
        g.DrawString(btnText, -1, g_pAllAppsFont, togRectF, &centerFormat, &toggleBtnBrush);
    }

    static void DrawFooterBar(Gdiplus::Graphics& g, const RECT& rc,
                              const RECT& pwrRect, bool pwrHovered,
                              const RECT& setRect, bool setHovered,
                              const RECT& expRect, bool expHovered,
                              const RECT& dwnRect, bool dwnHovered) {
        float footerY = (float)rc.bottom - 52.0f;

        Gdiplus::Pen divPen(GetBorderColor(), 1.0f);
        g.DrawLine(&divPen, 0.0f, footerY, (float)rc.right, footerY);

        Gdiplus::StringFormat centerFormat;
        centerFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        centerFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        auto drawFooterButton = [&](const RECT& r, bool hovered, const wchar_t* glyph) {
            Gdiplus::RectF btnRect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top));
            if (hovered) {
                Gdiplus::GraphicsPath path;
                AddRoundedRectToPath(path, btnRect, 8.0f);
                Gdiplus::SolidBrush hovBrush(GetHoverColor());
                g.FillPath(&hovBrush, &path);
            }
            Gdiplus::SolidBrush glyphBrush(hovered ? GetTextPrimaryColor() : GetTextSecondaryColor());
            g.DrawString(glyph, -1, g_pFooterGlyphFont, btnRect, &centerFormat, &glyphBrush);
        };

        drawFooterButton(dwnRect, dwnHovered, L"\xE896");
        drawFooterButton(expRect, expHovered, L"\xED25");
        drawFooterButton(setRect, setHovered, L"\xE713");
        drawFooterButton(pwrRect, pwrHovered, L"\xE7E8");
    }

    static void DrawPowerFlyoutUpward(Gdiplus::Graphics& g, const RECT& flyoutRect, int hoverIndex) {
        Gdiplus::RectF flyoutRectF((float)flyoutRect.left, (float)flyoutRect.top, (float)(flyoutRect.right - flyoutRect.left), (float)(flyoutRect.bottom - flyoutRect.top));

        Gdiplus::GraphicsPath shadowPath;
        Gdiplus::RectF shadowRectF = flyoutRectF;
        shadowRectF.Inflate(2.0f, 2.0f);
        AddRoundedRectToPath(shadowPath, shadowRectF, 12.0f);
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(100, 0, 0, 0));
        g.FillPath(&shadowBrush, &shadowPath);

        Gdiplus::GraphicsPath cardPath;
        AddRoundedRectToPath(cardPath, flyoutRectF, 10.0f);
        Gdiplus::SolidBrush cardBg(GetCardBgColor());
        Gdiplus::Pen cardBorder(GetBorderColor(), 1.0f);
        g.FillPath(&cardBg, &cardPath);
        g.DrawPath(&cardBorder, &cardPath);

        struct PowerItem {
            const wchar_t* glyph;
            const wchar_t* textRu;
            const wchar_t* textEn;
        };

        const PowerItem items[] = {
            { L"\xE72E", L"\x0411\x043B\x043E\x043A\x0438\x0440\x043E\x0432\x043A\x0430", L"Lock" },
            { L"\xE708", L"\x0421\x043F\x044F\x0449\x0438\x0439 \x0440\x0435\x0436\x0438\x043C", L"Sleep" },
            { L"\xE7E8", L"\x0417\x0430\x0432\x0435\x0440\x0448\x0435\x043D\x0438\x0435 \x0440\x0430\x0431\x043E\x0442\x044B", L"Shut down" },
            { L"\xE777", L"\x041F\x0435\x0440\x0435\x0437\x0430\x0433\x0440\x0435\x0437\x043A\x0430", L"Restart" }
        };

        float itemH = 38.0f;
        float startY = flyoutRectF.Y + 6.0f;

        Gdiplus::StringFormat glyphFormat;
        glyphFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        glyphFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::StringFormat textFormat;
        textFormat.SetAlignment(Gdiplus::StringAlignmentNear);
        textFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::SolidBrush textBrush(GetTextPrimaryColor());

        for (int i = 0; i < 4; ++i) {
            Gdiplus::RectF itemRect(flyoutRectF.X + 6.0f, startY + (i * itemH), flyoutRectF.Width - 12.0f, itemH);

            if (i == hoverIndex) {
                Gdiplus::GraphicsPath hoverPill;
                AddRoundedRectToPath(hoverPill, itemRect, 6.0f);
                Gdiplus::SolidBrush hoverBrush(GetHoverColor());
                g.FillPath(&hoverBrush, &hoverPill);
            }

            Gdiplus::RectF iconRect(itemRect.X + 6.0f, itemRect.Y, 24.0f, itemRect.Height);
            g.DrawString(items[i].glyph, -1, g_pGlyphFont, iconRect, &glyphFormat, &textBrush);

            const wchar_t* txt = Config::IsRussian() ? items[i].textRu : items[i].textEn;
            Gdiplus::RectF labelRect(itemRect.X + 36.0f, itemRect.Y, itemRect.Width - 40.0f, itemRect.Height);
            g.DrawString(txt, -1, g_pMenuFont, labelRect, &textFormat, &textBrush);
        }
    }

    static void DrawEmptyCard(Gdiplus::Graphics& g, const RECT& rc) {
        Gdiplus::RectF emptyCard(48.0f, 170.0f, (float)rc.right - 96.0f, 180.0f);
        Gdiplus::GraphicsPath emptyPath;
        AddRoundedRectToPath(emptyPath, emptyCard, 12.0f);

        Gdiplus::SolidBrush cardBg(GetCardBgColor());
        Gdiplus::Pen cardBorder(GetBorderColor(), 1.0f);
        g.FillPath(&cardBg, &emptyPath);
        g.DrawPath(&cardBorder, &emptyPath);

        Gdiplus::StringFormat centerFormat;
        centerFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        centerFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::SolidBrush primaryBrush(GetTextPrimaryColor());
        Gdiplus::RectF label1(emptyCard.X, emptyCard.Y + 48, emptyCard.Width, 26);
        Gdiplus::RectF label2(emptyCard.X, emptyCard.Y + 80, emptyCard.Width, 40);

        const wchar_t* title = Config::IsRussian() ? L"\x042D\x043B\x0435\x043C\x0435\x043D\x0442\x044B \x043D\x0430\x0439\x0434\x0435\x043D\x044B" : L"No items found";
        const wchar_t* hint = Config::IsRussian()
            ? L"\x041D\x0430\x0436\x043C\x0438\x0442\x0435 \x043F\x0440\x0430\x0432\x043E\x0439 \x043A\x043D\x043E\x043F\x043A\x043E\x0439 \x043C\x044B\x0448\x0438, \x0447\x0442\x043E\x0431\x044B \x0437\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x044D\x043B\x0435\x043C\x0435\x043D\x0442\x044B"
            : L"Right-click anywhere to pin items or create custom tabs";

        g.DrawString(title, -1, g_pTitleFont, label1, &centerFormat, &primaryBrush);
        Gdiplus::SolidBrush hintBrush(GetTextSecondaryColor());
        g.DrawString(hint, -1, g_pHintFont, label2, &centerFormat, &hintBrush);
    }

    // Grid rendering with tactile launch flash and clean 2-line clamping
    static void DrawGrid(Gdiplus::Graphics& g, HDC memDC, const RECT& rc,
                         const std::vector<AppItem>& items,
                         int gridStartX, int gridStartY, int gridCols, int cellWidth, int cellHeight,
                         int viewportHeight, int scrollY, int hoveredIndex,
                         bool isDragging, int dragItemIndex, int dragTargetIndex,
                         int justLaunchedIdx, DWORD justLaunchedTime) {

        int savedDC = SaveDC(memDC);
        HRGN hGdiClipRgn = CreateRectRgn(gridStartX - 4, gridStartY, rc.right - 16, gridStartY + viewportHeight);
        SelectClipRgn(memDC, hGdiClipRgn);
        DeleteObject(hGdiClipRgn);

        Gdiplus::RectF clipRectF((float)gridStartX - 4.0f, (float)gridStartY, (float)(rc.right - 16 - (gridStartX - 4)), (float)viewportHeight);
        g.SetClip(clipRectF);

        Gdiplus::StringFormat tileFormat;
        tileFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        tileFormat.SetLineAlignment(Gdiplus::StringAlignmentNear);
        tileFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        tileFormat.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);

        DWORD now = GetTickCount();

        for (size_t i = 0; i < items.size(); ++i) {
            int col = (int)i % gridCols;
            int row = (int)i / gridCols;

            int tileLeft = gridStartX + (col * cellWidth);
            int tileTop = gridStartY + (row * cellHeight) - scrollY;
            int tileBottom = tileTop + cellHeight;

            if (tileBottom < gridStartY - 5 || tileTop > gridStartY + viewportHeight + 5) continue;

            Gdiplus::RectF tileRect((float)tileLeft + 2.0f, (float)tileTop + 2.0f, (float)cellWidth - 4.0f, (float)cellHeight - 4.0f);

            if (isDragging && (int)i == dragItemIndex) {
                Gdiplus::GraphicsPath slotPath;
                AddRoundedRectToPath(slotPath, tileRect, 9.0f);
                Gdiplus::Pen slotPen(GetTextSecondaryColor(), 1.5f);
                slotPen.SetDashStyle(Gdiplus::DashStyleDash);
                g.DrawPath(&slotPen, &slotPath);
                continue;
            }

            if (isDragging && (int)i == dragTargetIndex) {
                Gdiplus::GraphicsPath targetGlow;
                AddRoundedRectToPath(targetGlow, tileRect, 9.0f);
                Gdiplus::SolidBrush glowBrush(Gdiplus::Color(60, 100, 160, 255));
                Gdiplus::Pen glowPen(Gdiplus::Color(200, 120, 180, 255), 1.5f);
                g.FillPath(&glowBrush, &targetGlow);
                g.DrawPath(&glowPen, &targetGlow);
            } else if ((int)i == justLaunchedIdx && (now - justLaunchedTime < 500)) {
                // Tactile launch flash feedback
                Gdiplus::GraphicsPath launchPath;
                AddRoundedRectToPath(launchPath, tileRect, 9.0f);
                Gdiplus::SolidBrush flashBg(Gdiplus::Color(65, 0, 120, 215));
                Gdiplus::Pen flashPen(Gdiplus::Color(180, 0, 140, 255), 1.2f);
                g.FillPath(&flashBg, &launchPath);
                g.DrawPath(&flashPen, &launchPath);
            } else if ((int)i == hoveredIndex && !isDragging) {
                Gdiplus::GraphicsPath tilePath;
                AddRoundedRectToPath(tilePath, tileRect, 9.0f);

                if (Config::IsDarkMode()) {
                    Gdiplus::LinearGradientBrush hovFill(
                        tileRect,
                        Gdiplus::Color(32, 255, 255, 255),
                        Gdiplus::Color(16, 255, 255, 255),
                        Gdiplus::LinearGradientModeVertical
                    );
                    g.FillPath(&hovFill, &tilePath);

                    Gdiplus::LinearGradientBrush hovBorder(
                        tileRect,
                        Gdiplus::Color(55, 255, 255, 255),
                        Gdiplus::Color(18, 255, 255, 255),
                        Gdiplus::LinearGradientModeVertical
                    );
                    Gdiplus::Pen borderPen(&hovBorder, 1.0f);
                    g.DrawPath(&borderPen, &tilePath);
                } else {
                    Gdiplus::SolidBrush hovFill(Gdiplus::Color(15, 0, 0, 0));
                    g.FillPath(&hovFill, &tilePath);

                    Gdiplus::Pen borderPen(Gdiplus::Color(35, 0, 0, 0), 1.0f);
                    g.DrawPath(&borderPen, &tilePath);
                }
            }

            // Draw crisp 32x32 icon at exact 1:1 pixel mapping
            if (items[i].hIcon) {
                int iconX = tileLeft + (cellWidth - 32) / 2;
                int iconY = tileTop + 8;
                DrawIconEx(memDC, iconX, iconY, items[i].hIcon, 32, 32, 0, (HBRUSH)NULL, DI_NORMAL);
            }

            bool isItemHovered = ((int)i == hoveredIndex && !isDragging);
            Gdiplus::Color labelColor = isItemHovered ? Gdiplus::Color(255, 255, 255, 255) : GetTextPrimaryColor();

            // Height = 38.0f: perfectly accommodates exactly two lines of text with clean ellipsis
            Gdiplus::RectF labelRect((float)tileLeft + 4.0f, (float)tileTop + 44.0f, (float)cellWidth - 8.0f, 38.0f);
            Gdiplus::SolidBrush labelBrush(labelColor);
            g.DrawString(items[i].name.c_str(), -1, g_pTileFont, labelRect, &tileFormat, &labelBrush);
        }

        g.ResetClip();
        RestoreDC(memDC, savedDC);
    }

    static void DrawScrollBar(Gdiplus::Graphics& g, const RECT& rc, int gridStartY, int viewportHeight, int scrollY, int maxScrollY) {
        if (maxScrollY <= 0) return;

        float scrollTrackHeight = (float)viewportHeight;
        float thumbHeight = (std::max)(32.0f, scrollTrackHeight * scrollTrackHeight / (scrollTrackHeight + maxScrollY));
        float thumbY = (float)gridStartY + ((float)scrollY / (float)maxScrollY) * (scrollTrackHeight - thumbHeight);

        Gdiplus::RectF scrollThumbRect((float)rc.right - 14.0f, thumbY, 4.0f, thumbHeight);
        Gdiplus::GraphicsPath thumbPath;
        AddRoundedRectToPath(thumbPath, scrollThumbRect, 2.0f);
        Gdiplus::Color thumbColor = Config::IsDarkMode() ? Gdiplus::Color(70, 255, 255, 255) : Gdiplus::Color(70, 0, 0, 0);
        Gdiplus::SolidBrush thumbBrush(thumbColor);
        g.FillPath(&thumbBrush, &thumbPath);
    }

    static void DrawFloatingDragItem(Gdiplus::Graphics& g, HDC memDC, const AppItem& item, POINT currentPt, int cellWidth, int cellHeight) {
        float dragX = (float)currentPt.x - cellWidth / 2.0f;
        float dragY = (float)currentPt.y - 24.0f;
        Gdiplus::RectF dragRect(dragX, dragY, (float)cellWidth, (float)cellHeight);

        Gdiplus::GraphicsPath dragCard;
        AddRoundedRectToPath(dragCard, dragRect, 9.0f);
        Gdiplus::Color cardColor = Config::IsDarkMode() ? Gdiplus::Color(230, 48, 48, 48) : Gdiplus::Color(230, 255, 255, 255);
        Gdiplus::SolidBrush dragCardBrush(cardColor);
        Gdiplus::Pen dragCardPen(Gdiplus::Color(255, 0, 120, 215), 1.5f);
        g.FillPath(&dragCardBrush, &dragCard);
        g.DrawPath(&dragCardPen, &dragCard);

        if (item.hIcon) {
            int iconX = (int)dragX + (cellWidth - 32) / 2;
            int iconY = (int)dragY + 8;
            DrawIconEx(memDC, iconX, iconY, item.hIcon, 32, 32, 0, (HBRUSH)NULL, DI_NORMAL);
        }

        Gdiplus::StringFormat tileFormat;
        tileFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        tileFormat.SetLineAlignment(Gdiplus::StringAlignmentNear);
        tileFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        tileFormat.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);

        Gdiplus::RectF dragLabelRect(dragX + 4.0f, dragY + 44.0f, (float)cellWidth - 8.0f, 38.0f);
        Gdiplus::SolidBrush textBrush(GetTextPrimaryColor());
        g.DrawString(item.name.c_str(), -1, g_pTileFont, dragLabelRect, &tileFormat, &textBrush);
    }

    static std::wstring ResolveItemContainingDirectory(const AppItem& item) {
        std::wstring path = item.target;
        if (path.length() >= 2 && path.front() == L'"' && path.back() == L'"') {
            path = path.substr(1, path.length() - 2);
        }

        if (item.isDirectory) {
            return path;
        }

        size_t dotPos = path.find_last_of(L'.');
        if (dotPos != std::wstring::npos && _wcsicmp(path.c_str() + dotPos, L".lnk") == 0) {
            std::wstring iconPath;
            int iconIdx = 0;
            std::wstring realTarget = AppIndexer::ResolveLnkTarget(path, iconPath, iconIdx);
            if (!realTarget.empty() && realTarget != path) {
                path = realTarget;
            }
        }

        path = Config::ResolveAppPath(path);

        size_t slash = path.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            return path.substr(0, slash);
        }
        return path;
    }

    static void DrawItemPathTooltip(Gdiplus::Graphics& g, const AppItem& item, int tileX, int tileY, int cellWidth, int cellHeight, int menuWidth) {
        std::wstring displayLocation = ResolveItemContainingDirectory(item);
        if (displayLocation.empty()) return;

        bool hasLongTitle = (item.name.length() > 16);

        Gdiplus::RectF layoutRect(0.0f, 0.0f, 340.0f, 40.0f);
        Gdiplus::RectF pathBox;
        Gdiplus::StringFormat measureFormat;
        measureFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        measureFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisPath);
        g.MeasureString(displayLocation.c_str(), -1, g_pTooltipFont, layoutRect, &measureFormat, &pathBox);

        Gdiplus::RectF titleBox(0.0f, 0.0f, 0.0f, 0.0f);
        if (hasLongTitle) {
            Gdiplus::StringFormat titleMeasureFmt;
            titleMeasureFmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            titleMeasureFmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            g.MeasureString(item.name.c_str(), -1, g_pTooltipBoldFont, layoutRect, &titleMeasureFmt, &titleBox);
        }

        float contentW = (std::max)(pathBox.Width + 34.0f, titleBox.Width + 24.0f);
        float tipW = (std::min)(360.0f, (std::max)(110.0f, contentW));
        float tipH = hasLongTitle ? 44.0f : 26.0f;

        float tipX = (float)tileX + ((float)cellWidth - tipW) / 2.0f;
        if (tipX < 24.0f) tipX = 24.0f;
        if (tipX + tipW > (float)menuWidth - 24.0f) tipX = (float)menuWidth - 24.0f - tipW;

        float tipY = (float)tileY - tipH - 6.0f;
        if (tipY < 106.0f) {
            tipY = (float)tileY + (float)cellHeight + 6.0f;
        }

        Gdiplus::RectF tipRect(tipX, tipY, tipW, tipH);

        Gdiplus::RectF shadowRect = tipRect;
        shadowRect.Offset(0.0f, 1.5f);
        shadowRect.Inflate(1.5f, 1.5f);
        Gdiplus::GraphicsPath shadowPath;
        AddRoundedRectToPath(shadowPath, shadowRect, 7.0f);
        Gdiplus::SolidBrush shadowBrush(Config::IsDarkMode() ? Gdiplus::Color(70, 0, 0, 0) : Gdiplus::Color(25, 0, 0, 0));
        g.FillPath(&shadowBrush, &shadowPath);

        Gdiplus::GraphicsPath cardPath;
        AddRoundedRectToPath(cardPath, tipRect, 6.0f);
        Gdiplus::Color cardBgColor = Config::IsDarkMode() 
            ? Gdiplus::Color(248, 44, 44, 44) 
            : Gdiplus::Color(250, 255, 255, 255);
        Gdiplus::SolidBrush cardBrush(cardBgColor);
        g.FillPath(&cardBrush, &cardPath);

        Gdiplus::Color cardBorderColor = Config::IsDarkMode() 
            ? Gdiplus::Color(180, 68, 68, 68) 
            : Gdiplus::Color(180, 215, 215, 220);
        Gdiplus::Pen cardPen(cardBorderColor, 1.0f);
        g.DrawPath(&cardPen, &cardPath);

        Gdiplus::StringFormat singleLineFmt;
        singleLineFmt.SetAlignment(Gdiplus::StringAlignmentNear);
        singleLineFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        singleLineFmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        if (hasLongTitle) {
            singleLineFmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            Gdiplus::RectF nameBox(tipX + 10.0f, tipY + 3.0f, tipW - 20.0f, 18.0f);
            Gdiplus::SolidBrush nameBrush(GetTextPrimaryColor());
            g.DrawString(item.name.c_str(), -1, g_pTooltipBoldFont, nameBox, &singleLineFmt, &nameBrush);

            Gdiplus::StringFormat centerFmt;
            centerFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            centerFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF iconBox(tipX + 8.0f, tipY + 22.0f, 14.0f, 18.0f);
            Gdiplus::SolidBrush iconBrush(GetTextSecondaryColor());
            g.DrawString(L"\xED25", -1, g_pTooltipGlyphFont, iconBox, &centerFmt, &iconBrush);

            singleLineFmt.SetTrimming(Gdiplus::StringTrimmingEllipsisPath);
            Gdiplus::RectF pathRow(tipX + 24.0f, tipY + 22.0f, tipW - 32.0f, 18.0f);
            Gdiplus::SolidBrush pathBrush(GetTextSecondaryColor());
            g.DrawString(displayLocation.c_str(), -1, g_pTooltipFont, pathRow, &singleLineFmt, &pathBrush);
        } else {
            Gdiplus::StringFormat centerFmt;
            centerFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            centerFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF iconBox(tipX + 6.0f, tipY, 16.0f, tipH);
            Gdiplus::SolidBrush iconBrush(GetTextSecondaryColor());
            g.DrawString(L"\xED25", -1, g_pTooltipGlyphFont, iconBox, &centerFmt, &iconBrush);

            singleLineFmt.SetTrimming(Gdiplus::StringTrimmingEllipsisPath);
            Gdiplus::RectF textBox(tipX + 24.0f, tipY, tipW - 30.0f, tipH);
            Gdiplus::SolidBrush textBrush(GetTextPrimaryColor());
            g.DrawString(displayLocation.c_str(), -1, g_pTooltipFont, textBox, &singleLineFmt, &textBrush);
        }
    }
};