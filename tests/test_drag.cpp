// Drives NanaZipAutoCP.dll the way Explorer does, without Explorer, and prints what it offers.
// test_drag.exe DLL drag|click FOLDER COMMAND FILE...
//   drag:  FOLDER is the drop target of a right-drag; click: FOLDER holds the right-clicked files
//   COMMAND: 0 runs the first item, 1 the second, -1 only lists the menu
// Exits with 2 on bad arguments or a failure before Initialize, otherwise 0.
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <io.h>
#include <fcntl.h>
#include <vector>

static const CLSID kDragHandler = {0xf395f013, 0xbb22, 0x4061, {0xb8, 0x44, 0xbf, 0x5c, 0x79, 0xeb, 0xbe, 0x58}};
static const CLSID kContextMenuHandler = {0xda650b86, 0x9be4, 0x4187, {0x98, 0x70, 0x91, 0x79, 0xae, 0x35, 0xac, 0x68}};
static const UINT kFirstId = 100;
typedef HRESULT(STDAPICALLTYPE* GetClassObjectFn)(REFCLSID, REFIID, void**);

static int Fail(const wchar_t* what, HRESULT hr) {
    wprintf(L"%ls failed 0x%08lx\n", what, static_cast<unsigned long>(hr));
    return 2;
}

int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U8TEXT);
    wchar_t* end = nullptr;
    const long cmd = argc >= 6 ? wcstol(argv[4], &end, 10) : -2;
    const bool drag = argc >= 6 && wcscmp(argv[2], L"drag") == 0;
    if (argc < 6 || (!drag && wcscmp(argv[2], L"click") != 0) || *end || cmd < -1 || cmd > 1) {
        wprintf(L"usage: test_drag.exe DLL drag|click FOLDER COMMAND(-1|0|1) FILE...\n");
        return 2;
    }
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return Fail(L"CoInitializeEx", hr);
    HMODULE dll = LoadLibraryW(argv[1]);
    if (!dll) return Fail(L"LoadLibrary", HRESULT_FROM_WIN32(GetLastError()));
    auto getClassObject = reinterpret_cast<GetClassObjectFn>(GetProcAddress(dll, "DllGetClassObject"));
    if (!getClassObject) return Fail(L"GetProcAddress", HRESULT_FROM_WIN32(GetLastError()));
    IClassFactory* factory = nullptr;
    hr = getClassObject(drag ? kDragHandler : kContextMenuHandler, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return Fail(L"DllGetClassObject", hr);
    IShellExtInit* init = nullptr;
    hr = factory->CreateInstance(nullptr, IID_PPV_ARGS(&init));
    if (FAILED(hr)) return Fail(L"CreateInstance", hr);

    PIDLIST_ABSOLUTE folder = nullptr;
    hr = SHParseDisplayName(argv[3], nullptr, &folder, 0, nullptr);
    if (FAILED(hr)) return Fail(L"SHParseDisplayName(folder)", hr);
    std::vector<PIDLIST_ABSOLUTE> items;
    for (int i = 5; i < argc; i++) {
        PIDLIST_ABSOLUTE p = nullptr;
        hr = SHParseDisplayName(argv[i], nullptr, &p, 0, nullptr);
        if (FAILED(hr)) return Fail(L"SHParseDisplayName(file)", hr);
        items.push_back(p);
    }
    IShellItemArray* array = nullptr;
    hr = SHCreateShellItemArrayFromIDLists(static_cast<UINT>(items.size()), const_cast<PCIDLIST_ABSOLUTE_ARRAY>(items.data()), &array);
    if (FAILED(hr)) return Fail(L"SHCreateShellItemArrayFromIDLists", hr);
    IDataObject* data = nullptr;
    hr = array->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data));
    if (FAILED(hr)) return Fail(L"BindToHandler", hr);

    hr = init->Initialize(folder, data, nullptr);
    wprintf(L"Initialize 0x%08lx\n", static_cast<unsigned long>(hr));
    if (SUCCEEDED(hr)) {
        IContextMenu* cm = nullptr;
        hr = init->QueryInterface(IID_PPV_ARGS(&cm));
        if (FAILED(hr)) return Fail(L"QueryInterface(IContextMenu)", hr);
        HMENU menu = CreatePopupMenu();
        hr = cm->QueryContextMenu(menu, 0, kFirstId, 0x7FFF, CMF_NORMAL);
        wprintf(L"QueryContextMenu 0x%08lx items %d\n", static_cast<unsigned long>(hr), GetMenuItemCount(menu));
        for (int i = 0; i < GetMenuItemCount(menu); i++) {
            wchar_t text[512] = {};
            GetMenuStringW(menu, i, text, 512, MF_BYPOSITION);
            MENUITEMINFOW mii = {sizeof(mii)};
            mii.fMask = MIIM_ID | MIIM_BITMAP;
            GetMenuItemInfoW(menu, i, TRUE, &mii);
            wprintf(L"  id %u: %ls\n", mii.wID - kFirstId, text);
            DIBSECTION ds = {};
            if (mii.hbmpItem && GetObjectW(mii.hbmpItem, sizeof(ds), &ds) == sizeof(ds) && ds.dsBm.bmBitsPixel == 32) {
                const DWORD* px = static_cast<const DWORD*>(ds.dsBm.bmBits);
                const int w = ds.dsBm.bmWidth, h = ds.dsBm.bmHeight;
                wprintf(L"  bitmap %dx%d alpha corner %lu center %lu\n", w, h, px[0] >> 24, px[(h / 2) * w + w / 2] >> 24);
            }
        }
        if (cmd >= 0) {
            CMINVOKECOMMANDINFO ici = {sizeof(ici)};
            ici.fMask = CMIC_MASK_FLAG_NO_UI;
            ici.lpVerb = MAKEINTRESOURCEA(cmd);
            ici.nShow = SW_SHOWNORMAL;
            wprintf(L"InvokeCommand 0x%08lx\n", static_cast<unsigned long>(cm->InvokeCommand(&ici)));
        }
        DestroyMenu(menu);
        cm->Release();
    }
    data->Release();
    array->Release();
    for (PIDLIST_ABSOLUTE p : items) CoTaskMemFree(p);
    CoTaskMemFree(folder);
    init->Release();
    factory->Release();
    return 0;
}
