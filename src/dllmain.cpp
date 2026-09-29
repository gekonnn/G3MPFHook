#include "pch.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (DetourIsHelperProcess())
        return TRUE;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        ::DisableThreadLibraryCalls(hModule);

        if (HANDLE hZmqThread = CreateThread(nullptr, 0, G3MPFHook::ConnectZMQThread, hModule, 0, nullptr))
            CloseHandle(hZmqThread);

        if (!G3MPFHook::GetInstance()->EarlyInit(hModule))
            return false;

        if (HANDLE hThread = CreateThread(nullptr, 0, G3MPFHook::LateInitThread, hModule, 0, nullptr))
            CloseHandle(hThread);
        else
            return FALSE;
    }
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
