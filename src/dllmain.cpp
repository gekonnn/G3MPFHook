#include "pch.h"

#define D3D_HK_ENDSCENE 0
#define D3D_HK_PRESENT 1

G3MPFHook* g3mpfhook;

// messed up completely, ignore
// todo: move it somewhere else bruh
BOOL __declspec(dllexport) MountStringtable(const std::string& stPath)
{
    printf("G3MPFHook] Mounting stringtable \"%s\"\n", stPath.c_str());

    std::ifstream file(stPath, std::ios::binary);
    if (!file) {
        printf("Failed to open file\n");
        return 1;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (buffer.size() >= 2 && buffer[0] == '\xFF' && buffer[1] == '\xFE') {
        buffer.erase(buffer.begin(), buffer.begin() + 2);
    }

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring utf16_str(reinterpret_cast<const wchar_t*>(buffer.data()), buffer.size() / sizeof(wchar_t));
    std::string utf8_str = converter.to_bytes(utf16_str);

    std::stringstream ss(utf8_str);

    static eCLocAdmin& locAdmin = eCLocAdmin::GetInstance();
    return TRUE;

    bCString currentLanguage = locAdmin.GetCurrentLanguage(); // changes lang to russian?
    
    //std::cout << currentLanguage << std::endl;

    return TRUE;

    std::string line;
    while (std::getline(ss, line))
    {
        size_t eqPos = line.find_first_of('=');
        if (eqPos == std::string::npos)
            continue;

        std::string strKey = (line.substr(0, eqPos).c_str());
        std::string strValue = (line.substr(eqPos + 1).c_str());

        const bCString& Key(strKey.c_str());
        const bCString& Value(strValue.c_str());

        eCLocTable::SEntry Entry;
        Entry.Text = Value;
        Entry.StageDirection = bCUnicodeString();

        //if (eCLocAdmin::GetInstance().SetString(Key, Entry))
        if (true)
        {
            //std::cout << strKey << " added to the stringtable" << std::endl;
        }
        else
        {
            std::cout << "Failed to add " << strValue << " into the stringtable" << std::endl;
        }
    }

    printf("==== StringTable mount attempted =====\n");
    printf("lang : %s\n", currentLanguage.GetText());
    printf("CurrentLanguage Index : %d\n", eCLocAdmin::GetInstance().FindLanguageIndex(currentLanguage));

    // fixes the language changing to another when setting strings
    eCLocAdmin::GetInstance().SetCurrentLanguage(currentLanguage); // doesn't

    return TRUE;
}

BOOL WINAPI Attach(HMODULE hModule) {
    AllocConsole();

    FILE* pCout;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    freopen_s(&pCout, "CONOUT$", "w", stderr);
    freopen_s(&pCout, "CONIN$", "r", stdin);

    HWND hWnd = GetConsoleWindow();
    if (hWnd != nullptr) {
        ShowWindow(hWnd, SW_HIDE);
    }

    g3mpfhook = G3MPFHook::GetInstance();
    g3mpfhook->PostInit(hModule);

    return TRUE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    if (DetourIsHelperProcess())
        return TRUE;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ::DisableThreadLibraryCalls(hModule);
        Attach(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}