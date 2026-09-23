#pragma once

#ifndef DETOURMANAGER_H
#define DETOURMANAGER_H

#include "pch.h"

namespace Symbols
{
	namespace Engine {
		inline constexpr char SetRootPath[]			= "?SetRootPath@eCVirtualFileSystem@@QAEXABVbCString@@@Z";						// eCVirtualFileSystem::SetRootPath(const bCString&)
		inline constexpr char AddDirToCache[]		= "?AddDirToCache@eCVirtualFileSystem@@QAEIABVbCString@@AAI@Z";					// eCVirtualFileSystem::AddDirToCache(const bCString&, GEUInt&)
		inline constexpr char LocAdminRead[]		= "?Read@eCLocAdmin@@QAE_NXZ";													// eCLocAdmin::Read()
		inline constexpr char FindModule[]			= "?FindModule@eCModuleAdmin@@QBEPAVeCEngineComponentBase@@ABVbCString@@@Z";	// eCModuleAdmin::FindModule(const bCString&)
		inline constexpr char CreateSplashScreen[]	= "?CreateSplashScreen@eCApplication@@SGXXZ";									// eCApplication::CreateSplashScreen()
		inline constexpr char InputDispatch[]		= "?Dispatch@eCInputDispatcher@@UAEXXZ";									// eCInputDispatcher::Dispatch()
	}

	namespace Game {
		inline constexpr char LoadAllScriptDLLs[]	= "?LoadAllScriptDLLs@gCScriptAdmin@@UAE_NXZ";										// gCScriptAdmin::LoadAllScriptDLLs()
		inline constexpr char WorldDoLoadData[]		= "?DoLoadData@gCWorld@@MAE?AW4eEResult@eCProcessibleElement@@AAVbCIStream@@@Z";	// gCWorld::DoLoadData(bCIStream&)
	}

	namespace FileSystem {
		inline constexpr char MountPackFile[] = "?MountPackFile@CFFFileSystemModule@@UAE_NABVbCString@@0@Z"; // CFFFileSystemModule::MountPackFile(const bCString&, const bCString&)
	}

	namespace Kernel32 {
		inline constexpr char LoadLibraryA[] = "LoadLibraryA"; // LoadLibraryA(LPCSTR)
	}
}

namespace RVA {
	namespace Game {
		inline constexpr uintptr_t FUN_1008FC40 = 0x8FC40;
	}
}

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

#define G3MPFHOOK_REGISTER_ONLY_ENTRY(RVA, MODULE_NAME, CALL_CONV, FUNC_NAME, RET_TYPE, ...) \
	G3MPFHOOK_REGISTER_ONLY(RVA, MODULE_NAME, FUNC_NAME)

#define G3MPFHOOK_DETOUR_ENTRY(RVA, MODULE_NAME, CALL_CONV, FUNC_NAME, RET_TYPE, ...) 	\
	G3MPFHOOK_MAKE_DETOUR_FUNC(RET_TYPE, CALL_CONV, FUNC_NAME, __VA_ARGS__)

#define G3MPFHOOK_REGISTER_ONLY_ENTRY_PROC(EXPORT_NAME, MODULE_NAME, CALL_CONV, FUNC_NAME, RET_TYPE, ...) \
    G3MPFHOOK_REGISTER_ONLY_PROC(EXPORT_NAME, MODULE_NAME, FUNC_NAME)

#define G3MPFHOOK_DETOUR_ENTRY_PROC(EXPORT_NAME, MODULE_NAME, CALL_CONV, FUNC_NAME, RET_TYPE, ...) \
    G3MPFHOOK_MAKE_DETOUR_FUNC(RET_TYPE, CALL_CONV, FUNC_NAME, __VA_ARGS__)

#define G3MPFHOOK_DETOUR_LIST \
    /*							SYM/RVA									MODULE_NAME			CALL_CONV		FUNC_NAME			RET_TYPE							... */												\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Engine::SetRootPath,			"Engine.dll",		__thiscall,		SetRootPath,		void,								void*, const bCString&);							\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Engine::AddDirToCache,		"Engine.dll",		__thiscall,		AddDirToCache,		GEUInt,								eCVirtualFileSystem*, const bCString&, GEUInt&);	\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Game::LoadAllScriptDLLs,		"Game.dll",			__thiscall,		LoadAllScriptDLLs,	BOOL,								void*);												\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Game::WorldDoLoadData,		"Game.dll",			__thiscall,		WorldDoLoadData,	eCProcessibleElement::eEResult, 	gCWorld*, bCIStream&);								\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Engine::LocAdminRead,			"Engine.dll",		__thiscall,		LocAdminRead,		GEBool,								eCLocAdmin*);										\
	G3MPFHOOK_DETOUR_ENTRY		(RVA::Game::FUN_1008FC40,				"Game.dll",			__fastcall,		FUN_1008FC40,		void,								CFFGFCView*);										\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Engine::CreateSplashScreen,	"Engine.dll",		__thiscall,		CreateSplashImage,	void,								eCApplication*);									\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Engine::InputDispatch,		"Engine.dll",		__thiscall,		InputDispatch,		void,								eCInputDispatcher*);								\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::FileSystem::MountPackFile,	"FileSystem.dll",	__thiscall,		MountPackFile,		BOOL,								void*, const bCString&, const bCString&);			\
	G3MPFHOOK_DETOUR_ENTRY_PROC	(Symbols::Kernel32::LoadLibraryA,		"kernel32.dll",		WINAPI,			LoadLibraryA,		HMODULE,							LPCSTR);											\
	

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
