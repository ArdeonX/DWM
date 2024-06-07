
#include "imgui.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tchar.h>


// Win32 Data
static HWND                 g_hWnd = NULL;
static INT64                g_Time = 0;
static INT64                g_TicksPerSecond = 0;
//static ImGuiMouseCursor     g_LastMouseCursor = ImGuiMouseCursor_COUNT;
//static bool                 g_HasGamepad = false;
//static bool                 g_WantUpdateHasGamepad = true;

// Functions
bool    ImGui_ImplWin32_Init(void* hwnd)
{
    if (!::QueryPerformanceFrequency((LARGE_INTEGER*)&g_TicksPerSecond))
        return false;
    if (!::QueryPerformanceCounter((LARGE_INTEGER*)&g_Time))
        return false;

    // Setup back-end capabilities flags
    g_hWnd = (HWND)hwnd;
    ImGuiIO& io = ImGui::GetIO();
    io.ImeWindowHandle = hwnd;

    return true;
}

void    ImGui_ImplWin32_Shutdown()
{
    g_hWnd = (HWND)0;
}

void    ImGui_ImplWin32_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.Fonts->IsBuilt() && "");

    // Setup display size (every frame to accommodate for window resizing)
    RECT rect;
    ::GetClientRect(g_hWnd, &rect);
    io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

    // Setup time step
    INT64 current_time;
    ::QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
    io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
    g_Time = current_time;

    
}

