#include "imgui/imgui.h"
//#include "imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_dx10.h"
#include <d3d11.h>
#include <d3d10_1.h>
#include <d3d10.h>
#include <string>
#include <intrin.h>
#include <Psapi.h>
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3d11.lib")
using namespace std;

std::string string_To_UTF8(const std::string& str)
{
	int nwLen = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	ZeroMemory(pwBuf, nwLen * 2 + 2);

	::MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = (int)(::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL));

	char* pBuf = new char[nLen + 1];
	ZeroMemory(pBuf, nLen + 1);

	::WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr(pBuf);

	delete[]pwBuf;
	delete[]pBuf;

	pwBuf = NULL;
	pBuf = NULL;

	return retStr;
}


VOID DbgPrint(LPCSTR szFormat, ...)
{
	static bool bDbgPrint = false;
	if (!bDbgPrint)
	{
		bDbgPrint = AllocConsole();
		if (bDbgPrint)
		{
			freopen_s((_iobuf**)__acrt_iob_func(0), "conin$", "r", (_iobuf*)__acrt_iob_func(0));
			freopen_s((_iobuf**)__acrt_iob_func(1), "conout$", "w", (_iobuf*)__acrt_iob_func(1));
			freopen_s((_iobuf**)__acrt_iob_func(2), "conout$", "w", (_iobuf*)__acrt_iob_func(2));
		}
	}
	char szBuffer[1024] = { 0 };
	va_list pArgList;
	va_start(pArgList, szFormat);
	_vsnprintf_s(szBuffer, sizeof(szBuffer) / sizeof(char), szFormat, pArgList);
	va_end(pArgList);
	char szOutBuffer[1024] = { 0 };
	sprintf_s(szOutBuffer, "[+] %s \n", szBuffer);
	printf(szOutBuffer);
}


typedef LONG(__stdcall* fnRtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation);
fnRtlGetVersion pRtlGetVersion;

typedef HRESULT(__stdcall* D3DPresentProxy) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef __int64(__fastcall* fnCDWMSwapChain_PresentInternal)(struct CDWMSwapChain* pSwapChain, const struct CRegion* a2, __int64 a3, unsigned int a4, const struct RenderTargetPresentParameters* a5);
typedef __int32(__fastcall* fnCLegacySwapChain_Present)(struct CLegacySwapChain* pSwapChain, unsigned int a1, unsigned int a2, unsigned int a3);
typedef unsigned int(__fastcall* fnCLegacyRenderTarget_GetPresentFlags)(struct CLegacyRenderTarget* a1);


ID3D11Device* g_pD3D11Device = NULL;
ID3D11DeviceContext* g_pD3D11Context = nullptr;
ID3D11RenderTargetView* D3D11RenderTargetView = nullptr;

ID3D10RenderTargetView* D3D10RenderTargetView = NULL;
ID3D10Device* g_pD3D10Device = NULL;
ID3D10Device1* g_pD3D10Device1 = NULL;
IDXGISwapChain* m_pSwapChain;
DWORD SwapChainOffset = 0;
D3DPresentProxy phookD3DPresent = NULL;
fnCDWMSwapChain_PresentInternal PfnCDWMSwapChain_PresentInternal = NULL;
fnCLegacySwapChain_Present PfnCLegacySwapChain_Present = NULL;
fnCLegacyRenderTarget_GetPresentFlags PfnCLegacyRenderTarget_GetPresentFlags = NULL;
bool firstTime = true;


typedef enum __Render_Type
{
	Draw_Rects = 51,
	Draw_Fills,
	Draw_Lines,
	Draw_Texts,
	Draw_Circle,
	Draw_FillCircle

};

typedef struct __RECT2
{
	float left;
	float top;
	float right;
	float bottom;
}RECT2, * PRECT2;

typedef struct __POINT2
{
	float x;
	float y;
}POINT2, * PPOINT2;

typedef struct __Render_Rect
{
	int Render_Try;
	RECT2  point;
	DWORD Color;
}Render_Rect, * PRenderRect;

typedef struct __Render_Line
{
	int Render_Try;
	RECT2  point;
	float thick;
	DWORD Color;
}Render_Line, * PRenderLine;

typedef struct __Render_Text
{
	int Render_Try;
	POINT2  point;
	float font;
	DWORD Color;
	char Text[255];
}Render_Text, * PRenderText;

typedef struct __Render_Cir
{
	int Render_Try;
	POINT2  point;
	float radius;
	DWORD Color;
}Render_Cir, * PRenderCir;

typedef struct __Render
{
	ULONG64 drawindex;
	PUCHAR pMemory1;
	PUCHAR pMemory2;
	PUCHAR pMemory3;
}Render, * PRender;


namespace GSInject
{
	BOOL SendDrvMsg(DWORD Code, PVOID Data, ULONG DataSize)
	{
		return RegSetValueExA(HKEY_CURRENT_USER, NULL, NULL, Code, (CONST BYTE*)Data, DataSize) == 998;
	}

	BOOL DeleteInject()
	{
		return SendDrvMsg(0x1001, NULL, NULL);
	}

	namespace SSDT
	{
		BOOL CreateThread(ULONGLONG StratAddress, ULONG Flags)
		{
			/*
				HIDE_THREAD        2
				ANTI_DEBUG_THREAD  4
			*/

			static struct
			{
				ULONGLONG StratAddress;
				ULONGLONG ThreadType;
			} Structs;

			RtlZeroMemory(&Structs, sizeof(Structs));

			Structs.StratAddress = StratAddress;

			Structs.ThreadType = Flags;

			return SendDrvMsg(0x1002, &Structs, sizeof(Structs));
		}

		BOOL VirtualProtect(ULONGLONG MemoryAddress, ULONGLONG MemorySize, ULONG NewProtect)
		{
			static struct
			{
				ULONGLONG MemoryAddress;
				ULONGLONG MemorySize;
				ULONGLONG NewProtect;
			} Structs;

			RtlZeroMemory(&Structs, sizeof(Structs));

			Structs.MemoryAddress = MemoryAddress;

			Structs.MemorySize = MemorySize;

			Structs.NewProtect = NewProtect;

			return SendDrvMsg(0x1003, &Structs, sizeof(Structs));
		}

		BOOL VirtualQuery(ULONGLONG MemoryAddress, PVOID BufferAddress, ULONGLONG BufferSize)
		{
			static struct
			{
				ULONGLONG MemoryAddress;
				ULONGLONG BufferAddress;
				ULONGLONG BufferSize;
			} Structs;

			RtlZeroMemory(&Structs, sizeof(Structs));

			Structs.MemoryAddress = MemoryAddress;

			Structs.BufferAddress = (ULONGLONG)BufferAddress;

			Structs.BufferSize = BufferSize;

			return SendDrvMsg(0x1004, &Structs, sizeof(Structs));
		}

		ULONGLONG VirtualAllocate(ULONGLONG MemoryAddress, ULONGLONG RegionSize, ULONGLONG HideMemory)
		{
			static struct
			{
				ULONGLONG MemoryAddress;
				ULONGLONG RegionSize;
				ULONGLONG HideMemory;
				ULONGLONG BufferAddress;
			} Structs;

			RtlZeroMemory(&Structs, sizeof(Structs));

			Structs.MemoryAddress = MemoryAddress;

			Structs.RegionSize = RegionSize;

			Structs.HideMemory = HideMemory;

			Structs.BufferAddress = (ULONGLONG)&Structs.BufferAddress;

			return SendDrvMsg(0x1005, &Structs, sizeof(Structs)) ? Structs.BufferAddress : NULL;
		}
	}

	namespace Mouse
	{
		BOOL SimulatedMove(ULONG X, ULONG Y, ULONG Flags)
		{
			static struct
			{
				USHORT UnitId;
				USHORT Flags;
				union {
					ULONG Buttons;
					struct {
						USHORT  ButtonFlags;
						USHORT  ButtonData;
					};
				};
				ULONG RawButtons;
				LONG LastX;
				LONG LastY;
				ULONG ExtraInformation;
			} Structs;

			RtlZeroMemory(&Structs, sizeof(Structs));

			if (Flags == MOUSE_MOVE_RELATIVE)
			{
				Structs.LastX = X;

				Structs.LastY = Y;
			}

			if (Flags == MOUSE_MOVE_ABSOLUTE)
			{
				Structs.LastX = X * 65535 / GetSystemMetrics(SM_CXSCREEN);

				Structs.LastY = Y * 65535 / GetSystemMetrics(SM_CYSCREEN);
			}

			return SendDrvMsg(0x1006, &Structs, sizeof(Structs));
		}

		BOOL SimulatedButton(ULONG ButtonFlags)
		{
			/*
				MOUSE_BUTTON_1_DOWN    1
				MOUSE_BUTTON_1_UP      2
				MOUSE_BUTTON_2_DOWN    4
				MOUSE_BUTTON_2_UP      8
				MOUSE_BUTTON_3_DOWN   16
				MOUSE_BUTTON_3_UP     32
				MOUSE_BUTTON_4_DOWN   64
				MOUSE_BUTTON_4_UP    128
				MOUSE_BUTTON_5_DOWN  256
				MOUSE_BUTTON_5_UP    512
				MOUSE_WHEEL         1024
			*/

			static struct
			{
				USHORT UnitId;
				USHORT Flags;
				union {
					ULONG Buttons;
					struct {
						USHORT  ButtonFlags;
						USHORT  ButtonData;
					};
				};
				ULONG RawButtons;
				LONG LastX;
				LONG LastY;
				ULONG ExtraInformation;
			} Structs;

			RtlZeroMemory(&Structs, sizeof(Structs));

			Structs.ButtonFlags = (USHORT)ButtonFlags;

			return SendDrvMsg(0x1006, &Structs, sizeof(Structs));
		}
	}

	namespace Keyboard
	{
		BOOL SimulatedInput(ULONG Key, ULONG Flags)
		{
			/*
				KEY_DOWN    0
				KEY_UP      1
			*/

			static struct
			{
				USHORT UnitId;
				USHORT MakeCode;
				USHORT Flags;
				USHORT Reserved;
				ULONG ExtraInformation;
			} Structs;

			RtlZeroMemory(&Structs, sizeof(Structs));

			Structs.Flags = (USHORT)Flags;

			Structs.MakeCode = (USHORT)MapVirtualKeyA(Key, 0);;

			return SendDrvMsg(0x1007, &Structs, sizeof(Structs));
		}
	}

}


ImFont* ft;
PRender pRenderer = NULL;

bool m_IsWindows7()
{
	RTL_OSVERSIONINFOW verInfo = { 0 };
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	if (hNtdll == NULL) return false;

	pRtlGetVersion = (fnRtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
	if (pRtlGetVersion == NULL) return false;

	verInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	pRtlGetVersion(&verInfo);
	//Dbgprintf("[+] version %d\r\n", verInfo.dwBuildNumber);
	if (verInfo.dwBuildNumber == 7600 || verInfo.dwBuildNumber == 7601)
		return true;
	else
		return false;
}

wstring AsciiToUnicode(const string& str) {
	// 预算-缓冲区中宽字节的长度    
	int unicodeLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
	// 给指向缓冲区的指针变量分配内存    
	wchar_t* pUnicode = (wchar_t*)malloc(sizeof(wchar_t) * unicodeLen);
	// 开始向缓冲区转换字节    
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, pUnicode, unicodeLen);
	wstring ret_str = pUnicode;
	free(pUnicode);
	return ret_str;
}

string UnicodeToUtf8(const wstring& wstr) {
	// 预算-缓冲区中多字节的长度    
	int ansiiLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	// 给指向缓冲区的指针变量分配内存    
	char* pAssii = (char*)malloc(sizeof(char) * ansiiLen);
	// 开始向缓冲区转换字节    
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, pAssii, ansiiLen, nullptr, nullptr);
	string ret_str = pAssii;
	free(pAssii);
	return ret_str;
}

bool EnableDebugPrivilege()
{
	HANDLE hToken;
	LUID sedebugnameValue;
	TOKEN_PRIVILEGES tkp;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		return   FALSE;
	}
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue))
	{
		CloseHandle(hToken);
		return false;
	}
	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Luid = sedebugnameValue;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL))
	{
		CloseHandle(hToken);
		return false;
	}
	return true;
}

HRESULT __stdcall hookD3D10Present(IDXGISwapChain* pSwapChains, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		firstTime = false; 
		if (SUCCEEDED(pSwapChains->GetDevice(__uuidof(ID3D10Device), (void**)&g_pD3D10Device)))
		{
			pSwapChains->GetDevice(__uuidof(ID3D10Device1), (void**)&g_pD3D10Device1);
		}

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui_ImplWin32_Init(GetDesktopWindow());
		ImGui_ImplDX10_Init(g_pD3D10Device, g_pD3D10Device1);
		ft = io.Fonts->AddFontFromFileTTF("C:\windows\fonts\simsun.ttc", 13.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
	}
	//create rendertarget
	if (D3D10RenderTargetView == NULL)
	{
		ID3D10Texture2D* backbuffer = NULL;
		pSwapChains->GetBuffer(0, __uuidof(ID3D10Texture2D), (LPVOID*)&backbuffer);


		g_pD3D10Device->CreateRenderTargetView(backbuffer, NULL, &D3D10RenderTargetView);
		backbuffer->Release();

	}
	else //call before you draw
		g_pD3D10Device->OMSetRenderTargets(1, &D3D10RenderTargetView, NULL);


	ImGui_ImplDX10_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	PUCHAR CurrentMemory = NULL;
	if (pRenderer->drawindex == 0)
		CurrentMemory = pRenderer->pMemory1;
	else if (pRenderer->drawindex == 1)
		CurrentMemory = pRenderer->pMemory2;
	else if (pRenderer->drawindex == 2)
		CurrentMemory = pRenderer->pMemory3;
	if (CurrentMemory)
	{
		int step = 4;
		int Render_len = *(int*)CurrentMemory;
		while (Render_len > step)
		{
			int type = *(int*)(CurrentMemory + step);
			if (type == Draw_Rects)
			{
				PRenderRect pRect = (PRenderRect)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddRect(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color);
				step += sizeof(__Render_Rect);
			}
			else if (type == Draw_Fills)
			{
				PRenderRect pRect = (PRenderRect)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color);
				step += sizeof(__Render_Rect);
			}
			else if (type == Draw_Lines)
			{
				PRenderLine pRect = (PRenderLine)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddLine(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color, pRect->thick);
				step += sizeof(__Render_Line);
			}
			else if (type == Draw_Texts)
			{
				PRenderText pText = (PRenderText)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddText(ft,pText->font,ImVec2(pText->point.x, pText->point.y), pText->Color,
					UnicodeToUtf8(AsciiToUnicode(pText->Text)).c_str());
				step += sizeof(Render_Text);
			}
			else if (type == Draw_Circle)
			{
				PRenderCir pCir = (PRenderCir)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddCircle(ImVec2(pCir->point.x, pCir->point.y), pCir->radius, pCir->Color);
				step += sizeof(Render_Cir);

			}
			else if (type == Draw_FillCircle)
			{
				PRenderCir pCir = (PRenderCir)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(pCir->point.x, pCir->point.y), pCir->radius, pCir->Color);
				step += sizeof(Render_Cir);

			}
			else
			{
				break;
			}
		}
	}
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());

	return phookD3DPresent(pSwapChains, SyncInterval, Flags);
}

void DrawString(float x, float y, ImU32 color, const char* message, ...)
{
	char output[1024] = {};
	va_list args;
	va_start(args, message);
	vsprintf_s(output, message, args);
	va_end(args);

	std::string utf_8_1 = std::string(output);
	std::string utf_8_2 = string_To_UTF8(utf_8_1);

	auto coord = ImVec2(x, y);
	auto coord2 = ImVec2(coord.x + 1, coord.y + 1);
	auto coord_out = ImVec2{ coord.x - 1, coord.y - 1 };

	auto DrawList = ImGui::GetOverlayDrawList();

	DrawList->AddText(coord2, IM_COL32(0, 0, 0, 200), utf_8_2.c_str());
	DrawList->AddText(coord_out, IM_COL32(0, 0, 0, 200), utf_8_2.c_str());
	DrawList->AddText(coord, color, utf_8_2.c_str());
}

void Menustyler()
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.WindowPadding = ImVec2(16, 8);
	style.WindowMinSize = ImVec2(32, 32);
	style.WindowRounding = 0.0f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.ChildRounding = 0.0f;
	style.FramePadding = ImVec2(4, 3);
	style.FrameRounding = 0.0f;
	style.ItemSpacing = ImVec2(4, 3);
	style.ItemInnerSpacing = ImVec2(4, 4);
	style.TouchExtraPadding = ImVec2(0, 0);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 3.0f;
	style.ScrollbarSize = 8.f;
	style.ScrollbarRounding = 0.0f;
	style.GrabMinSize = 1.0f;
	style.GrabRounding = 0.0f;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.DisplayWindowPadding = ImVec2(22, 22);
	style.DisplaySafeAreaPadding = ImVec2(4, 4);
	style.AntiAliasedLines = true;
	//style.AntiAliasedShapes = true;
	style.CurveTessellationTol = 1.25f;
	style.FrameBorderSize = 1.0f;
}

void Menucolor()
{

	ImGuiStyle& style = ImGui::GetStyle();

	style.Colors[ImGuiCol_Text] = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.92f, 0.18f, 0.29f, 0.37f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.92f, 0.18f, 0.29f, 0.75f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.92f, 0.18f, 0.29f, 0.63f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
	style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);

}

VOID DrawMenu()
{
	static bool alpha_preview = true;
	static bool alpha_half_preview = true;
	static bool drag_and_drop = true;
	static bool options_menu = true;
	static bool hdr = false;
	//ImGuiColorEditFlags misc_flags = (hdr ? ImGuiColorEditFlags_HDR : 0) | (drag_and_drop ? 0 : ImGuiColorEditFlags_NoDragDrop) |
	//	(alpha_half_preview ? ImGuiColorEditFlags_AlphaPreviewHalf : (alpha_preview ? ImGuiColorEditFlags_AlphaPreview : 0)) | (options_menu ? 0 : ImGuiColorEditFlags_NoOptions);
	//ImGui::SetColorEditOptions(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_PickerHueWheel);

	//ImGui::Begin(("DWM全系统绘制测试 QQ:1299941557"), NULL, ImGuiWindowFlags_NoCollapse);

	//ImGui::SetWindowSize({ 200,300 });

	DrawString(600.0f, 600.0f, IM_COL32(255, 0, 0, 255), (char*)"DWM 绘制测试");

	//ImGui::End();

	//ImGui::Begin("Hello");
	//float samples[100];
	//for (int n = 0; n < 100; n++)
	//	samples[n] = sinf(n * 0.2f + ImGui::GetTime() * 1.5f);
	//ImGui::PlotLines("Samples", samples, 100);

	ImGui::End();
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(100.0f, 100.0f), ImVec2(500.0f, 500.0f), IM_COL32(0, 255, 255, 255), 1.0f);
	ImGui::GetOverlayDrawList()->AddCircle(ImVec2(400.0f,400.0f),200.0f, IM_COL32(0, 255, 255, 255));

	
}

unsigned int __fastcall CLegacyRenderTarget_GetPresentFlags(struct CLegacyRenderTarget* a1)
{

	if (!g_pD3D11Device)
	{
		ULONG64 pSwapChain = *(ULONG64*)((PUCHAR)a1 + 0xA0);
		m_pSwapChain = *(IDXGISwapChain**)((UINT8*)pSwapChain - 0x118);

		if (m_pSwapChain && (m_pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_pD3D11Device)) == S_OK))
		{
			g_pD3D11Device->GetImmediateContext(&g_pD3D11Context);

			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();

			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			ft = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyhbd.ttc", 50.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
			ImGui_ImplWin32_Init(GetDesktopWindow());
			ImGui_ImplDX11_Init(g_pD3D11Device, g_pD3D11Context);

			//get backbuffer
			ID3D11Texture2D* backbuffer = NULL;
			m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer);

			//create rendertargetview
			g_pD3D11Device->CreateRenderTargetView(backbuffer, NULL, &D3D11RenderTargetView);
			backbuffer->Release();
		}
	}
	ULONG64 pSwapChain = *(ULONG64*)((PUCHAR)a1 + 0xA0);
	if (m_pSwapChain != *(IDXGISwapChain**)((UINT8*)pSwapChain - 0x118))
	{
		g_pD3D11Context->Release();
		g_pD3D11Device->Release();
		D3D11RenderTargetView->Release();
		g_pD3D11Device = NULL;
		return PfnCLegacyRenderTarget_GetPresentFlags(a1);
	}

	g_pD3D11Context->OMSetRenderTargets(1, &D3D11RenderTargetView, NULL);
	////imgui
	//Menustyler();
	//Menucolor();
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();
	PUCHAR CurrentMemory = NULL;
	if (pRenderer->drawindex == 0)
		CurrentMemory = pRenderer->pMemory1;
	else if (pRenderer->drawindex == 1)
		CurrentMemory = pRenderer->pMemory2;
	else if (pRenderer->drawindex == 2)
		CurrentMemory = pRenderer->pMemory3;
	if (CurrentMemory)
	{
		int step = 4;
		int Render_len = *(int*)CurrentMemory;
		while (Render_len > step)
		{
			int type = *(int*)(CurrentMemory + step);
			if (type == Draw_Rects)
			{
				PRenderRect pRect = (PRenderRect)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddRect(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color);
				step += sizeof(__Render_Rect);
			}
			else if (type == Draw_Fills)
			{
				PRenderRect pRect = (PRenderRect)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color);
				step += sizeof(__Render_Rect);
			}
			else if (type == Draw_Lines)
			{
				PRenderLine pRect = (PRenderLine)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddLine(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color, pRect->thick);
				step += sizeof(__Render_Line);
			}
			else if (type == Draw_Texts)
			{
				PRenderText pText = (PRenderText)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddText(ft, pText->font,ImVec2(pText->point.x, pText->point.y), pText->Color,
					UnicodeToUtf8(AsciiToUnicode(pText->Text)).c_str());
				step += sizeof(Render_Text);
			}
			else if (type == Draw_Circle)
			{
				PRenderCir pCir = (PRenderCir)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddCircle(ImVec2(pCir->point.x, pCir->point.y), pCir->radius, pCir->Color);
				step += sizeof(Render_Cir);

			}
			else if (type == Draw_FillCircle)
			{
				PRenderCir pCir = (PRenderCir)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(pCir->point.x, pCir->point.y), pCir->radius, pCir->Color);
				step += sizeof(Render_Cir);

			}
			else
			{
				break;
			}
		}
	}
	DrawMenu();
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	return PfnCLegacyRenderTarget_GetPresentFlags(a1);
}

__int64 __fastcall CDWMSwapChain_PresentInternal(struct CDWMSwapChain* pSwapChain, const struct CRegion* a2, __int64 a3, unsigned int a4, const struct RenderTargetPresentParameters* a5)

{
	//VMStart();
	if (a3 != 1)
	{
		return PfnCDWMSwapChain_PresentInternal(pSwapChain, a2, a3, a4, a5);
	}
	if (!g_pD3D11Device)
	{
		m_pSwapChain = *(IDXGISwapChain**)((UINT8*)pSwapChain + SwapChainOffset);
		if (m_pSwapChain && (m_pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_pD3D11Device)) == S_OK))
		{
			g_pD3D11Device->GetImmediateContext(&g_pD3D11Context);

			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();

			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			ft = io.Fonts->AddFontFromFileTTF("C:\windows\fonts\simsun.ttc", 13.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
			ImGui_ImplWin32_Init(GetDesktopWindow());
			ImGui_ImplDX11_Init(g_pD3D11Device, g_pD3D11Context);

			//get backbuffer
			ID3D11Texture2D* backbuffer = NULL;
			m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer);

			//create rendertargetview
			g_pD3D11Device->CreateRenderTargetView(backbuffer, NULL, &D3D11RenderTargetView);
			backbuffer->Release();
		}
	}

	g_pD3D11Context->OMSetRenderTargets(1, &D3D11RenderTargetView, NULL);
	Menustyler();
	Menucolor();
	////imgui
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();
	PUCHAR CurrentMemory = NULL;
	if (pRenderer->drawindex == 0)
		CurrentMemory = pRenderer->pMemory1;
	else if (pRenderer->drawindex == 1)
		CurrentMemory =pRenderer->pMemory2;
	else if (pRenderer->drawindex == 2)
		CurrentMemory = pRenderer->pMemory3;
	if (CurrentMemory)
	{
		int step = 4;
		int Render_len = *(int*)CurrentMemory;
		while (Render_len > step)
		{
			int type = *(int*)(CurrentMemory + step);
			if (type == Draw_Rects)
			{
				PRenderRect pRect = (PRenderRect)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddRect(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color);
				step += sizeof(__Render_Rect);
			}
			else if (type == Draw_Fills)
			{
				PRenderRect pRect = (PRenderRect)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color);
				step += sizeof(__Render_Rect);
			}
			else if (type == Draw_Lines)
			{
				PRenderLine pRect = (PRenderLine)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddLine(ImVec2(pRect->point.left, pRect->point.top),
					ImVec2(pRect->point.right, pRect->point.bottom),
					pRect->Color, pRect->thick);
				step += sizeof(__Render_Line);
			}
			else if (type == Draw_Texts)
			{
				PRenderText pText = (PRenderText)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddText(ft, pText->font,ImVec2(pText->point.x, pText->point.y), pText->Color,
					UnicodeToUtf8(AsciiToUnicode(pText->Text)).c_str());
				step += sizeof(Render_Text);
			}
			else if (type == Draw_Circle)
			{
				PRenderCir pCir = (PRenderCir)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddCircle(ImVec2(pCir->point.x, pCir->point.y), pCir->radius, pCir->Color);
				step += sizeof(Render_Cir);

			}
			else if (type == Draw_FillCircle)
			{
				PRenderCir pCir = (PRenderCir)(CurrentMemory + step);
				ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(pCir->point.x, pCir->point.y), pCir->radius, pCir->Color);
				step += sizeof(Render_Cir);

			}
			else
			{
				break;
			}
		}
	}
	DrawMenu();
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	//VMEnd();
	return PfnCDWMSwapChain_PresentInternal(pSwapChain, a2, a3, a4, a5);
}

bool BBSearchPattern(IN PUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT PVOID* ppFound)
{

	if (ppFound == NULL || pattern == NULL || base == NULL)
		return false;

	for (ULONG_PTR i = 0; i < size - len; i++)
	{
		BOOLEAN found = TRUE;
		for (ULONG_PTR j = 0; j < len; j++)
		{
			if (pattern[j] != wildcard && pattern[j] != ((PUCHAR)base)[i + j])
			{
				found = FALSE;
				break;
			}
		}

		if (found != FALSE)
		{
			*ppFound = (PUCHAR)base + i;
			return true;
		}
	}

	return false;
}

bool SearchExecutePattern(IN PUCHAR pattern, IN ULONG_PTR len, IN HMODULE mode, OUT PVOID* ppFound)
{
	LPCVOID dwmcore = (LPCVOID)mode;
	MODULEINFO mi;
	GetModuleInformation((HANDLE)-1, (HMODULE)dwmcore, &mi, sizeof(MODULEINFO));
	/* if (BBSearchPattern(pattern, 0xCC, len, dwmcore, mi.SizeOfImage, ppFound))
		 return true;*/

	DWORD size = 0;
	while (true)
	{
		MEMORY_BASIC_INFORMATION basic;
		VirtualQuery((PUCHAR)dwmcore + size, &basic, sizeof(basic));
		if (basic.Protect == PAGE_EXECUTE_READ || basic.Protect == PAGE_EXECUTE_READWRITE || basic.Protect == PAGE_EXECUTE_WRITECOPY)
		{
			if (BBSearchPattern(pattern, 0xCC, len, basic.BaseAddress, basic.RegionSize, ppFound))
				return true;
		}
		size += basic.RegionSize;
		if (size > mi.SizeOfImage)
			break;
	}
	return false;
}

bool SearchReadPattern(IN PUCHAR pattern, IN ULONG_PTR len, IN HMODULE mode, OUT PVOID* ppFound)
{
	LPCVOID dwmcore = (LPCVOID)mode;
	MODULEINFO mi;
	GetModuleInformation((HANDLE)-1, (HMODULE)dwmcore, &mi, sizeof(MODULEINFO));
	/* if (BBSearchPattern(pattern, 0xCC, len, dwmcore, mi.SizeOfImage, ppFound))
		 return true;*/

	DWORD size = 0;
	while (true)
	{
		MEMORY_BASIC_INFORMATION basic;
		VirtualQuery((PUCHAR)dwmcore + size, &basic, sizeof(basic));
		if (basic.Protect == PAGE_READONLY)
		{
			if (BBSearchPattern(pattern, 0xCC, len, basic.BaseAddress, basic.RegionSize, ppFound))
				return true;
		}
		size += basic.RegionSize;
		if (size > mi.SizeOfImage)
			break;
	}
	return false;
}

VOID hook_present(LPVOID pt)
{
	DbgPrint("hook_present");
	PfnCDWMSwapChain_PresentInternal = (fnCDWMSwapChain_PresentInternal) * (DWORD_PTR*)(pt);
	DWORD dwOld;
	VirtualProtect((LPVOID)pt, 8, PAGE_EXECUTE_READWRITE, &dwOld);
	*(DWORD_PTR*)(pt) = (DWORD_PTR)CDWMSwapChain_PresentInternal;
	VirtualProtect((LPVOID)pt, 8, dwOld, &dwOld);
}

VOID hook_present3(LPVOID pt)
{
	DbgPrint("hook_present3");
	PfnCLegacyRenderTarget_GetPresentFlags = (fnCLegacyRenderTarget_GetPresentFlags) * (DWORD_PTR*)(pt);

	DWORD dwOld;
	VirtualProtect((LPVOID)pt, 8, PAGE_EXECUTE_READWRITE, &dwOld);
	*(DWORD_PTR*)(pt) = (DWORD_PTR)CLegacyRenderTarget_GetPresentFlags;
	VirtualProtect((LPVOID)pt, 8, dwOld, &dwOld);
}

DWORD GetSystemVersion()
{
	RTL_OSVERSIONINFOW verInfo = { 0 };
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	if (hNtdll == NULL) return false;

	pRtlGetVersion = (fnRtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
	if (pRtlGetVersion == NULL) return false;

	verInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	if (pRtlGetVersion(&verInfo) == 0)
	{
		return verInfo.dwBuildNumber;
	}
	return 0;
}

BOOL InitializeDWM2()
{
	DbgPrint("InitializeDWM2");
	HMODULE dwmcore = GetModuleHandleA("dwmcore.dll");
	PVOID pFound = NULL;
	DWORD ver = GetSystemVersion();
	DbgPrint("Win ver %d",ver);
	if (ver == 10240)
	{
		UCHAR pattern[] = "\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x81\xEC\x68\x01\x00\x00\x48\x8D\xAC\x24\xF0\x00\x00\x00\x48";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			ULONG64 present = (ULONG64)pFound;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x128;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 10586)
	{
		UCHAR pattern[] = "\x48\x8B\x07\x48\x8B\x98\x08\x01\x00\x00\x48\x8D\x05";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			pFound = (PUCHAR)pFound + 0xA;
			ULONG64 present = 0;
			*((DWORD*)&(present)+1) = (ULONG64)pFound >> 32;
			*((DWORD*)&(present)+0) = (uint32_t)((ULONG64)pFound + *(DWORD*)((PUCHAR)pFound + 3)) + 7;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x128;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 14393)
	{
		UCHAR pattern[] = "\x48\x8B\x07\x48\x8D\x0D\xCC\xCC\x00\x00\x44\x8B\xCD";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			pFound = (PUCHAR)pFound + 0x3;
			ULONG64 present = 0;
			*((DWORD*)&(present)+1) = (ULONG64)pFound >> 32;
			*((DWORD*)&(present)+0) = (uint32_t)((ULONG64)pFound + *(DWORD*)((PUCHAR)pFound + 3)) + 7;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x128;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 15063)
	{
		UCHAR pattern[] = "\x48\x8B\x07\x48\x8D\x0D\xCC\xCC\xCC\xCC\x44\x8B\xCD";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			pFound = (PUCHAR)pFound + 0x3;
			ULONG64 present = 0;
			*((DWORD*)&(present)+1) = (ULONG64)pFound >> 32;
			*((DWORD*)&(present)+0) = (uint32_t)((ULONG64)pFound + *(DWORD*)((PUCHAR)pFound + 3)) + 7;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x130;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 16299)
	{
		UCHAR pattern[] = "\x48\x8B\x03\x48\x8D\x0D\xCC\xCC\xCC\xCC\x44\x8B\xCD";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			pFound = (PUCHAR)pFound + 0x3;
			ULONG64 present = 0;
			*((DWORD*)&(present)+1) = (ULONG64)pFound >> 32;
			*((DWORD*)&(present)+0) = (uint32_t)((ULONG64)pFound + *(DWORD*)((PUCHAR)pFound + 3)) + 7;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x1A0;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 17134)
	{
		UCHAR pattern[] = "\x48\x8B\x07\x48\x8D\x0D\xCC\xCC\x00\x00\x44\x8B\x45";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			pFound = (PUCHAR)pFound + 0x3;
			ULONG64 present = 0;
			*((DWORD*)&(present)+1) = (ULONG64)pFound >> 32;
			*((DWORD*)&(present)+0) = (uint32_t)((ULONG64)pFound + *(DWORD*)((PUCHAR)pFound + 3)) + 7;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x1A8;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 17763)
	{
		UCHAR pattern[] = "\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x68\xFF\xFF\xFF\x48\x81\xEC\xC8\x01\x00\x00";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			ULONG64 present = (ULONG64)pFound;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x1A8;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 18362 || ver == 18363)
	{
		UCHAR pattern[] = "\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x68\xFF\xFF\xFF\x48\x81\xEC\xC8\x01\x00\x00";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			ULONG64 present = (ULONG64)pFound;
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				SwapChainOffset = 0x1A8;
				hook_present(pFound);
				return true;
			}
		}
	}
	else if (ver == 19041 || ver == 19042 || ver == 19043 || ver==19045)
	{
		UCHAR pattern[] = "\x33\xC0\x38\x81\x9A\x48\x00\x00\x74\x02\xC3";
		if (SearchExecutePattern(pattern, sizeof(pattern) - 1, dwmcore, &pFound))
		{
			ULONG64 present = (ULONG64)pFound;
			DbgPrint("present %llX", present);
			UCHAR read_pattern[8] = { 0 };
			memcpy(read_pattern, &present, 8);
			if (SearchReadPattern(read_pattern, sizeof(read_pattern) - 1, dwmcore, &pFound))
			{
				hook_present3(pFound);
				return true;
			}
		}
	}
	return false;
}

DWORD InitHooks(ULONG64 hModule)
{
	DbgPrint("InitHooks");
	//VMStart();
	EnableDebugPrivilege();
	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandleA("dxgi.dll");
		Sleep(100);
	} while (!hDXGIDLL);
	Sleep(100);

	pRenderer = new __Render;
	pRenderer->drawindex = 1;
	pRenderer->pMemory1 = (PUCHAR)VirtualAlloc(NULL, 0x85000, MEM_COMMIT, PAGE_READWRITE);
	pRenderer->pMemory2 = (PUCHAR)VirtualAlloc(NULL, 0x85000, MEM_COMMIT, PAGE_READWRITE);
	pRenderer->pMemory3 = (PUCHAR)VirtualAlloc(NULL, 0x85000, MEM_COMMIT, PAGE_READWRITE);

	DbgPrint("pRenderer");
	if (m_IsWindows7())
	{
		DbgPrint("m_IsWindows7");
		DWORD_PTR point = (DWORD_PTR)hDXGIDLL + 0x5A20;
		phookD3DPresent = (D3DPresentProxy) * (DWORD_PTR*)((DWORD_PTR)hDXGIDLL + 0x5A20);
		DWORD dwOld;
		VirtualProtect((LPVOID)point, 8, PAGE_EXECUTE_READWRITE, &dwOld);
		*(DWORD_PTR*)((DWORD_PTR)hDXGIDLL + 0x5A20) = (DWORD_PTR)hookD3D10Present;
		VirtualProtect((LPVOID)point, 8, dwOld, &dwOld);
	}
	else
	{
		InitializeDWM2();
	}
	//VMEnd();

	//GSInject::DeleteInject();
	
	HANDLE hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, "Kaspersky");
	LPVOID lpBase = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0,0,4096);
	*(ULONG64*)(lpBase) =(ULONG64)pRenderer;
	UnmapViewOfFile(lpBase);
	return NULL;
}

//==========================================================================================================================

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH: // A process is loading the DLL.
		InitHooks((ULONG64)hModule);
		break;

	case DLL_PROCESS_DETACH: // A process unloads the DLL.

		break;
	}
	return TRUE;
}