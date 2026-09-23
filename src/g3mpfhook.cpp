#include "pch.h"

#include <cctype>
#include <cwctype>
#include <filesystem>
#include <string_view>

static std::vector<std::string> archive_generations = { "pak", "cpt", "mod", "nod" };

G3MPFHook* G3MPFHook::GetInstance()
{
    static G3MPFHook instance;
    return &instance;
}

G3MPFHook::G3MPFHook()
{
	zmq_connect_socket();
}

void G3MPFHook::PostInit(HMODULE hModule)
{
    this->hModule = hModule;

    const HANDLE hD3D9Thread = CreateThread(nullptr, NULL, D3DOverlay::ThreadInit, hModule, NULL, nullptr);

    Logger::LogIPC(LOG_INFO, "G3MPFHook injected successfully");
    GE_MESSAGEF_INFO(5, (char*)"Hello from G3MPFHOOK!");

    g3mmpinf_ReadFromSharedMem();

    if (g3mmpinf_initialized(session_pinf)) {
        Logger::LogIPC(LOG_INFO, "G3MMPINF read successfully");
        if (session_pinf.VERSION != G3MMPINF_VER) {
            ThrowMessage("G3MMPINF version mismatch!\n(received) 0x" + Utils::uint16_to_hex(session_pinf.VERSION) + " != 0x" + Utils::uint16_to_hex(G3MMPINF_VER), MB_ICONWARNING);
        }
    }
    //else {
    //    ThrowCriticalError("Failed to read G3MMPINF.", true);
    //}

    g3mmpinf_handle(session_pinf);

    if (game_path.empty())
        game_path = get_exec_directory();

    if (!populate_mount_list())
        ThrowCriticalError("Failed to populate mountlist from mountlist.ini", false);

    DetourManager::GetInstance().SetupPredefDetours();

    Logger::LogIPC(LOG_INFO, "All ready");
}

G3MPFHook::~G3MPFHook()
{
    if (g3mmpinf_initialized(session_pinf)) {
        g3mmpinf_free(&session_pinf);
    }
}

void G3MPFHook::zmq_send_message(zmq_msg_struct msg)
{
	if (!zmq_sock->handle())
		return;

	size_t outSize;
	char* serialized = zms_write(msg, &outSize);

	zmq::message_t zmq_msg(outSize);
	memcpy(zmq_msg.data(), serialized, outSize);
	zmq_sock->send(zmq_msg, zmq::send_flags::none);

	free(serialized);
}

BOOL G3MPFHook::IsValidArchiveFile(const std::string& fileName)
{
    std::string fileExt = fileName.substr(fileName.find_last_of('.') + 1, fileName.length());

    for (const std::string& gen : archive_generations)
    {
        std::string gen_prefix = std::string(1, gen.at(0));
        if (fileExt == gen
            || std::regex_match(fileExt, std::regex(gen_prefix + "\\d\\d"))
            || std::regex_match(fileExt, std::regex(gen_prefix + "\\dx")))
        {
            return TRUE;
        }
    }

    return FALSE;
}

g3mmmod_t G3MPFHook::DetectModFileType(std::string filepath)
{
    std::string fileName = Utils::to_lower(filepath.substr(filepath.find_last_of("/\\") + 1));
    std::string fileExt = fileName.substr(fileName.find_last_of('.') + 1, fileName.length());
    std::string fileNameNoExt = fileName.substr(0, fileName.find_last_of("."));

    if (fileExt == "dll")
        return MODT_SCRIPT;
    else if (fileName == "stringtable.ini")
        return MODT_STRINGTABLE;
    else if (fileExt == "wrldatasc")
        return MODT_LRTPLDATASC;
    else if (fileExt == "ini")
        return MODT_INI;
    else if (IsValidArchiveFile(filepath))
        return MODT_ARCHIVE;

    return MODT_UNKNOWN;
}

const std::vector<std::pair<std::string, std::string>>& G3MPFHook::GetMountList()
{
    return mountlist;
}

std::string G3MPFHook::GetMountPoint(const std::string& archive_name)
{
    if (!IsValidArchiveFile(archive_name)) return "";

    std::string fileName = archive_name.substr(archive_name.find_last_of("/\\") + 1);
    std::string fileNameNoExt = fileName.substr(0, fileName.find_last_of("."));
    std::string mount_point = "#G3:/";
    std::string mount_point_data = "Data/" + fileNameNoExt;

    for (const auto& [mountKey, mountValue] : mountlist) {
        if (mountKey == mount_point_data ||
            mountKey == fileNameNoExt)
        {
            mount_point += mountValue;
            break;
        }
    }

    if (mount_point == "#G3:/") {
        if (fileNameNoExt == "Projects_compiled") {
            mount_point += "Data/Projects";
        }
        else {
            mount_point += "Data/" + fileNameNoExt;
        }
    }

    return mount_point;
}

BOOL G3MPFHook::MountPackFile(const bCString& mountPoint, const bCString& packDir)
{
    CFFFileSystemModule& fsModule = CFFFileSystemModule::GetInstance();
    BOOL result = fsModule.MountPackFile(mountPoint, packDir);

    return result;
}

BOOL G3MPFHook::MountPackFile(const std::string mountPoint, const std::string packDir)
{
    bCString bCStrMountPoint = bCString(mountPoint.c_str());
    bCString bCStrPackDir = bCString(packDir.c_str());
    return MountPackFile(bCStrMountPoint, bCStrPackDir);
}

BOOL G3MPFHook::MountPackFile(const std::string packDir)
{
    return MountPackFile(GetMountPoint(packDir), packDir);
}

// re-orders all packFiles to match the mountlist order and mounts all files
// while adding directories to the cache
BOOL G3MPFHook::MountPackFiles(const std::vector<PackFile>& packFiles)
{
    std::vector<std::pair<std::string, std::vector<PackFile>>> mountorder;

    std::vector<PackFile> remaining_packFiles = packFiles;

    // segregate filenames to their according mountpoints
    for (const auto& [_, mountpoint] : GetMountList())
    {
        std::vector<PackFile> files = {};

        for (auto it = remaining_packFiles.begin(); it != remaining_packFiles.end();)
        {
            const PackFile& packfile = *it;

            if (packfile.mountPoint == "#G3:/" + mountpoint)
            {
                files.push_back(packfile);
                it = remaining_packFiles.erase(it);
            }
            else
                it++;
        }

        mountorder.push_back({ mountpoint, files });
    }

    int mounted = 0;

    for (auto& mount_pair : mountorder)
    {
        bCString mount_point(mount_pair.first.c_str());
        bCString full_mountpoint = "#G3:/" + mount_point;

        for (auto& packfile : mount_pair.second)
        {
            // printf("%s -> %s\n", packfile.path.c_str(), full_mountpoint.GetText());

            bCString pack_dir(packfile.path.c_str());
            BOOL success = MountPackFile(full_mountpoint, pack_dir);
            if (success)
                mounted++;
        }

        static GEUInt adddir_param_2 = 0;

        eCVirtualFileSystem::GetInstance().AddDirToCache(mount_point, adddir_param_2);
    }

    Logger::LogIPC(LOG_INFO, "Mounted %d archives successfully", mounted);

    return TRUE;
}

bool G3MPFHook::MountAllFilesInDirectory(std::string folder_path, bool gather)
{
    if (folder_path.back() != '\\' && folder_path.back() != '/')
        folder_path.push_back('\\');

    std::string ffdPath = folder_path + "*";

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    hFind = FindFirstFileA(ffdPath.c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        std::string err = "Failed to mount directory:\n\"" + folder_path + "\"";
        ThrowMessage(err, MB_ICONERROR);
        return false;
    }

    do
    {
        std::string fileName(ffd.cFileName);
        std::string filepath = folder_path + ffd.cFileName;

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        g3mmmod_t mod_file_type = DetectModFileType(filepath);
        switch (mod_file_type)
        {
        case MODT_ARCHIVE: {
            if (gather)
                ext_packfile_queue.push_back({ filepath, GetMountPoint(filepath) });
            else
                MountPackFile(filepath);
            break;
        }
        case MODT_SCRIPT: {
            if (gather)
                ext_script_queue.push_back({ filepath });
            else
                LoadScriptDLL(filepath);
            break;
        }
        case MODT_INI: {
            if (gather)
                ext_ini_queue.push_back({ filepath });
            else
                CopyIniToGameVfs(filepath);
            break;
        }
        case MODT_LRTPLDATASC: {
            ext_wrldatasc_queue.push_back(filepath);
            break;
        }
        case MODT_STRINGTABLE: {
            ext_stringtable_queue.push_back(filepath);
            break;
        }
        default: break;
        }
    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);

    return true;
}

BOOL G3MPFHook::LoadScriptDLL(const std::string& scriptPath)
{
    printf("[G3MPFHook] LoadScriptDLL called on %s\n", scriptPath.c_str());

    gCScriptAdminExt& scriptAdmin = GetScriptAdminExt();

    fLoadLibraryA_customDllPath = scriptPath;
    fLoadLibraryA_takeCustomDll = true;

    BOOL result = scriptAdmin.LoadScriptDLL(bCString(scriptPath.c_str()));

    printf("res = %d\n", result);

    return result;
}

void G3MPFHook::LoadAllScriptDLLsInQueue()
{
    for (auto it = ext_script_queue.begin(); it != ext_script_queue.end();)
    {
        const std::string& dll_path = *it;

        LoadScriptDLL(dll_path);
        it = ext_script_queue.erase(it);
    }
}

void G3MPFHook::CopyAllIniFilesInQueue()
{
    for (const std::string& iniPath : ext_ini_queue)
    {
        if (!CopyIniToGameVfs(iniPath))
        {
            Logger::LogIPC(
                LOG_ERROR,
                "Failed to copy INI \"%s\" into the game VFS",
                iniPath.c_str());
        }
    }

    ext_ini_queue.clear();
}

bool G3MPFHook::CopyIniToGameVfs(const std::string& iniPath)
{
    std::string destination =
        "ini\\" + std::filesystem::path(iniPath).filename().string();

    eCVirtualFileSystem& vfs = eCVirtualFileSystem::GetInstance();
    bCString destinationPath(destination.c_str());

    IFFVirtualFile* existing = vfs.CreateFileA(
        destinationPath,
        bEFileCreationMode_OpenExisting,
        1);

    if (existing)
    {
        existing->Release();
        return true;
    }

    std::ifstream source(iniPath, std::ios::binary);
    if (!source)
        return false;

    std::vector<char> contents{
        std::istreambuf_iterator<char>(source),
        std::istreambuf_iterator<char>()
    };

    IFFVirtualFile* file = vfs.CreateFileA(
        destinationPath,
        bEFileCreationMode_CreateAlways,
        0);

    if (!file)
        return false;

    GEU32 size = static_cast<GEU32>(contents.size());
    bool success = size == 0 || file->Write(contents.data(), size) == size;

    file->Release();
    return success;
}

void G3MPFHook::ImportWorldDataFiles(gCWorld* world)
{
    auto trim = [](std::string value) {
        auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
        auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c); }).base();
        return first < last ? std::string(first, last) : std::string();
    };

    for (const std::string& path : ext_wrldatasc_queue)
    {
        std::ifstream file(path);
        if (!file)
        {
            Logger::LogIPC(LOG_ERROR, "Failed to read world data file \"%s\"", path.c_str());
            continue;
        }

        bool sectorList = false;
        std::string line;

        while (std::getline(file, line))
        {
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
                line.erase(0, 3);

            line = trim(line);

            if (line.empty() || line.front() == ';' || line.front() == '#')
                continue;

            if (line.front() == '[')
            {
                sectorList = Utils::to_lower(line) == "[sector.list]";
                continue;
            }

            if (!sectorList)
                continue;

            size_t separator = line.find('=');
            if (separator == std::string::npos)
                continue;

            std::string sectorName = trim(line.substr(0, separator));
            std::string value = Utils::to_lower(trim(line.substr(separator + 1)));
            if (sectorName.empty())
                continue;

            bool enabled = value == "true" || value == "1" || value == "yes" || value == "on";
            bCString sectorNameString(sectorName.c_str());
            gCSector* sector = world->GetSector(sectorNameString);

            if (!sector)
                sector = world->ImportSector(sectorNameString);

            if (!sector)
            {
                Logger::LogIPC(LOG_ERROR, "Failed to import sector \"%s\" from \"%s\"", sectorName.c_str(), path.c_str());
                continue;
            }

            sector->Enable(enabled);
            //Logger::LogIPC(LOG_INFO, "Applied sector \"%s\" from \"%s\"", sectorName.c_str(), path.c_str());
        }
    }
}

void G3MPFHook::MergeAllStringtables(eCLocAdmin& locAdmin)
{
    GEInt languageCount = locAdmin.GetLanguageCount();
    std::vector<bCString> languages;
    languages.reserve(languageCount);

    for (GEInt i = 0; i < languageCount; ++i)
        languages.push_back(locAdmin.FindLanguage(i));

    auto trim = [](std::wstring_view value) {
        while (!value.empty() && std::iswspace(value.front()))
            value.remove_prefix(1);
        while (!value.empty() && std::iswspace(value.back()))
            value.remove_suffix(1);
        return value;
    };

    ULONGLONG started = GetTickCount64();
    size_t mergedFiles = 0;
    size_t mergedIds = 0;
    size_t mergedValues = 0;

    for (const std::string& path : ext_stringtable_queue)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            Logger::LogIPC(LOG_ERROR, "Failed to read string table \"%s\"", path.c_str());
            continue;
        }

        std::streamsize fileSize = file.tellg();
        if (fileSize <= 0)
        {
            Logger::LogIPC(LOG_WARNING, "String table \"%s\" is empty", path.c_str());
            continue;
        }

        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> bytes(static_cast<size_t>(fileSize));
        if (fileSize > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), fileSize))
        {
            Logger::LogIPC(LOG_ERROR, "Failed to read string table \"%s\"", path.c_str());
            continue;
        }

        std::wstring contents;

        if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
        {
            contents.reserve((bytes.size() - 2) / 2);
            for (size_t i = 2; i + 1 < bytes.size(); i += 2)
                contents.push_back(static_cast<wchar_t>(bytes[i] | (bytes[i + 1] << 8)));
        }
        else if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
        {
            contents.reserve((bytes.size() - 2) / 2);
            for (size_t i = 2; i + 1 < bytes.size(); i += 2)
                contents.push_back(static_cast<wchar_t>((bytes[i] << 8) | bytes[i + 1]));
        }
        else
        {
            size_t offset = bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF ? 3 : 0;
            int byteCount = static_cast<int>(bytes.size() - offset);
            int charCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(bytes.data() + offset), byteCount, nullptr, 0);
            UINT codePage = CP_UTF8;
            DWORD flags = MB_ERR_INVALID_CHARS;

            if (charCount == 0 && byteCount != 0)
            {
                codePage = CP_ACP;
                flags = 0;
                charCount = MultiByteToWideChar(codePage, flags, reinterpret_cast<const char*>(bytes.data() + offset), byteCount, nullptr, 0);
            }

            if (charCount > 0)
            {
                contents.resize(charCount);
                MultiByteToWideChar(codePage, flags, reinterpret_cast<const char*>(bytes.data() + offset), byteCount, contents.data(), charCount);
            }
        }

        bool stringsSection = false;
        size_t position = 0;
        size_t fileIds = 0;
        size_t fileValues = 0;

        while (position <= contents.size())
        {
            size_t lineEnd = contents.find(L'\n', position);
            if (lineEnd == std::wstring::npos)
                lineEnd = contents.size();

            std::wstring_view line(contents.data() + position, lineEnd - position);
            line = trim(line);
            position = lineEnd + 1;

            if (line.empty() || line.front() == L';' || line.front() == L'#')
                continue;

            if (line.front() == L'[')
            {
                stringsSection = _wcsnicmp(line.data(), L"[LocAdmin_Strings]", line.size()) == 0 && line.size() == 18;
                continue;
            }

            if (!stringsSection)
                continue;

            size_t separator = line.find(L'=');
            if (separator == std::wstring_view::npos)
                continue;

            std::wstring_view keyView = trim(line.substr(0, separator));
            std::wstring_view values = line.substr(separator + 1);
            if (keyView.empty())
                continue;

            std::string key;
            key.reserve(keyView.size());
            bool validKey = true;

            for (wchar_t character : keyView)
            {
                if (character > 0x7F)
                {
                    validKey = false;
                    break;
                }
                key.push_back(static_cast<char>(character));
            }

            if (!validKey)
                continue;

            bCString stringId(key.c_str());
            size_t fieldStart = 0;
            size_t appliedForId = 0;

            for (size_t languageIndex = 0; languageIndex < languages.size(); ++languageIndex)
            {
                size_t textEnd = values.find(L';', fieldStart);
                if (textEnd == std::wstring_view::npos)
                    textEnd = values.size();
                std::wstring_view text = values.substr(fieldStart, textEnd - fieldStart);
                fieldStart = textEnd < values.size() ? textEnd + 1 : values.size();

                size_t directionEnd = values.find(L';', fieldStart);
                if (directionEnd == std::wstring_view::npos)
                    directionEnd = values.size();
                std::wstring_view stageDirection = values.substr(fieldStart, directionEnd - fieldStart);
                fieldStart = directionEnd < values.size() ? directionEnd + 1 : values.size();

                if (text.empty())
                    continue;

                std::wstring textString(text);
                std::wstring stageDirectionString(stageDirection);
                eCLocTable::SEntry entry;
                entry.Text = bCUnicodeString(textString.c_str());
                entry.StageDirection = bCUnicodeString(stageDirectionString.c_str());

                if (locAdmin.SetString(stringId, entry, languages[languageIndex]))
                    ++appliedForId;
            }

            if (appliedForId != 0)
            {
                ++fileIds;
                fileValues += appliedForId;
            }
        }

        ++mergedFiles;
        mergedIds += fileIds;
        mergedValues += fileValues;
    }

    Logger::LogIPC(LOG_INFO, "Merged %u string tables: %u IDs, %u values in %u ms", static_cast<GEU32>(mergedFiles), static_cast<GEU32>(mergedIds), static_cast<GEU32>(mergedValues), static_cast<GEU32>(GetTickCount64() - started));
}

void G3MPFHook::ThrowMessage(const std::string& text, UINT uType, LogLevel lv)
{
    Logger::LogIPC(lv, text.c_str());
    MessageBoxA(NULL, text.c_str(), "G3MPFHook", uType);
}

void G3MPFHook::ThrowCriticalError(const std::string& text, bool _exit, UINT uType)
{
    ThrowMessage(text, uType, LOG_ERROR);
    if (_exit)
        exit(448);
}

void G3MPFHook::SetPackFileGatherMode(bool mode)
{
    gatherFiles = mode;
}

bool G3MPFHook::GetPackFileGatherMode()
{
    return gatherFiles;
}

void G3MPFHook::SetScriptTakeCustomDll(bool mode)
{
    fLoadLibraryA_takeCustomDll = mode;
}

bool G3MPFHook::GetScriptTakeCustomDll()
{
    return fLoadLibraryA_takeCustomDll;
}

std::string& G3MPFHook::GetScriptCustomDll()
{
    return fLoadLibraryA_customDllPath;
}

void G3MPFHook::SetScriptCustomDll(std::string file)
{
    fLoadLibraryA_customDllPath = file;
}

std::vector<PackFile>& G3MPFHook::GetOriginalPackFileQueue()
{
    return orig_packfile_queue;
}

std::vector<PackFile>& G3MPFHook::GetExternalPackFileQueue()
{
    return ext_packfile_queue;
}

void G3MPFHook::AddToPackFileQueue(PackFile file)
{
    orig_packfile_queue.push_back(file);
}

void G3MPFHook::AddToScriptQueue(std::string file)
{
    ext_script_queue.push_back(file);
}

g3mmpinf G3MPFHook::get_pinf()
{
    return session_pinf;
}

void G3MPFHook::zmq_connect_socket()
{
	zmq_ctx = new zmq::context_t(1);
	zmq_sock = new zmq::socket_t(*zmq_ctx, zmq::socket_type::push);
	zmq_sock->connect(ZMQ_DLL_ENDPOINT);
}

BOOL G3MPFHook::populate_mount_list()
{
    std::string filename = game_path + "\\ini\\mountlist.ini";

    std::ifstream file(filename);
    if (!file.is_open())
    {
        ThrowMessage("Failed to read \"" + filename + "\"", MB_ICONERROR);
        return false;
    }

    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line.front() == ';') continue;

        auto sep_pos = line.find_first_of('=');
        if (sep_pos == std::string::npos) continue;

        std::string left = line.substr(0, sep_pos);
        std::string right = line.substr(sep_pos + 1);

        mountlist.push_back({ left, right });
    }

    return true;
}

bool G3MPFHook::g3mmpinf_initialized(const g3mmpinf& pinf)
{
    return pinf.MAGIC != nullptr && memcmp(pinf.MAGIC, G3MMPINF_MAGIC, strlen(G3MMPINF_MAGIC) - 1) == 0;
}

void G3MPFHook::g3mmpinf_handle(const g3mmpinf& pinf)
{
    if (!g3mmpinf_initialized(pinf))
        return;

    if (pinf.INFO.SHOW_CONSOLE == 1)
    {
        AllocConsole();

        FILE* pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        freopen_s(&pCout, "CONOUT$", "w", stderr);
        freopen_s(&pCout, "CONIN$", "r", stdin);
    }

    game_path = std::string(pinf.INFO.GAME_PATH.DATA);

    for (int i = 0; i < pinf.MOD_COUNT; ++i) {
        g3mmmod& mod = pinf.MOD_ARRAY[i];
        printf("[%d]\t %s\n", i, mod.NAME.DATA);

        if (mod.FILE_ARRAY_TYPE == MODFILEARR_T_SELECTED_FILES)
        {
            for (int j = 0; j < mod.FILE_COUNT; ++j) {
                g3mmmodf& modf = mod.FILE_ARRAY[j];
                std::string path = std::string(modf.FILEPATH.DATA);

                Logger::Log(LOG_WARNING, "Mod: %s, file: %s", mod.NAME.DATA, path.c_str());

                if (modf.TYPE == MODT_ARCHIVE)
                {
                    //printf("add %s to queue\n", path.c_str());
                    ext_packfile_queue.push_back({ path, GetMountPoint(path) });
                }
                if (modf.TYPE == MODT_SCRIPT)
                {
                    printf("add %s to queue\n", path.c_str());
                    ext_script_queue.push_back({ path });
                }
                if (modf.TYPE == MODT_INI)
                {
                    ext_ini_queue.push_back({ path });
                }
                if (modf.TYPE == MODT_LRTPLDATASC)
                {
                    ext_wrldatasc_queue.push_back({ path });
                }
                if (modf.TYPE == MODT_STRINGTABLE)
                {
                    ext_stringtable_queue.push_back({ path });
                }
            }
        }
        else if (mod.FILE_ARRAY_TYPE == MODFILEARR_T_ALL_IN_DIRECTORY)
        {
            std::string mod_root_path = std::string(mod.FILES_DIRECTORY.DATA, mod.FILES_DIRECTORY.LENGTH);

            MountAllFilesInDirectory(mod_root_path, true);
        }
    }
}

void G3MPFHook::g3mmpinf_ReadFromSharedMem()
{
    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, MAPPING_FILE_NAME);

    if (g3mmpinf_initialized(session_pinf))
        g3mmpinf_free(&session_pinf);

    session_pinf = {};

    if (hMapFile) {
        char* buf = (char*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
        if (buf) {
            uint32_t size = *(uint32_t*)buf;
            char* data = buf + sizeof(uint32_t);

            printf("g3mmpinf size: %d\n", size);
#if DUMP_G3MMPINF
            printf("g3mmpinf dump:\n");
            Utils::dumpbytes(data, size + sizeof(uint32_t), 0, 16, true, 0);
#endif
            printf("\nNow deserializing\n");
            g3mmpinf_deserialize(data, size, &session_pinf);

            UnmapViewOfFile(buf);
        }
        else {
            ThrowMessage("[g3mmpinf_ReadFromSharedMem] MapViewOfFile failed");
        }
        
        CloseHandle(hMapFile);
    }
    else
    {
        Logger::Log(LOG_WARNING, "[g3mmpinf_ReadFromSharedMem] failed to read g3mmpinf from shared memory");
    }
}

std::string G3MPFHook::get_exec_directory()
{
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
        return "";

    std::string fullPath(path);
    size_t pos = fullPath.find_last_of("\\/");
    if (pos != std::string::npos)
        return fullPath.substr(0, pos);

    return "";
}
