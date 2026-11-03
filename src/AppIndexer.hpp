#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>

class IconCache {
private:
    static inline std::unordered_map<std::wstring, HICON> s_extCache;
    static inline std::unordered_map<std::wstring, HICON> s_fileIconCache;
    static inline std::unordered_map<std::wstring, HICON> s_shortcutIconCache;
    static inline std::mutex s_cacheMutex;
    static inline HICON s_defaultFolderIcon = nullptr;
    static inline HICON s_defaultFileIcon = nullptr;

public:
    static void Clear() {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        for (auto& pair : s_fileIconCache) {
            if (pair.second) DestroyIcon(pair.second);
        }
        s_fileIconCache.clear();

        for (auto& pair : s_shortcutIconCache) {
            if (pair.second) DestroyIcon(pair.second);
        }
        s_shortcutIconCache.clear();

        for (auto& pair : s_extCache) {
            if (pair.second) DestroyIcon(pair.second);
        }
        s_extCache.clear();

        if (s_defaultFolderIcon) {
            DestroyIcon(s_defaultFolderIcon);
            s_defaultFolderIcon = nullptr;
        }
        s_defaultFileIcon = nullptr;
    }

    static HICON GetDefaultFolderIcon() {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        if (!s_defaultFolderIcon) {
            SHFILEINFOW sfi = { 0 };
            if (SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)) {
                s_defaultFolderIcon = sfi.hIcon;
            }
        }
        return s_defaultFolderIcon;
    }

    static HICON GetDriveIcon(const std::wstring& path) {
        std::wstring norm = Config::ToLower(path);
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            auto it = s_fileIconCache.find(norm);
            if (it != s_fileIconCache.end() && it->second) {
                return it->second;
            }
        }

        SHFILEINFOW sfi = { 0 };
        if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) && sfi.hIcon) {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            s_fileIconCache[norm] = sfi.hIcon;
            return sfi.hIcon;
        }

        return GetDefaultFolderIcon();
    }

    static HICON GetDefaultFileIcon() {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        if (!s_defaultFileIcon) {
            s_defaultFileIcon = LoadIconW(nullptr, IDI_APPLICATION);
        }
        return s_defaultFileIcon;
    }

    static HICON GetExtensionIcon(const std::wstring& lowerExt) {
        if (lowerExt.empty()) return GetDefaultFileIcon();

        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_extCache.find(lowerExt);
        if (it != s_extCache.end() && it->second) {
            return it->second;
        }

        SHFILEINFOW sfi = { 0 };
        if (SHGetFileInfoW(lowerExt.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES) && sfi.hIcon) {
            s_extCache[lowerExt] = sfi.hIcon;
            return sfi.hIcon;
        }

        return GetDefaultFileIcon();
    }

    static HICON GetCachedFileIcon(const std::wstring& path) {
        std::wstring norm = Config::ToLower(path);
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            auto it = s_fileIconCache.find(norm);
            if (it != s_fileIconCache.end() && it->second) {
                return it->second;
            }
        }

        HICON hExtracted = nullptr;
        ExtractIconExW(path.c_str(), 0, &hExtracted, nullptr, 1);
        if (hExtracted) {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            s_fileIconCache[norm] = hExtracted;
            return hExtracted;
        }

        size_t dotPos = norm.find_last_of(L'.');
        std::wstring ext = (dotPos != std::wstring::npos) ? norm.substr(dotPos) : L"";
        return GetExtensionIcon(ext);
    }

    static HICON GetShellItemIcon(const std::wstring& path) {
        std::wstring norm = Config::ToLower(path);
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            auto it = s_fileIconCache.find(norm);
            if (it != s_fileIconCache.end() && it->second) {
                return it->second;
            }
        }

        IShellItem* pItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&pItem))) && pItem) {
            IShellItemImageFactory* pFactory = nullptr;
            if (SUCCEEDED(pItem->QueryInterface(IID_PPV_ARGS(&pFactory))) && pFactory) {
                HBITMAP hBmp = nullptr;
                SIZE sz = { 32, 32 };
                if (SUCCEEDED(pFactory->GetImage(sz, SIIGBF_ICONONLY, &hBmp)) && hBmp) {
                    BITMAP bm = {};
                    GetObject(hBmp, sizeof(bm), &bm);
                    HBITMAP hMono = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, nullptr);
                    ICONINFO ii = {};
                    ii.fIcon = TRUE;
                    ii.hbmMask = hMono;
                    ii.hbmColor = hBmp;
                    HICON hIcon = CreateIconIndirect(&ii);
                    DeleteObject(hMono);
                    DeleteObject(hBmp);
                    pFactory->Release();
                    pItem->Release();
                    if (hIcon) {
                        std::lock_guard<std::mutex> lock(s_cacheMutex);
                        s_fileIconCache[norm] = hIcon;
                        return hIcon;
                    }
                }
                pFactory->Release();
            }
            pItem->Release();
        }
        return GetDefaultFileIcon();
    }

    static HICON GetShortcutIcon(const std::wstring& iconPath, int iconIndex) {
        std::wstring key = Config::ToLower(iconPath) + L"|" + std::to_wstring(iconIndex);
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            auto it = s_shortcutIconCache.find(key);
            if (it != s_shortcutIconCache.end()) {
                return it->second;
            }
        }

        HICON hExtracted = nullptr;
        ExtractIconExW(iconPath.c_str(), iconIndex, &hExtracted, nullptr, 1);

        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_shortcutIconCache[key] = hExtracted;
        return hExtracted;
    }
};

class AppIndexer {
public:
    static HICON GetDefaultFolderIcon() {
        return IconCache::GetDefaultFolderIcon();
    }

    static std::wstring ResolveLnkTarget(const std::wstring& lnkPath, std::wstring& outIconPath, int& outIconIndex, std::wstring* outArgs = nullptr) {
        IShellLinkW* psl = nullptr;
        std::wstring targetPath = lnkPath;
        outIconPath.clear();
        outIconIndex = 0;
        if (outArgs) outArgs->clear();

        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
            IPersistFile* ppf = nullptr;
            if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
                if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
                    wchar_t szTarget[MAX_PATH] = { 0 };
                    WIN32_FIND_DATAW wfd;
                    if (SUCCEEDED(psl->GetPath(szTarget, MAX_PATH, &wfd, SLGP_UNCPRIORITY))) {
                        if (szTarget[0] != L'\0') targetPath = szTarget;
                    }
                    if (targetPath == lnkPath || szTarget[0] == L'\0') {
                        if (SUCCEEDED(psl->GetPath(szTarget, MAX_PATH, &wfd, SLGP_RAWPATH))) {
                            if (szTarget[0] != L'\0') targetPath = szTarget;
                        }
                    }
                    if (targetPath == lnkPath || szTarget[0] == L'\0') {
                        PIDLIST_ABSOLUTE pidl = nullptr;
                        if (SUCCEEDED(psl->GetIDList(&pidl)) && pidl) {
                            if (SHGetPathFromIDListW(pidl, szTarget) && szTarget[0] != L'\0') {
                                targetPath = szTarget;
                            }
                            CoTaskMemFree(pidl);
                        }
                    }

                    wchar_t szIcon[MAX_PATH] = { 0 };
                    int nIcon = 0;
                    if (SUCCEEDED(psl->GetIconLocation(szIcon, MAX_PATH, &nIcon))) {
                        if (szIcon[0] != L'\0') {
                            outIconPath = szIcon;
                            outIconIndex = nIcon;
                        }
                    }

                    if (outArgs) {
                        wchar_t szArgs[MAX_PATH] = { 0 };
                        if (SUCCEEDED(psl->GetArguments(szArgs, MAX_PATH)) && szArgs[0] != L'\0') {
                            *outArgs = szArgs;
                        }
                    }
                }
                ppf->Release();
            }
            psl->Release();
        }
        return targetPath;
    }

    static std::wstring ResolveItemRealTargetPath(const std::wstring& rawTarget) {
        std::wstring path = rawTarget;
        if (path.length() >= 2 && path.front() == L'"' && path.back() == L'"') {
            path = path.substr(1, path.length() - 2);
        }
        if (path.rfind(L"shell:AppsFolder\\", 0) == 0) {
            return path;
        }
        size_t dotPos = path.find_last_of(L'.');
        if (dotPos != std::wstring::npos && _wcsicmp(path.c_str() + dotPos, L".lnk") == 0) {
            std::wstring iconPath;
            int iconIdx = 0;
            std::wstring realTarget = ResolveLnkTarget(path, iconPath, iconIdx);
            if (!realTarget.empty() && realTarget != path) {
                path = realTarget;
            }
        }
        wchar_t expanded[MAX_PATH];
        if (ExpandEnvironmentStringsW(path.c_str(), expanded, MAX_PATH)) {
            path = expanded;
        }
        std::wstring resolved = Config::ResolveAppPath(path);
        if (!resolved.empty()) {
            path = resolved;
        }
        return path;
    }

    static std::wstring ResolveItemRealTargetPath(const AppItem& item) {
        if (item.isDirectory) return item.target;
        return ResolveItemRealTargetPath(item.target);
    }

    static HICON ExtractCleanIcon(const std::wstring& path, bool isDir = false) {
        if (isDir) {
            if (path.length() <= 3 && path.find(L':') != std::wstring::npos) {
                return IconCache::GetDriveIcon(path);
            }
            return IconCache::GetDefaultFolderIcon();
        }

        std::wstring targetPath = path;
        std::wstring iconPath;
        int iconIndex = 0;

        wchar_t expandedInput[MAX_PATH];
        if (ExpandEnvironmentStringsW(path.c_str(), expandedInput, MAX_PATH)) {
            targetPath = expandedInput;
        }

        size_t dotPos = targetPath.find_last_of(L'.');
        std::wstring ext = (dotPos != std::wstring::npos) ? Config::ToLower(targetPath.substr(dotPos)) : L"";

        if (ext == L".lnk") {
            targetPath = ResolveLnkTarget(targetPath, iconPath, iconIndex);
            dotPos = targetPath.find_last_of(L'.');
            ext = (dotPos != std::wstring::npos) ? Config::ToLower(targetPath.substr(dotPos)) : L"";
        }

        if (!iconPath.empty()) {
            HICON hShortcutIcon = IconCache::GetShortcutIcon(iconPath, iconIndex);
            if (hShortcutIcon) return hShortcutIcon;
        }

        if (targetPath.rfind(L"shell:", 0) == 0) {
            return IconCache::GetShellItemIcon(targetPath);
        }

        if (ext == L".exe") {
            if (targetPath.find(L'\\') == std::wstring::npos) {
                std::wstring res = Config::ResolveAppPath(targetPath);
                if (!res.empty()) targetPath = res;
            }
            return IconCache::GetCachedFileIcon(targetPath);
        }

        if (!ext.empty()) {
            return IconCache::GetExtensionIcon(ext);
        }

        return IconCache::GetDefaultFileIcon();
    }

    static std::wstring CleanAppName(const std::wstring& rawName) {
        std::wstring s = rawName;
        for (auto& c : s) {
            if (c == 0x00A0) c = L' ';
        }
        while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
        while (!s.empty() && iswspace(s.back())) s.pop_back();

        static const wchar_t* archTags[] = { L" (x64)", L" (x86)", L" (64-bit)", L" (32-bit)", L" - 64-bit", L" - 32-bit" };
        for (const wchar_t* tag : archTags) {
            size_t pos = s.find(tag);
            if (pos != std::wstring::npos) {
                s.erase(pos, wcslen(tag));
            }
        }
        return s;
    }

    static bool IsSystem32Path(const std::wstring& path) {
        if (path.empty()) return false;
        std::wstring lower = Config::ToLower(path);
        for (auto& c : lower) {
            if (c == L'/') c = L'\\';
        }
        return (lower.find(L"\\system32") != std::wstring::npos ||
                lower.find(L"\\syswow64") != std::wstring::npos);
    }

    static bool IsJunkOrSecondaryShortcut(const std::wstring& name, const std::wstring& fullShortcutPath, const std::wstring& resolvedTarget) {
        std::wstring lowerName = Config::ToLower(name);
        std::wstring lowerPath = Config::ToLower(fullShortcutPath);
        std::wstring lowerTarget = Config::ToLower(resolvedTarget);

        if (IsSystem32Path(lowerTarget) || IsSystem32Path(lowerPath)) {
            return true;
        }

        if (lowerPath.find(L"\\administrative tools") != std::wstring::npos ||
            lowerPath.find(L"\\windows tools") != std::wstring::npos ||
            lowerPath.find(L"\\system tools") != std::wstring::npos ||
            lowerPath.find(L"\\accessibility") != std::wstring::npos ||
            lowerPath.find(L"\\windows kits") != std::wstring::npos) {
            return true;
        }

        if (lowerName == L"voiceaccess" || lowerName == L"voice access" ||
            lowerName == L"narrator" || lowerName == L"magnify" ||
            lowerName == L"on-screen keyboard" || lowerName == L"livecaptions") {
            return true;
        }

        static const wchar_t* junkKeywords[] = {
            L"uninstall", L"deinstall", L"uninst", L"installer", L"setup",
            L"error report", L"crash report", L"crashreporter", L"bug report", L"feedback",
            L"recovery", L"diagnostic", L"troubleshoot", L"repair", L"cleaner",
            L"readme", L"read me", L"read_me", L"license", L"eula", L"terms", L"privacy",
            L"help", L"manual", L"guide", L"faq", L"documentation", L"release notes", L"changelog",
            L"website", L"homepage", L"online support",
            L"sample desktop", L"sample uwp", L"tools for desktop", L"tools for uwp",
            L"application verifier", L"app cert kit", L"cert kit", L"developer command",
            L"developer powershell", L"database compare", L"spreadsheet compare",
            L"private browsing", L"auto-start", L"autostart", L"updater", L"check for update",
            L"check updates", L"native tools", L"cross tools", L"install additional",
            L"module docs", L"manuals",
            L"\x0434\x0435\x0438\x043D\x0441\x0442\x0430\x043B\x043B",
            L"\x0443\x0434\x0430\x043B\x0438\x0442\x044C",
            L"\x0443\x0434\x0430\x043B\x0435\x043D\x0438\x0435",
            L"\x043E\x0431\x043D\x043E\x0432\x043B\x0435\x043D\x0438\x0435",
            L"\x0436\x0443\x0440\x043D\x0430\x043B",
            L"\x044F\x0437\x044B\x043A\x043E\x0432\x044B\x0435",
            L"\x0441\x043F\x0440\x0430\x0432\x043A\x0430",
            L"\x0440\x0443\x043A\x043E\x0432\x043E\x0434\x0441\x0442\x0432\x043E",
            L"\x043F\x043E\x043C\x043E\x0449\x044C",
            L"\x043A\x043E\x0440\x0437\x0438\x043D\x0430"
        };

        for (const wchar_t* junk : junkKeywords) {
            if (lowerName.find(junk) != std::wstring::npos) return true;
        }

        if (lowerName == L"git cmd" || lowerName == L"git gui") return true;
        if (lowerName == L"dfrgui" || lowerName == L"services") return true;
        if (lowerName.find(L"software development kit") != std::wstring::npos ||
            lowerName.find(L"windows kits") != std::wstring::npos) return true;
        if (lowerTarget.find(L"explorer.exe") != std::wstring::npos) {
            if (lowerName != L"\x043F\x0440\x043E\x0432\x043E\x0434\x043D\x0438\x043A" &&
                lowerName != L"file explorer" && lowerName != L"explorer") {
                return true;
            }
        }

        size_t slashPos = lowerTarget.find_last_of(L"\\/");
        size_t dotPos = lowerTarget.find_last_of(L'.');
        if (dotPos == std::wstring::npos || (slashPos != std::wstring::npos && dotPos < slashPos)) return true;
        std::wstring ext = lowerTarget.substr(dotPos);
        return (ext != L".exe");
    }

    static void ScanDirectoryForShortcuts(const std::wstring& directory, std::vector<AppItem>& outList,
                                          std::unordered_set<std::wstring>& seenNames,
                                          std::unordered_set<std::wstring>& seenTargets) {
        std::wstring searchPath = directory + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

                std::wstring fullPath = directory + L"\\" + fd.cFileName;

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    ScanDirectoryForShortcuts(fullPath, outList, seenNames, seenTargets);
                } else {
                    std::wstring fileName = fd.cFileName;
                    size_t dotPos = fileName.find_last_of(L'.');
                    if (dotPos != std::wstring::npos) {
                        std::wstring ext = fileName.substr(dotPos);
                        for (auto& c : ext) c = FastChar::ToLower(c);

                        if (ext == L".lnk") {
                            std::wstring appName = fileName.substr(0, dotPos);

                            std::wstring iconPath;
                            int iconIdx = 0;
                            std::wstring args;
                            std::wstring resolvedTarget = ResolveLnkTarget(fullPath, iconPath, iconIdx, &args);
                            resolvedTarget = Config::ResolveAppPath(resolvedTarget);

                            if (IsJunkOrSecondaryShortcut(appName, fullPath, resolvedTarget)) continue;

                            appName = CleanAppName(appName);

                            std::wstring normTarget = Config::ToLower(resolvedTarget);
                            std::wstring normName = Config::ToLower(appName);

                            if (seenNames.count(normName) > 0) continue;
                            if (!normTarget.empty() && seenTargets.count(normTarget) > 0) continue;

                            seenNames.insert(normName);
                            if (!normTarget.empty()) seenTargets.insert(normTarget);

                            AppItem item;
                            item.name = appName;
                            bool hasRealExe = (!resolvedTarget.empty() && args.empty() && GetFileAttributesW(resolvedTarget.c_str()) != INVALID_FILE_ATTRIBUTES);
                            item.target = hasRealExe ? resolvedTarget : fullPath;
                            item.arguments = args;
                            item.lowerName = normName;
                            item.lowerExt = hasRealExe ? L".exe" : L".lnk";
                            item.lowerTarget = normTarget;
                            item.isDirectory = false;
                            item.hIcon = ExtractCleanIcon(fullPath, false);
                            outList.push_back(item);
                        }
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    static std::vector<AppItem> ScanFrequentFolders() {
        std::vector<AppItem> folders;
        std::unordered_set<std::wstring> seenPaths;

        auto addFolderItem = [&](const std::wstring& name, const std::wstring& path) {
            if (path.empty()) return;
            if (!Config::DoesFolderExist(path)) return;

            std::wstring norm = Config::ToLower(path);
            while (norm.length() > 3 && (norm.back() == L'\\' || norm.back() == L'/')) {
                norm.pop_back();
            }
            if (seenPaths.count(norm) > 0) return;
            seenPaths.insert(norm);

            AppItem item;
            item.name = name;
            item.target = path;
            item.lowerName = Config::ToLower(name);
            item.lowerExt = L"";
            item.lowerTarget = norm;
            item.isDirectory = true;
            item.hIcon = ExtractCleanIcon(path, true);
            folders.push_back(item);
        };

        const struct FolderDef {
            const wchar_t* name;
            KNOWNFOLDERID fid;
        } standardFolders[] = {
            { L"Desktop",   FOLDERID_Desktop },
            { L"Downloads", FOLDERID_Downloads },
            { L"Documents", FOLDERID_Documents },
            { L"Pictures",  FOLDERID_Pictures },
            { L"Videos",    FOLDERID_Videos },
            { L"Music",     FOLDERID_Music }
        };

        for (const auto& fdef : standardFolders) {
            PWSTR pPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(fdef.fid, 0, nullptr, &pPath)) && pPath) {
                addFolderItem(fdef.name, pPath);
                CoTaskMemFree(pPath);
            }
        }

        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (drives & (1 << i)) {
                wchar_t driveLetter[] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
                UINT type = GetDriveTypeW(driveLetter);
                if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                    wchar_t volName[MAX_PATH] = { 0 };
                    std::wstring driveName;
                    if (GetVolumeInformationW(driveLetter, volName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0) && volName[0] != L'\0') {
                        driveName = std::wstring(volName) + L" (" + std::wstring(driveLetter, 2) + L")";
                    } else {
                        driveName = std::wstring(L"Disk (") + std::wstring(driveLetter, 2) + L")";
                    }
                    addFolderItem(driveName, driveLetter);
                }
            }
        }

        return folders;
    }

    static bool ShouldSkipDirectory(const wchar_t* dirName) {
        if (!dirName || dirName[0] == L'\0') return true;
        if (_wcsicmp(dirName, L"System Volume Information") == 0) return true;
        if (_wcsicmp(dirName, L"WinSxS") == 0) return true;
        return false;
    }

    static bool ShouldSkipFile(const wchar_t* fName) {
        if (!fName || fName[0] == L'\0') return true;
        if (fName[0] == L'.' && (fName[1] == L'\0' || (fName[1] == L'.' && fName[2] == L'\0'))) return true;
        return false;
    }

    static void ScanDirectoryFast(const std::wstring& directory, int maxDepth, int currentDepth,
                                  std::vector<std::wstring>& dirPool,
                                  std::vector<CompactFileItem>& outItems) {
        if (currentDepth > maxDepth) return;

        std::wstring pattern = directory;
        if (pattern.back() != L'\\') pattern += L'\\';
        pattern += L'*';

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (hFind == INVALID_HANDLE_VALUE) return;

        uint32_t dirId = (uint32_t)dirPool.size();
        dirPool.push_back(directory);

        std::vector<std::wstring> subdirs;
        subdirs.reserve(16);

        do {
            if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) {
                continue;
            }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                    continue;
                }
                if (ShouldSkipDirectory(fd.cFileName)) continue;

                CompactFileItem folderItem;
                folderItem.dirId = dirId;
                folderItem.name = fd.cFileName;
                folderItem.fileSize = 0;
                folderItem.lastWriteTime = fd.ftLastWriteTime;
                folderItem.isDirectory = true;
                outItems.push_back(std::move(folderItem));

                std::wstring fullPath = directory;
                if (fullPath.back() != L'\\') fullPath += L'\\';
                fullPath += fd.cFileName;
                subdirs.push_back(std::move(fullPath));
            } else {
                if (ShouldSkipFile(fd.cFileName)) continue;

                CompactFileItem item;
                item.dirId = dirId;
                item.name = fd.cFileName;
                ULARGE_INTEGER ul;
                ul.LowPart = fd.nFileSizeLow;
                ul.HighPart = fd.nFileSizeHigh;
                item.fileSize = ul.QuadPart;
                item.lastWriteTime = fd.ftLastWriteTime;
                item.isDirectory = false;
                outItems.push_back(std::move(item));
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);

        for (const auto& sub : subdirs) {
            ScanDirectoryFast(sub, maxDepth, currentDepth + 1, dirPool, outItems);
        }
    }

    static void ScanDriveRoot(const std::wstring& rootDir,
                              std::vector<std::wstring>& dirPool,
                              std::vector<CompactFileItem>& items) {
        std::wstring pattern = rootDir;
        if (pattern.back() != L'\\') pattern += L'\\';
        pattern += L'*';

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (hFind == INVALID_HANDLE_VALUE) return;

        uint32_t dirId = (uint32_t)dirPool.size();
        dirPool.push_back(rootDir);

        std::vector<std::wstring> subdirs;
        subdirs.reserve(32);

        do {
            if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) {
                continue;
            }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                    continue;
                }
                if (ShouldSkipDirectory(fd.cFileName)) continue;

                CompactFileItem folderItem;
                folderItem.dirId = dirId;
                folderItem.name = fd.cFileName;
                folderItem.fileSize = 0;
                folderItem.lastWriteTime = fd.ftLastWriteTime;
                folderItem.isDirectory = true;
                items.push_back(std::move(folderItem));

                std::wstring fullPath = rootDir;
                if (fullPath.back() != L'\\') fullPath += L'\\';
                fullPath += fd.cFileName;
                subdirs.push_back(std::move(fullPath));
            } else {
                if (ShouldSkipFile(fd.cFileName)) continue;

                CompactFileItem item;
                item.dirId = dirId;
                item.name = fd.cFileName;
                ULARGE_INTEGER ul;
                ul.LowPart = fd.nFileSizeLow;
                ul.HighPart = fd.nFileSizeHigh;
                item.fileSize = ul.QuadPart;
                item.lastWriteTime = fd.ftLastWriteTime;
                item.isDirectory = false;
                items.push_back(std::move(item));
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);

        for (const auto& sub : subdirs) {
            ScanDirectoryFast(sub, 32, 1, dirPool, items);
        }
    }

    static void ScanAllDrivesAndUserFiles(std::vector<std::wstring>& dirPool,
                                          std::vector<CompactFileItem>& items) {
        items.reserve(262144);
        dirPool.reserve(32768);

        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (drives & (1 << i)) {
                wchar_t driveLetter = (wchar_t)(L'A' + i);
                wchar_t driveRoot[] = { driveLetter, L':', L'\\', L'\0' };
                UINT type = GetDriveTypeW(driveRoot);
                if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                    std::wstring rootStr = std::wstring(1, driveLetter) + L":";
                    ScanDriveRoot(rootStr, dirPool, items);
                }
            }
        }

        items.shrink_to_fit();
        dirPool.shrink_to_fit();
    }

    static bool IsAllowedAppsFolderApp(const std::wstring& name, const std::wstring& parseName) {
        std::wstring lowerName = Config::ToLower(name);
        std::wstring lowerParse = Config::ToLower(parseName);

        if (lowerParse.find(L"microsoft.windowsstore") != std::wstring::npos ||
            lowerName == L"microsoft store" || lowerName == L"store" ||
            lowerName == L"\x043C\x0430\x0433\x0430\x0437\x0438\x043D microsoft") {
            return true;
        }

        if (lowerParse.find(L"nvidiacontrolpanel") != std::wstring::npos ||
            lowerName.find(L"nvidia control panel") != std::wstring::npos ||
            lowerName.find(L"\x043F\x0430\x043D\x0435\x043B\x044C \x0443\x043F\x0440\x0430\x0432\x043B\x0435\x043D\x0438\x044F nvidia") != std::wstring::npos) {
            return true;
        }

        if (lowerParse.find(L"bingweather") != std::wstring::npos ||
            lowerName == L"\x043F\x043E\x0433\x043E\x0434\x0430" || lowerName == L"weather" ||
            lowerName == L"msn weather" || lowerName == L"msn \x043F\x043E\x0433\x043E\x0434\x0430") {
            return true;
        }

        if (lowerParse.find(L"screensketch") != std::wstring::npos ||
            lowerParse.find(L"snippingtool") != std::wstring::npos ||
            lowerName == L"\x043D\x043E\x0436\x043D\x0438\x0446\x044B" || lowerName == L"snipping tool") {
            return true;
        }

        return false;
    }

    static void ScanAppsFolder(std::vector<AppItem>& outList,
                               std::unordered_set<std::wstring>& seenNames,
                               std::unordered_set<std::wstring>& seenTargets) {
        IShellItem* pAppsFolder = nullptr;
        if (FAILED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&pAppsFolder))) || !pAppsFolder) {
            return;
        }

        IEnumShellItems* pEnum = nullptr;
        if (FAILED(pAppsFolder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&pEnum))) || !pEnum) {
            pAppsFolder->Release();
            return;
        }

        IShellItem* pItem = nullptr;
        while (pEnum->Next(1, &pItem, nullptr) == S_OK && pItem) {
            LPWSTR name = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &name)) && name) {
                std::wstring sName = CleanAppName(name);
                std::wstring lowerName = Config::ToLower(sName);
                if (!sName.empty() && !seenNames.count(lowerName)) {
                    LPWSTR parseName = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parseName)) && parseName) {
                        if (IsAllowedAppsFolderApp(sName, parseName)) {
                            std::wstring target = L"shell:AppsFolder\\" + std::wstring(parseName);
                            std::wstring lowerTarget = Config::ToLower(target);
                            if (!seenTargets.count(lowerTarget)) {
                                AppItem app;
                                app.name = sName;
                                app.lowerName = lowerName;
                                app.target = target;
                                app.lowerTarget = lowerTarget;
                                app.isDirectory = false;
                                app.hIcon = ExtractCleanIcon(target);

                                seenNames.insert(lowerName);
                                seenTargets.insert(lowerTarget);
                                outList.push_back(std::move(app));
                            }
                        }
                        CoTaskMemFree(parseName);
                    }
                }
                CoTaskMemFree(name);
            }
            pItem->Release();
        }

        pEnum->Release();
        pAppsFolder->Release();
    }

    static void StartAsyncIndexing(HWND notifyWnd) {
        std::thread([notifyWnd]() {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

            std::vector<AppItem> apps;
            std::unordered_set<std::wstring> seenNames;
            std::unordered_set<std::wstring> seenTargets;

            PWSTR commonPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, nullptr, &commonPath)) && commonPath) {
                ScanDirectoryForShortcuts(commonPath, apps, seenNames, seenTargets);
                CoTaskMemFree(commonPath);
            }

            PWSTR userPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &userPath)) && userPath) {
                ScanDirectoryForShortcuts(userPath, apps, seenNames, seenTargets);
                CoTaskMemFree(userPath);
            }

            PWSTR desktopPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktopPath)) && desktopPath) {
                ScanDirectoryForShortcuts(desktopPath, apps, seenNames, seenTargets);
                CoTaskMemFree(desktopPath);
            }

            PWSTR pubDesktopPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop, 0, nullptr, &pubDesktopPath)) && pubDesktopPath) {
                ScanDirectoryForShortcuts(pubDesktopPath, apps, seenNames, seenTargets);
                CoTaskMemFree(pubDesktopPath);
            }

            ScanAppsFolder(apps, seenNames, seenTargets);

            std::sort(apps.begin(), apps.end(), [](const AppItem& a, const AppItem& b) {
                return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
            });

            std::vector<AppItem> frequentFolders = ScanFrequentFolders();

            auto initialSnapshot = std::make_shared<IndexSnapshot>();
            initialSnapshot->installedApps = apps;
            initialSnapshot->frequentFolders = frequentFolders;
            {
                std::lock_guard<std::mutex> lock(Config::g_indexMutex);
                Config::g_indexSnapshot = initialSnapshot;
                Config::g_isIndexingComplete = false;
            }
            if (notifyWnd) {
                PostMessageW(notifyWnd, WM_APP_INDEX_READY, 0, 0);
            }

            auto fullSnapshot = std::make_shared<IndexSnapshot>();
            fullSnapshot->installedApps = std::move(apps);
            fullSnapshot->frequentFolders = std::move(frequentFolders);
            ScanAllDrivesAndUserFiles(fullSnapshot->dirPool, fullSnapshot->userFiles);

            {
                std::lock_guard<std::mutex> lock(Config::g_indexMutex);
                Config::g_indexSnapshot = std::move(fullSnapshot);
                Config::g_isIndexingComplete = true;
            }

            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

            if (notifyWnd) {
                PostMessageW(notifyWnd, WM_APP_INDEX_READY, 0, 0);
            }

            CoUninitialize();
        }).detach();
    }

    static std::wstring ConvertKeyboardLayout(const std::wstring& input) {
        static const std::unordered_map<wchar_t, wchar_t> ruToEn = {
            {L'й', L'q'}, {L'ц', L'w'}, {L'у', L'e'}, {L'к', L'r'}, {L'е', L't'}, {L'н', L'y'},
            {L'г', L'u'}, {L'ш', L'i'}, {L'щ', L'o'}, {L'з', L'p'}, {L'х', L'['}, {L'ъ', L']'},
            {L'ф', L'a'}, {L'ы', L's'}, {L'в', L'd'}, {L'а', L'f'}, {L'п', L'g'}, {L'р', L'h'},
            {L'о', L'j'}, {L'л', L'k'}, {L'д', L'l'}, {L'ж', L';'}, {L'э', L'\''}, {L'я', L'z'},
            {L'ч', L'x'}, {L'с', L'c'}, {L'м', L'v'}, {L'и', L'b'}, {L'т', L'n'}, {L'ь', L'm'},
            {L'б', L','}, {L'ю', L'.'}
        };

        std::wstring result = input;
        for (auto& c : result) {
            wchar_t low = FastChar::ToLower(c);
            auto it = ruToEn.find(low);
            if (it != ruToEn.end()) {
                c = it->second;
            }
        }
        return result;
    }

    static int LevenshteinDistance(std::wstring_view s1, std::wstring_view s2) {
        const size_t len1 = s1.size(), len2 = s2.size();
        if (len1 == 0) return (int)len2;
        if (len2 == 0) return (int)len1;
        if (len2 >= 64) return 99;

        int col[65];
        for (size_t y = 0; y <= len2; ++y) col[y] = (int)y;

        for (size_t x = 1; x <= len1; ++x) {
            int lastDiag = col[0];
            col[0] = (int)x;
            for (size_t y = 1; y <= len2; ++y) {
                int oldCol = col[y];
                int cost = (s1[x - 1] == s2[y - 1]) ? 0 : 1;
                col[y] = (std::min)({ col[y] + 1, col[y - 1] + 1, lastDiag + cost });
                lastDiag = oldCol;
            }
        }
        return col[len2];
    }

    static int CalculateItemScoreFast(const AppItem& item,
                                      const std::wstring& lowerQuery,
                                      const std::wstring& convertedQuery,
                                      const std::vector<std::wstring>& tokens,
                                      const std::vector<std::wstring>& convertedTokens) {
        if (lowerQuery.empty()) return 100;

        if (item.lowerName == lowerQuery) return 100;
        if (item.lowerName.rfind(lowerQuery, 0) == 0) return 96;

        size_t namePos = item.lowerName.find(lowerQuery);
        if (namePos != std::wstring::npos) {
            return 92 - (int)(std::min)((size_t)15, namePos);
        }

        if (convertedQuery != lowerQuery) {
            if (item.lowerName == convertedQuery) return 98;
            if (item.lowerName.rfind(convertedQuery, 0) == 0) return 94;
            size_t cpos = item.lowerName.find(convertedQuery);
            if (cpos != std::wstring::npos) return 88 - (int)(std::min)((size_t)15, cpos);
        }

        if (tokens.size() > 1) {
            bool allInName = true;
            for (size_t t = 0; t < tokens.size(); ++t) {
                if (item.lowerName.find(tokens[t]) == std::wstring::npos) {
                    if (convertedTokens[t].empty() || item.lowerName.find(convertedTokens[t]) == std::wstring::npos) {
                        allInName = false;
                        break;
                    }
                }
            }
            if (allInName) return 95;

            bool allFound = true;
            for (size_t t = 0; t < tokens.size(); ++t) {
                bool inThis = (item.lowerName.find(tokens[t]) != std::wstring::npos) ||
                              (!item.lowerExt.empty() && item.lowerExt.find(tokens[t]) != std::wstring::npos) ||
                              (!item.lowerTarget.empty() && item.lowerTarget.find(tokens[t]) != std::wstring::npos);
                if (!inThis && !convertedTokens[t].empty()) {
                    inThis = (item.lowerName.find(convertedTokens[t]) != std::wstring::npos) ||
                             (!item.lowerExt.empty() && item.lowerExt.find(convertedTokens[t]) != std::wstring::npos) ||
                             (!item.lowerTarget.empty() && item.lowerTarget.find(convertedTokens[t]) != std::wstring::npos);
                }
                if (!inThis) {
                    allFound = false;
                    break;
                }
            }
            if (allFound) return 72;
        }

        if (!item.lowerExt.empty()) {
            if (lowerQuery == item.lowerExt || 
               (lowerQuery.length() + 1 == item.lowerExt.length() && item.lowerExt.compare(1, lowerQuery.length(), lowerQuery) == 0)) {
                return 80;
            }
        }

        if (!item.lowerTarget.empty() && item.lowerTarget.find(lowerQuery) != std::wstring::npos) {
            return 68;
        }

        if (lowerQuery.length() >= 4 && item.lowerName.length() >= 3 && item.lowerName.length() <= 32) {
            std::wstring_view prefix(item.lowerName.c_str(), (std::min)(item.lowerName.length(), lowerQuery.length()));
            int dist = LevenshteinDistance(prefix, lowerQuery);
            if (dist <= 2) return 55 - (dist * 10);
        }

        return 0;
    }

    static inline size_t FindCaseInsensitive(const std::wstring& haystack, const std::wstring& lowerNeedle) {
        if (lowerNeedle.empty()) return 0;
        if (haystack.length() < lowerNeedle.length()) return std::wstring::npos;

        size_t nLen = lowerNeedle.length();
        size_t hLen = haystack.length();
        wchar_t first = lowerNeedle[0];

        for (size_t i = 0; i <= hLen - nLen; ++i) {
            if (FastChar::ToLower(haystack[i]) == first) {
                bool match = true;
                for (size_t j = 1; j < nLen; ++j) {
                    if (FastChar::ToLower(haystack[i + j]) != lowerNeedle[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return i;
            }
        }
        return std::wstring::npos;
    }

    static int CalculateCompactScoreFast(const CompactFileItem& item,
                                         const std::wstring& lowerQuery,
                                         const std::wstring& convertedQuery,
                                         const std::vector<std::wstring>& tokens,
                                         const std::vector<std::wstring>& convertedTokens) {
        if (lowerQuery.empty()) return 100;

        const std::wstring& name = item.name;
        size_t nLen = name.length();
        size_t qLen = lowerQuery.length();

        if (nLen == qLen) {
            bool exact = true;
            for (size_t i = 0; i < nLen; ++i) {
                if (FastChar::ToLower(name[i]) != lowerQuery[i]) {
                    exact = false;
                    break;
                }
            }
            if (exact) return 100;
        }

        size_t namePos = FindCaseInsensitive(name, lowerQuery);
        if (namePos == 0) return 96;
        if (namePos != std::wstring::npos) {
            return 92 - (int)(std::min)((size_t)15, namePos);
        }

        if (!convertedQuery.empty() && convertedQuery != lowerQuery) {
            if (nLen == convertedQuery.length()) {
                bool exact = true;
                for (size_t i = 0; i < nLen; ++i) {
                    if (FastChar::ToLower(name[i]) != convertedQuery[i]) {
                        exact = false;
                        break;
                    }
                }
                if (exact) return 98;
            }
            size_t cpos = FindCaseInsensitive(name, convertedQuery);
            if (cpos == 0) return 94;
            if (cpos != std::wstring::npos) return 88 - (int)(std::min)((size_t)15, cpos);
        }

        if (tokens.size() > 1) {
            bool allFound = true;
            for (size_t t = 0; t < tokens.size(); ++t) {
                if (FindCaseInsensitive(name, tokens[t]) == std::wstring::npos) {
                    if (convertedTokens[t].empty() || FindCaseInsensitive(name, convertedTokens[t]) == std::wstring::npos) {
                        allFound = false;
                        break;
                    }
                }
            }
            if (allFound) return 90;
        }

        if (!item.isDirectory) {
            size_t dotPos = name.find_last_of(L'.');
            if (dotPos != std::wstring::npos) {
                size_t extLen = nLen - (dotPos + 1);
                if (extLen == qLen) {
                    bool matchExt = true;
                    for (size_t i = 0; i < qLen; ++i) {
                        if (FastChar::ToLower(name[dotPos + 1 + i]) != lowerQuery[i]) {
                            matchExt = false;
                            break;
                        }
                    }
                    if (matchExt) return 80;
                }
            }
        }

        return 0;
    }
};