#pragma once

#define WIN32_LEAN_AND_MEAN

// Windows Header Files
#include <sstream>
#include <iostream>
#include <memory>
#include <regex>
#include <fstream>
#include <codecvt>
#include <map>
#include <chrono>

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <ShlObj.h>
#include <atlbase.h>

#include <d3d9.h>
#include <d3dx9.h>

#include <detours/detours.h>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")