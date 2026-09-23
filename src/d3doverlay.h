#pragma once

#include "pch.h"

#define message_x 16
#define message_y 16
#define message_padding 8
#define message_font "Consolas"
#define message_height 20
#define message_bg_alpha 150

#define alt_message_x 6
#define alt_message_y 6
#define alt_message_font "Arial"
#define alt_message_height 14

#define IMGUI_OVERLAY_TOGGLE_KEY VK_F1

typedef HRESULT(__stdcall* Present_t)(LPDIRECT3DDEVICE9, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(__stdcall* CreateDeviceEx_t)(LPDIRECT3D9EX, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, LPDIRECT3DDEVICE9EX*);

namespace D3DOverlay {
    bool IsCapturingGameInput();
    void SetStartupLogoStatus(const char* status);
    void FadeOutStartupLogo();

    struct ScreenMessage
    {
        bool enabled = false;

        std::string text;

        float duration = 0.0f;
        std::chrono::steady_clock::time_point start;

        int x = 0;
        int y = 0;
        int height = 16;
        int padding = 0;

        const char* font = "Arial";

        bool background = false;

        LPD3DXFONT dxFont = nullptr;
    };

    DWORD WINAPI ThreadInit(PVOID pModule);

    extern ScreenMessage dbg_msg;
    extern ScreenMessage message;

    void ShowMessage(ScreenMessage& m, float duration, const char* fmt, ...);
    void DrawScreenMessage(LPDIRECT3DDEVICE9 dev, ScreenMessage& m);

    LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    HRESULT __stdcall hkCreateDeviceEx(LPDIRECT3D9EX pD3D, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* params, D3DDISPLAYMODEEX* fullscreenMode, LPDIRECT3DDEVICE9EX* device);
    HRESULT __stdcall hkPresent(LPDIRECT3DDEVICE9 pDevice, const RECT*, const RECT*, HWND, const RGNDATA*);
}
