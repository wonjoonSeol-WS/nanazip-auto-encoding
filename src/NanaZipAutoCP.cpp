// Explorer menu items that extract archives with NanaZip through Expand-ArchiveAuto.ps1, which picks
// the ZIP file name code page. Registered as a context menu handler for .zip (right-click) and as a
// drag-and-drop handler for folders (right-drag). Labels, paths and the archive extensions come from
// HKCU\Software\NanaZipAutoCP.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <new>
#include <string>
#include <vector>

// {F395F013-BB22-4061-B844-BF5C79EBBE58}: right-drag, extracts into the drop target
static const CLSID CLSID_DragHandler = {0xf395f013, 0xbb22, 0x4061, {0xb8, 0x44, 0xbf, 0x5c, 0x79, 0xeb, 0xbe, 0x58}};
// {DA650B86-9BE4-4187-9870-9179AE35AC68}: right-click, extracts next to each archive
static const CLSID CLSID_ContextMenuHandler = {0xda650b86, 0x9be4, 0x4187, {0x98, 0x70, 0x91, 0x79, 0xae, 0x35, 0xac, 0x68}};

static LONG g_dllRefs = 0;

namespace {

const wchar_t kSettingsKey[] = L"Software\\NanaZipAutoCP";
const wchar_t* const kModes[] = {L"Smart", L"Folder"};
const wchar_t* const kLabelValues[] = {L"LabelSmart", L"LabelFolder"};
const wchar_t* const kFallbackLabels[] = {
    L"Extract Here (Smart) with NanaZip (auto encoding)",
    L"Extract to Folder with NanaZip (auto encoding)",
};
constexpr UINT kCommandCount = ARRAYSIZE(kModes);
static_assert(ARRAYSIZE(kLabelValues) == kCommandCount && ARRAYSIZE(kFallbackLabels) == kCommandCount, "one label per mode");
// CreateProcess accepts at most 32767 characters; longer selections start several runs
constexpr size_t kMaxCommandLine = 32000;

std::wstring ReadSetting(const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS ||
        size < sizeof(wchar_t)) {
        return L"";
    }
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_SZ, nullptr, &value[0], &size) != ERROR_SUCCESS) {
        return L"";
    }
    value.resize(wcslen(value.c_str()));
    return value;
}

std::wstring Lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], static_cast<DWORD>(s.size()));
    return s;
}

bool IsFile(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsDirectory(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// PowerShell reads a backslash before a closing quote as an escaped quote, so "C:\" becomes "C:\."
std::wstring Quote(const std::wstring& arg) {
    return L"\"" + arg + (!arg.empty() && arg.back() == L'\\' ? L"." : L"") + L"\"";
}

// Menu bitmaps must be 32-bit with alpha. Small NanaZip icon images are paletted with a mask and
// no alpha, so the alpha channel is rebuilt from the mask when needed.
HBITMAP IconToMenuBitmap(const std::wstring& iconPath) {
    const int cx = GetSystemMetrics(SM_CXSMICON);
    const int cy = GetSystemMetrics(SM_CYSMICON);
    HICON icon = static_cast<HICON>(LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, cx, cy, LR_LOADFROMFILE));
    if (!icon) return nullptr;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = -cy;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC dc = CreateCompatibleDC(nullptr);
    void* colorBits = nullptr;
    void* maskBits = nullptr;
    HBITMAP color = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &colorBits, nullptr, 0) : nullptr;
    HBITMAP mask = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &maskBits, nullptr, 0) : nullptr;
    bool ok = color && mask;
    if (ok) {
        HGDIOBJ old = SelectObject(dc, color);
        ok = DrawIconEx(dc, 0, 0, icon, cx, cy, 0, nullptr, DI_NORMAL) != FALSE;
        SelectObject(dc, mask);
        ok = ok && DrawIconEx(dc, 0, 0, icon, cx, cy, 0, nullptr, DI_MASK) != FALSE;
        SelectObject(dc, old);
        GdiFlush();
    }
    if (ok) {
        DWORD* px = static_cast<DWORD*>(colorBits);
        const DWORD* mk = static_cast<const DWORD*>(maskBits);
        const int n = cx * cy;
        bool hasAlpha = false;
        for (int i = 0; i < n && !hasAlpha; i++) hasAlpha = (px[i] & 0xFF000000) != 0;
        if (!hasAlpha) {
            // The mask is white where transparent and black where the icon is drawn
            for (int i = 0; i < n; i++) px[i] = (mk[i] & 0x00FFFFFF) ? 0 : (px[i] | 0xFF000000);
        }
    }
    if (mask) DeleteObject(mask);
    if (!ok && color) {
        DeleteObject(color);
        color = nullptr;
    }
    if (dc) DeleteDC(dc);
    DestroyIcon(icon);
    return color;
}

HRESULT Launch(const std::wstring& application, std::wstring commandLine) {
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(application.c_str(), &commandLine[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &si, &pi)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return S_OK;
}

class ArchiveMenu final : public IShellExtInit, public IContextMenu {
public:
    explicit ArchiveMenu(bool drag) : drag_(drag) { InterlockedIncrement(&g_dllRefs); }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IContextMenu) {
            *ppv = static_cast<IContextMenu*>(this);
        } else if (riid == IID_IShellExtInit) {
            *ppv = static_cast<IShellExtInit*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG refs = InterlockedDecrement(&refs_);
        if (refs == 0) delete this;
        return refs;
    }

    // data holds the selected items; folder is the drop target for right-drag (right-click passes the
    // items' own folder, which can be virtual, such as search results)
    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE folder, IDataObject* data, HKEY) override {
        files_.clear();
        target_.clear();
        if (!data || (drag_ && !folder)) return E_INVALIDARG;
        try {
            if (drag_) {
                PWSTR path = nullptr;
                if (FAILED(SHGetNameFromIDList(folder, SIGDN_FILESYSPATH, &path))) return E_FAIL;
                target_ = path;
                CoTaskMemFree(path);
                if (!IsDirectory(target_)) return E_FAIL;
            }
            FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            STGMEDIUM stg = {};
            if (FAILED(data->GetData(&fmt, &stg))) return E_FAIL;
            if (stg.tymed == TYMED_HGLOBAL) {
                HDROP drop = static_cast<HDROP>(stg.hGlobal);
                const std::wstring extensions = L";" + Lower(ReadSetting(L"Extensions")) + L";";
                const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; i++) {
                    const UINT len = DragQueryFileW(drop, i, nullptr, 0);
                    std::wstring file(len + 1, L'\0');
                    DragQueryFileW(drop, i, &file[0], len + 1);
                    file.resize(len);
                    // Only when every selected item is an existing archive file
                    if (!IsFile(file) || extensions.find(L";" + Lower(PathFindExtensionW(file.c_str())) + L";") == std::wstring::npos) {
                        files_.clear();
                        break;
                    }
                    files_.push_back(file);
                }
            }
            ReleaseStgMedium(&stg);
        } catch (...) {
            files_.clear();
            return E_OUTOFMEMORY;
        }
        return files_.empty() ? E_FAIL : S_OK;
    }

    IFACEMETHODIMP QueryContextMenu(HMENU menu, UINT index, UINT first, UINT last, UINT flags) override {
        const HRESULT none = MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
        if ((flags & (CMF_DEFAULTONLY | CMF_OPTIMIZEFORINVOKE)) || files_.empty() || last - first + 1 < kCommandCount) return none;
        try {
            if (!IsFile(ReadSetting(L"Script"))) return none;
            if (!bitmap_) bitmap_ = IconToMenuBitmap(ReadSetting(L"Icon"));
            for (UINT i = 0; i < kCommandCount; i++) {
                std::wstring label = ReadSetting(kLabelValues[i]);
                if (label.empty()) label = kFallbackLabels[i];
                MENUITEMINFOW mii = {sizeof(mii)};
                mii.fMask = MIIM_STRING | MIIM_ID | (bitmap_ ? MIIM_BITMAP : 0);
                mii.wID = first + i;
                mii.dwTypeData = &label[0];
                mii.hbmpItem = bitmap_;
                InsertMenuItemW(menu, index + i, TRUE, &mii);
            }
        } catch (...) {
            return E_OUTOFMEMORY;
        }
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, kCommandCount);
    }

    IFACEMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* ici) override {
        if (!ici || !IS_INTRESOURCE(ici->lpVerb)) return E_FAIL;  // no canonical verbs
        const UINT cmd = LOWORD(reinterpret_cast<UINT_PTR>(ici->lpVerb));
        if (cmd >= kCommandCount) return E_FAIL;
        HRESULT hr = S_OK;
        try {
            wchar_t system[MAX_PATH];
            if (!GetSystemDirectoryW(system, MAX_PATH)) return HRESULT_FROM_WIN32(GetLastError());
            const std::wstring powershell = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
            std::wstring prefix = Quote(powershell) + L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " +
                                  Quote(ReadSetting(L"Script")) + L" -Mode " + kModes[cmd];
            if (!target_.empty()) prefix += L" -OutDir " + Quote(target_);
            // One run extracts every selected archive in turn
            std::wstring commandLine = prefix;
            for (const std::wstring& file : files_) {
                const std::wstring arg = L" " + Quote(file);
                if (commandLine.size() > prefix.size() && commandLine.size() + arg.size() > kMaxCommandLine) {
                    hr = Launch(powershell, commandLine);
                    if (FAILED(hr)) break;
                    commandLine = prefix;
                }
                commandLine += arg;
            }
            if (SUCCEEDED(hr) && commandLine.size() > prefix.size()) hr = Launch(powershell, commandLine);
        } catch (...) {
            hr = E_OUTOFMEMORY;
        }
        if (FAILED(hr) && !(ici->fMask & CMIC_MASK_FLAG_NO_UI)) {
            MessageBoxW(ici->hwnd, L"Could not start Windows PowerShell to run Expand-ArchiveAuto.ps1.",
                        L"NanaZip auto encoding", MB_OK | MB_ICONERROR);
        }
        return hr;
    }

    IFACEMETHODIMP GetCommandString(UINT_PTR, UINT, UINT*, CHAR*, UINT) override { return E_NOTIMPL; }

private:
    ~ArchiveMenu() {
        if (bitmap_) DeleteObject(bitmap_);
        InterlockedDecrement(&g_dllRefs);
    }

    const bool drag_;
    LONG refs_ = 1;
    std::wstring target_;
    std::vector<std::wstring> files_;
    HBITMAP bitmap_ = nullptr;
};

class ClassFactory final : public IClassFactory {
public:
    explicit ClassFactory(bool drag) : drag_(drag) {}

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    // A static object: counting only keeps the DLL loaded while a client holds the factory
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&g_dllRefs); }
    IFACEMETHODIMP_(ULONG) Release() override { return InterlockedDecrement(&g_dllRefs); }

    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        ArchiveMenu* menu = new (std::nothrow) ArchiveMenu(drag_);
        if (!menu) return E_OUTOFMEMORY;
        const HRESULT hr = menu->QueryInterface(riid, ppv);
        menu->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_dllRefs);
        else InterlockedDecrement(&g_dllRefs);
        return S_OK;
    }

private:
    const bool drag_;
};

ClassFactory g_dragFactory(true);
ClassFactory g_contextMenuFactory(false);

}  // namespace

_Check_return_ STDAPI DllGetClassObject(_In_ REFCLSID clsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (clsid == CLSID_DragHandler) return g_dragFactory.QueryInterface(riid, ppv);
    if (clsid == CLSID_ContextMenuHandler) return g_contextMenuFactory.QueryInterface(riid, ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

__control_entrypoint(DllExport) STDAPI DllCanUnloadNow() { return g_dllRefs == 0 ? S_OK : S_FALSE; }
