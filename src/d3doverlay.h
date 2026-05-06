#pragma once

#include "pch.h"

#define D3D_HK_ENDSCENE 0
#define D3D_HK_PRESENT 1

#define IMGUI_OVERLAY_TOGGLE_KEY VK_F1

typedef HRESULT(__stdcall* Present_t)(LPDIRECT3DDEVICE9, const RECT*, const RECT*, HWND, const RGNDATA*);

namespace D3DOverlay {
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
    HRESULT __stdcall hkPresent(LPDIRECT3DDEVICE9 pDevice, const RECT*, const RECT*, HWND, const RGNDATA*);
}