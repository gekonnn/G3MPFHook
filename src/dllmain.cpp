#include "pch.h"

DWORD WINAPI Attach(LPVOID parameter) {
    HMODULE hModule = static_cast<HMODULE>(parameter);
    HMODULE pinnedModule = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCSTR>(hModule), &pinnedModule))
        return 1;

    G3MPFHook* g3mpfhook = G3MPFHook::GetInstance();
    g3mpfhook->PostInit(hModule);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (DetourIsHelperProcess())
        return TRUE;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ::DisableThreadLibraryCalls(hModule);
        if (HANDLE hThread = CreateThread(nullptr, 0, Attach, hModule, 0, nullptr))
            CloseHandle(hThread);
        else
            return FALSE;
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
