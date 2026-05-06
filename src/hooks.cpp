#include "pch.h"

BOOL __fastcall G3MPFHOOK_DEF_FUNC(MountPackFile)(void* thisPtr, void* edx, const bCString& param_1, const bCString& param_2)
{
    DWORD dwAttrib = GetFileAttributesA(param_2.GetText());
    BOOL exists = (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));

    if (!exists) return TRUE;

    BOOL result = TRUE;

    if (G3MPFHook::GetInstance()->GetPackFileGatherMode())
    {
        printf("[MountPackFile] GATHER %s -> %s\n", param_2.GetText(), param_1.GetText());
        G3MPFHook::GetInstance()->AddToPackFileQueue({ param_2.GetText(), param_1.GetText() });
    }
    else
    {
        result = G3MPFHOOK_ORIGINAL_FUNC(MountPackFile)(thisPtr, param_1, param_2);
        printf("[MountPackFile] MOUNT %s %s -> %s\n", result ? "SUCCESS" : "ERROR", param_2.GetText(), param_1.GetText());
    }

    return result;
}
G3MPFHOOK_ASSIGN_DETOUR(MountPackFile);


// We intercept all MountPackFile's calls, where we create a PackFile of the path parameter on each call and instead of mounting them instantly,
// we append those files to orig_packfile_queue vector, and then we merge both orig_packfile_queue and ext_packfile_queue vectors to create a
// vector of all pack files to be mounted, which we later pass to MountPackFiles.
void __fastcall G3MPFHOOK_DEF_FUNC(SetRootPath)(void* thisPtr, void* edx, const bCString& path)
{
#if MOUNT_EXTERNAL_PAK
    printf("Gathering original files...\n");
    G3MPFHook::GetInstance()->SetPackFileGatherMode(true);
#endif
    G3MPFHOOK_ORIGINAL_FUNC(SetRootPath)(thisPtr, path);
#if MOUNT_EXTERNAL_PAK
    G3MPFHook::GetInstance()->SetPackFileGatherMode(false);

    auto& orig_packfile_queue = G3MPFHook::GetInstance()->GetOriginalPackFileQueue();
    auto& ext_packfile_queue = G3MPFHook::GetInstance()->GetExternalPackFileQueue();

    std::vector<PackFile> packfile_queue_merge;
    packfile_queue_merge.reserve(orig_packfile_queue.size() + ext_packfile_queue.size());
    packfile_queue_merge.insert(packfile_queue_merge.end(), orig_packfile_queue.begin(), orig_packfile_queue.end());
    packfile_queue_merge.insert(packfile_queue_merge.end(), ext_packfile_queue.begin(), ext_packfile_queue.end());

    G3MPFHook::GetInstance()->MountPackFiles(packfile_queue_merge);
#endif
}
G3MPFHOOK_ASSIGN_DETOUR(SetRootPath);

// Need to detour it so it doesn't add directories to cache while we're gathering files,
// because it doesn't allow us to call MountPackFile on the same path twice
GEUInt __fastcall G3MPFHOOK_DEF_FUNC(AddDirToCache)(eCVirtualFileSystem* thisPtr, void* edx, bCString const& param_1, GEUInt& param_2)
{
    GEUInt result = 0;

    if (!G3MPFHook::GetInstance()->GetPackFileGatherMode() || param_1 == "ini")
    {
        result = G3MPFHOOK_ORIGINAL_FUNC(AddDirToCache)(thisPtr, param_1, param_2);
        //printf("[hkAddDirToCache] %d (%s, %d)\n", result, param_1.GetText(), param_2);
    }

    return result;
}
G3MPFHOOK_ASSIGN_DETOUR(AddDirToCache);

BOOL __fastcall G3MPFHOOK_DEF_FUNC(LoadAllScriptDLLs)(void* thisPtr, void* edx)
{
    // printf("[hkLoadAllScriptDLLs] called\n");

    gCScriptAdminExt& scriptAdmin = GetScriptAdminExt();

    BOOL result = G3MPFHOOK_ORIGINAL_FUNC(LoadAllScriptDLLs)(thisPtr);

    G3MPFHook::GetInstance()->LoadAllScriptDLLsInQueue();

    printf("[hkLoadAllScriptDLLs] Loaded modules:\n");
    GE_ARRAY_FOR_EACH(pDll, scriptAdmin.GetLoadedDLLs())
    {
        printf(" - %s\n", pDll->m_strFileName.GetText());
    }

    return result;
}
G3MPFHOOK_ASSIGN_DETOUR(LoadAllScriptDLLs);

// Add DLL text to the main manu
void __fastcall G3MPFHOOK_DEF_FUNC(FUN_1008FC40)(CFFGFCView* gfcview)
{
    G3MPFHOOK_ORIGINAL_FUNC(FUN_1008FC40)(gfcview);

    CFFGFCWnd* wnd = (CFFGFCWnd*)((char*)gfcview + 0x110);

    bCUnicodeString text = wnd->GetWindowTextA();
    bCUnicodeString new_text = text + " | G3MPFHook Testing";

    printf("%ws\n", new_text.GetText());
    wnd->SetWindowTextA(new_text);

    bCRect rect; // widen the window so there's no line break
    wnd->GetWindowRect(rect);
    rect.Widen(bCPoint(100, 0));
    wnd->MoveWindow(rect);
}
G3MPFHOOK_ASSIGN_DETOUR(FUN_1008FC40);

// We detour LoadLibraryA to load external DLL using it's correct path, because
// LoadScriptDLL makes relative paths absolute which breaks our path absolute script path
HMODULE WINAPI G3MPFHOOK_DEF_FUNC(LoadLibraryA)(LPCSTR lpLibFileName)
{
    HMODULE hModule;

    if (G3MPFHook::GetInstance()->GetScriptTakeCustomDll())
    {
        printf("[hkLoadLibraryA]: Load custom DLL: \"%s\"\n", G3MPFHook::GetInstance()->GetScriptCustomDll().c_str());

        hModule = G3MPFHOOK_ORIGINAL_FUNC(LoadLibraryA)(G3MPFHook::GetInstance()->GetScriptCustomDll().c_str());

        G3MPFHook::GetInstance()->SetScriptTakeCustomDll(false);
        G3MPFHook::GetInstance()->SetScriptCustomDll("");
    }
    else
        hModule = G3MPFHOOK_ORIGINAL_FUNC(LoadLibraryA)(lpLibFileName);

    return hModule;
}
G3MPFHOOK_ASSIGN_DETOUR(LoadLibraryA);

eCEngineComponentBase* __fastcall G3MPFHOOK_DEF_FUNC(FindModule)(eCModuleAdmin* thisPtr, void* edx, const bCString& param_1)
{
    //printf("called, param1: %s\n", param_1);
    return G3MPFHOOK_ORIGINAL_FUNC(FindModule)(thisPtr, param_1);
}
G3MPFHOOK_ASSIGN_DETOUR(FindModule);

//bool __fastcall G3MPFHOOK_DEF_FUNC(ExecuteCallbackType)(bCMessageAdmin* thisPtr, void* edx, bEMessageCallbackPriority param_2, bEMessageTypes param_3, char* param_4, char* param_5, char* param_6, int param_7, int param_8)
//{
//    if (param_2 == 0)
//    {
//        Logger::LogIPC(LOG_INFO, param_4);
//    }
//
//    //printf("Called!\n");
//    //printf("param_1 = %p\n", thisPtr);
//    //printf("param_2 = %d\n", (int)param_2);
//    //printf("param_3 = %d\n", (int)param_3);
//    //printf("param_4 = %s\n", param_4);
//    //printf("param_5 = %s\n", param_5);
//    //printf("param_6 = %s\n", param_6);
//    //printf("param_7 = %d\n", param_7);
//    //printf("param_8 = %d\n", param_8);
//    
//    return G3MPFHOOK_ORIGINAL_FUNC(ExecuteCallbackType)(thisPtr, param_2, param_3, param_4, param_5, param_6, param_7, param_8);
//}
//G3MPFHOOK_ASSIGN_DETOUR(ExecuteCallbackType);

ULONGLONG __cdecl G3MPFHOOK_DEF_FUNC(FUN_1000d010)(void* param_1, CHAR param_2[MAX_PATH], unsigned int param_3)
{
    //printf("=================================================\n");
    //printf("Called!\n");

    //printf("param_1 dumpbytes:\n");
    //Utils::dumpbytes((const char*)param_1, 64, 0, 16, true);

    //printf("param_2 dumpbytes:\n");
    //Utils::dumpbytes((const char*)param_2, 64, 0, 16, true);

    //printf("param_3 = %p\n", &param_3);


    //printf("param_3 dumpbytes:\n");
    //Utils::dumpbytes((const char*)&param_3, 64, 0, 16, true);


    return G3MPFHOOK_ORIGINAL_FUNC(FUN_1000d010)(param_1, param_2, param_3);
}
G3MPFHOOK_ASSIGN_DETOUR(FUN_1000d010);

#define ENABLE_GAME_SPLASH 0 // todo: make this an option

void __fastcall G3MPFHOOK_DEF_FUNC(CreateSplashImage)(eCApplication* thisPtr, void* edx)
{
    printf("CreateSplashImage called\n");

#if ENABLE_GAME_SPLASH
    G3MPFHOOK_ORIGINAL_FUNC(CreateSplashImage)(thisPtr);
#else
    return;
#endif
}
G3MPFHOOK_ASSIGN_DETOUR(CreateSplashImage);

//CFFDirCacheEntry* __fastcall G3MPFHOOK_DEF_FUNC(GetDirCacheEntry)(CFFFileSystemModule* thisPtr, void* edx, void* str)
//{
//    CFFDirCacheEntry* res = G3MPFHOOK_ORIGINAL_FUNC(GetDirCacheEntry)(thisPtr, str);
//
//    //printf("Called, res: %p\n", res);
//
//    //if (res)
//    //{
//    //    Utils::dumpbytes((char*)res, 128, 0, 16, true);
//    //    G3MPFHook::GetInstance()->ThrowMessage("a");
//    //}
//
//    return res;
//}
//G3MPFHOOK_ASSIGN_DETOUR(GetDirCacheEntry);