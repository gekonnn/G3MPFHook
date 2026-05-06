#pragma once

#include "pch.h"
#include "g3mmpinf.h"

#include "zmq_msg_struct.h"

#define ZMQ_DLL_ENDPOINT "tcp://127.0.0.1:8447"

#define MAPPING_FILE_NAME "Local\\G3MMPINF_SHARED"

#define MOUNT_EXTERNAL_PAK 1
#define DUMP_G3MMPINF 1

struct PackFile {
	std::string path;
	std::string mountPoint;
};

class G3MPFHook
{
public:
	__declspec(dllexport) static G3MPFHook* GetInstance();

	G3MPFHook(const G3MPFHook&) = delete;
	G3MPFHook& operator=(const G3MPFHook&) = delete;

	void PostInit(HMODULE hModule);

	void zmq_send_message(zmq_msg_struct msg);

	BOOL IsValidArchiveFile(const std::string& fileName);
	g3mmmod_t DetectModFileType(std::string filepath);

	__declspec(dllexport) const std::vector<std::pair<std::string, std::string>>& GetMountList();
	__declspec(dllexport) std::string GetMountPoint(const std::string& archive_name);

	BOOL MountPackFile(const bCString& mountPoint, const bCString& packDir);
	__declspec(dllexport) BOOL MountPackFile(const std::string mountPoint, const std::string packDir);
	__declspec(dllexport) BOOL MountPackFile(const std::string packDir);
	__declspec(dllexport) BOOL MountPackFiles(const std::vector<PackFile>& packFiles);
	__declspec(dllexport) bool MountAllFilesInDirectory(std::string folder_path, bool gather = false);

	__declspec(dllexport) BOOL LoadScriptDLL(const std::string& scriptPath);
	void LoadAllScriptDLLsInQueue();

	__declspec(dllexport) void ThrowMessage(const std::string& text, UINT uType = MB_ICONINFORMATION, LogLevel lv = LOG_INFO);
	__declspec(dllexport) void ThrowCriticalError(const std::string& text, bool _exit = true, UINT uType = MB_ICONERROR);

	void SetPackFileGatherMode(bool mode);
	bool GetPackFileGatherMode();

	void SetScriptTakeCustomDll(bool mode);
	bool GetScriptTakeCustomDll();
	std::string& GetScriptCustomDll();
	void SetScriptCustomDll(std::string file);

	std::vector<PackFile>& GetOriginalPackFileQueue();
	std::vector<PackFile>& GetExternalPackFileQueue();
	void AddToPackFileQueue(PackFile file);
	void AddToScriptQueue(std::string file);

	__declspec(dllexport) g3mmpinf get_pinf();
private:
	// we deliberately declare these zmq properties as pointers and never clean them, 
	// otherwise we'd get "Successful WSASTARTUP not yet performed" crash.
	zmq::context_t* zmq_ctx;
	zmq::socket_t* zmq_sock;

	bool gatherFiles = false;
	std::vector<PackFile> orig_packfile_queue;
	std::vector<PackFile> ext_packfile_queue;
	std::vector<std::string> ext_script_queue;

	bool fLoadLibraryA_takeCustomDll = false;
	std::string fLoadLibraryA_customDllPath;

	g3mmpinf session_pinf;

	std::vector<std::pair<std::string, std::string>> mountlist = {};

	std::string game_path;

	void zmq_connect_socket();

	BOOL populate_mount_list();

	bool g3mmpinf_initialized(const g3mmpinf& pinf);
	void g3mmpinf_handle(const g3mmpinf& pinf);
	void g3mmpinf_ReadFromSharedMem(); // read G3MMPINF from G3MM to fetch all mod files to be loaded

	std::string get_exec_directory();

	HMODULE hModule = NULL;

	G3MPFHook();
	~G3MPFHook();
};

