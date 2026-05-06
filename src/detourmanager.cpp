#include "pch.h"

DetourManager& DetourManager::GetInstance()
{
	static DetourManager instance;
	return instance;
}

bool DetourManager::RegisterDetour(Detour detour)
{
    uintptr_t addr = GetFunctionAddress(detour.module_name.c_str(), detour.rva, detour.export_name);
    if (!addr)
    {
        printf("[G3MPFHook] Failed to get function address!\n");
        return FALSE;
    }

    if (detour.rva == NULL)
        detour.rva = addr;

    *detour.orig_func = (PVOID)addr;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(detour.orig_func, detour.hook_func);

	LONG commitRes = DetourTransactionCommit();
    if (commitRes != NO_ERROR)
    {
        Logger::LogIPC(LOG_ERROR, "Failed to detour \"%s\"", detour.func_name.c_str());
        Logger::LogIPC(LOG_ERROR, "DetourTransactionCommit returned %ld", commitRes);
        return FALSE;
    }

    printf("[G3MPFHook] Hooked \"%s\" at RVA 0x%08x in module \"%s\"\n", detour.func_name.c_str(), detour.rva, detour.module_name.c_str());
    m_registeredDetours.push_back(detour);

    return TRUE;
}

void DetourManager::SetupPredefDetours()
{
#undef G3MPFHOOK_DETOUR_ENTRY
#define G3MPFHOOK_DETOUR_ENTRY G3MPFHOOK_REGISTER_ONLY_ENTRY
#undef G3MPFHOOK_DETOUR_ENTRY_PROC
#define G3MPFHOOK_DETOUR_ENTRY_PROC G3MPFHOOK_REGISTER_ONLY_ENTRY_PROC
    G3MPFHOOK_DETOUR_LIST
#undef G3MPFHOOK_DETOUR_ENTRY
#undef G3MPFHOOK_DETOUR_ENTRY_PROC
}

std::vector<Detour> DetourManager::GetRegisteredDetours()
{
    return m_registeredDetours;
}

uintptr_t DetourManager::GetFunctionAddress(const char* moduleName, uintptr_t rva, const char* exportName)
{
	HMODULE module = GetModuleHandleA(moduleName);
	if (!module)
	{
		printf("[G3MPFHook] Module \"%s\" not found!\n", moduleName);
		return 0;
	}

    if (exportName) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(GetProcAddress(module, exportName));
        printf("%s::%s@0x%p\n", moduleName, exportName, (void*)addr);
        return addr;
    }
    else if (rva != 0) {
        uintptr_t baseAddress = reinterpret_cast<uintptr_t>(module);
        return baseAddress + rva;
    }

    return 0;
}

void DetourManager::DetachAllDetours()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    for (auto it = m_registeredDetours.begin(); it != m_registeredDetours.end();)
    {
        Detour& detour = *it;

        if (detour.orig_func)
            DetourDetach(detour.orig_func, detour.hook_func);

        detour.orig_func = nullptr;
        detour.hook_func = nullptr;

        it = m_registeredDetours.erase(it);
    }

    LONG err = DetourTransactionCommit();
    if (err != NO_ERROR) {
        printf("Failed to detach all hooks, error: %d\n", err);
    }
}

DetourManager::DetourManager() {}

DetourManager::~DetourManager()
{
    DetachAllDetours();
}
