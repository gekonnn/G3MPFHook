#pragma once

#ifndef DETOURMANAGER_H
#define DETOURMANAGER_H

#include "pch.h"

#define RVA_SetRootPath			0x29C350	// on detour calls MountPackFile
#define RVA_MountPackFile		0xA180		// 0x1B70
#define RVA_LoadScriptDLL		0x160C30	// debug
#define RVA_LoadAllScriptDLLs	0x160A70	// on detour calls LoadScriptDLL
#define RVA_ImportSector		0x19B580	// debug (for .wrldatasc)
#define RVA_OnCreateWindow		0xBB90		// for window title mod
#define RVA_FUN_1008fc40		0x8FC40		// Used for setting main menu window text (pb/cpt/sb v...), we detour it to add our own text
#define RVA_AddDirToCache		0x29D830
#define RVA_ReadFile			0x2A24B0	// eCConfigFile::ReadFile
#define RVA_Open				0x29B220	// eCVirtualFile::Open
#define RVA_FUN_1000d010		0xD010
#define RVA_FUN_100083b0		0x83B0
#define RVA_ExecuteCallbackType 0x29D60

struct Detour
{
	std::string func_name;
	std::string module_name;
	uintptr_t rva = 0;
	const char* export_name = nullptr;
	PVOID* orig_func;
	PVOID hook_func;
};

#define G3MPFHOOK_HOOKED_FUNC(FUNC_NAME) hk##FUNC_NAME
#define G3MPFHOOK_ORIGINAL_FUNC(FUNC_NAME) o##FUNC_NAME
#define G3MPFHOOK_DEF_FUNC(FUNC_NAME) hkDef##FUNC_NAME
#define G3MPFHOOK_FUNC_T(FUNC_NAME) FUNC_NAME##_t

#define G3MPFHOOK_ASSIGN_DETOUR(FUNC_NAME) \
	G3MPFHOOK_FUNC_T(FUNC_NAME) G3MPFHOOK_HOOKED_FUNC(FUNC_NAME) = (G3MPFHOOK_FUNC_T(FUNC_NAME))G3MPFHOOK_DEF_FUNC(FUNC_NAME)

#define G3MPFHOOK_MAKE_DETOUR_FUNC(RET_TYPE, CALL_CONV, FUNC_NAME, ...) 				\
	typedef RET_TYPE (CALL_CONV* G3MPFHOOK_FUNC_T(FUNC_NAME))(__VA_ARGS__); 			\
	inline G3MPFHOOK_FUNC_T(FUNC_NAME) G3MPFHOOK_ORIGINAL_FUNC(FUNC_NAME) = nullptr; 	\
	extern G3MPFHOOK_FUNC_T(FUNC_NAME) G3MPFHOOK_HOOKED_FUNC(FUNC_NAME) 				\

#define G3MPFHOOK_REGISTER_DETOUR(RVA, MODULE_NAME, RET_TYPE, CALL_CONV, FUNC_NAME, ...) 	\
	G3MPFHOOK_MAKE_DETOUR_FUNC(RET_TYPE, CALL_CONV, FUNC_NAME, __VA_ARGS__) 				\
	G3MPFHOOK_REGISTER_ONLY(RVA, MODULE_NAME, FUNC_NAME)

#define G3MPFHOOK_REGISTER_ONLY_PROC(EXPORT_NAME, MODULE_NAME, FUNC_NAME) \
    Detour det##FUNC_NAME{ #FUNC_NAME, MODULE_NAME, 0, EXPORT_NAME,       \
        reinterpret_cast<PVOID*>(&G3MPFHOOK_ORIGINAL_FUNC(FUNC_NAME)),    \
        reinterpret_cast<PVOID>(G3MPFHOOK_HOOKED_FUNC(FUNC_NAME)) };      \
    DetourManager::GetInstance().RegisterDetour(det##FUNC_NAME)

#define G3MPFHOOK_REGISTER_ONLY(RVA, MODULE_NAME, FUNC_NAME)			\
    Detour det##FUNC_NAME{ #FUNC_NAME, MODULE_NAME, RVA, nullptr,		\
        reinterpret_cast<PVOID*>(&G3MPFHOOK_ORIGINAL_FUNC(FUNC_NAME)),	\
        reinterpret_cast<PVOID>(G3MPFHOOK_HOOKED_FUNC(FUNC_NAME)) };	\
    DetourManager::GetInstance().RegisterDetour(det##FUNC_NAME)

#define G3MPFHOOK_REGISTER_ONLY_ENTRY(RVA, MODULE_NAME, RET_TYPE, CALL_CONV, FUNC_NAME, ...) \
	G3MPFHOOK_REGISTER_ONLY(RVA, MODULE_NAME, FUNC_NAME)

#define G3MPFHOOK_DETOUR_ENTRY(RVA, MODULE_NAME, RET_TYPE, CALL_CONV, FUNC_NAME, ...) 	\
	G3MPFHOOK_MAKE_DETOUR_FUNC(RET_TYPE, CALL_CONV, FUNC_NAME, __VA_ARGS__)

#define G3MPFHOOK_REGISTER_ONLY_ENTRY_PROC(EXPORT_NAME, MODULE_NAME, RET_TYPE, CALL_CONV, FUNC_NAME, ...) \
    G3MPFHOOK_REGISTER_ONLY_PROC(EXPORT_NAME, MODULE_NAME, FUNC_NAME)

#define G3MPFHOOK_DETOUR_ENTRY_PROC(EXPORT_NAME, MODULE_NAME, RET_TYPE, CALL_CONV, FUNC_NAME, ...) \
    G3MPFHOOK_MAKE_DETOUR_FUNC(RET_TYPE, CALL_CONV, FUNC_NAME, __VA_ARGS__)

#define G3MPFHOOK_DETOUR_LIST \
    /*					   RVA								MODULE_NAME			RET_TYPE,				CALL_CONV,		FUNC_NAME,				... */												\
	G3MPFHOOK_DETOUR_ENTRY(RVA_MountPackFile,				"FileSystem.dll",	BOOL,					__thiscall,		MountPackFile,			void*, const bCString&, const bCString&);			\
	G3MPFHOOK_DETOUR_ENTRY(RVA_SetRootPath,					"Engine.dll",		void,					__thiscall,		SetRootPath,			void*, const bCString&);							\
	G3MPFHOOK_DETOUR_ENTRY(RVA_AddDirToCache,				"Engine.dll",		GEUInt,					__thiscall,		AddDirToCache,			eCVirtualFileSystem*, const bCString&, GEUInt&);	\
	G3MPFHOOK_DETOUR_ENTRY(RVA_LoadAllScriptDLLs,			"Game.dll",			BOOL,					__thiscall,		LoadAllScriptDLLs,		void*);												\
	G3MPFHOOK_DETOUR_ENTRY(RVA_FUN_1008fc40,				"Game.dll",			void,					__fastcall,		FUN_1008FC40,			CFFGFCView*);										\
	G3MPFHOOK_DETOUR_ENTRY(0x1d570,							"Engine.dll",		eCEngineComponentBase*,	__thiscall,		FindModule,				eCModuleAdmin*, const bCString&);					\
	G3MPFHOOK_DETOUR_ENTRY(0xd010,							"FileSystem.dll",	ULONGLONG,				__cdecl,		FUN_1000d010,			void*, CHAR[MAX_PATH], unsigned int);				\
	G3MPFHOOK_DETOUR_ENTRY(0xbaa0,							"Engine.dll",		void,					__thiscall,		CreateSplashImage,		eCApplication*);									\
	G3MPFHOOK_DETOUR_ENTRY_PROC("LoadLibraryA",				"kernel32.dll",		HMODULE,				WINAPI,			LoadLibraryA,			LPCSTR);
//	G3MPFHOOK_DETOUR_ENTRY(0xac20,							"FileSystem.dll",	CFFDirCacheEntry*,		__thiscall,		GetDirCacheEntry,		CFFFileSystemModule*, void*);						\
//	G3MPFHOOK_DETOUR_ENTRY(RVA_ExecuteCallbackType,			"SharedBase.dll",	bool,			__thiscall,		ExecuteCallbackType,	bCMessageAdmin*, bEMessageCallbackPriority, bEMessageTypes, char*, char*, char*, int, int);

class DetourManager
{
public:
	static DetourManager& GetInstance();
	bool RegisterDetour(Detour detour);
	void SetupPredefDetours();

	std::vector<Detour> GetRegisteredDetours();
	static uintptr_t GetFunctionAddress(const char* moduleName, uintptr_t rva = 0, const char* exportName = nullptr);
private:
	std::vector<Detour> m_registeredDetours;

	void DetachAllDetours();

	DetourManager();
	~DetourManager();
};

G3MPFHOOK_DETOUR_LIST

#endif