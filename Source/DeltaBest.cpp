/*
rF2 Delta Best Plugin

Author: Cosimo Streppone <cosimo@streppone.it>
Date:   April/May 2014
URL:    http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

*/


#include "DeltaBest.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>

#define BEA_ENGINE_STATIC  /* specify the usage of a static version of BeaEngine */
#define BEA_USE_STDCALL    /* specify the usage of a stdcall version of BeaEngine */
#include <BeaEngine.h>
 
#pragma comment(lib, "BeaEngine_s_d.lib")

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")


// plugin information

extern "C" __declspec(dllexport)
	const char * __cdecl GetPluginName()                   { return PLUGIN_NAME; }

extern "C" __declspec(dllexport)
	PluginObjectType __cdecl GetPluginType()               { return PO_INTERNALS; }

extern "C" __declspec(dllexport)
	int __cdecl GetPluginVersion()                         { return 6; }

extern "C" __declspec(dllexport)
	PluginObject * __cdecl CreatePluginObject()            { return((PluginObject *) new DeltaBestPlugin); }

extern "C" __declspec(dllexport)
	void __cdecl DestroyPluginObject(PluginObject *obj)    { delete((DeltaBestPlugin *)obj); }

#define ENABLE_LOG 1

namespace semaphoreDX11{
	#ifdef ENABLE_LOG
	FILE* out_file = NULL;
	#endif

	bool g_realtime = false;
	bool g_messageDisplayed = false;

	HWND g_HWND = NULL;

	ID3D11Device* g_tempd3dDevice = NULL;
	ID3D11Device* g_d3dDevice = NULL;
	IDXGISwapChain* g_swapchain = NULL;
	ID3D11DeviceContext* g_context = NULL;
	
    typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* This, UINT SyncInterval, UINT Flags);
	D3D11PresentHook g_oldPresent;
	struct HookContext{
		BYTE original_code[64];
		SIZE_T dst_ptr;
		BYTE far_jmp[6];
	};
	HookContext* presenthook64;
}

using namespace semaphoreDX11;

void DeltaBestPlugin::WriteLog(const char * const msg)
{
#ifdef ENABLE_LOG
	if (out_file == NULL)
		out_file = fopen(LOG_FILE, "a");

	if (out_file != NULL){
		fprintf(out_file, "%s\n", msg);
		fflush(out_file);
	}
#endif 
}



void DeltaBestPlugin::Load()
{
#ifdef ENABLE_LOG
	WriteLog("--LOAD--");
#endif 

}

HRESULT __stdcall hookedPresent(IDXGISwapChain* This, UINT SyncInterval, UINT Flags){
	ID3D11RenderTargetView* backbuffer;
    FLOAT Green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    g_context->OMGetRenderTargets(1, &backbuffer, NULL);
    g_context->ClearRenderTargetView(backbuffer, Green);

	return g_oldPresent(This, SyncInterval, Flags);
}

void DeltaBestPlugin::EnterRealtime()
{
	g_realtime = true;
#ifdef ENABLE_LOG
	WriteLog("---ENTERREALTIME---");
#endif 
	if(!g_swapchain){
		g_swapchain = getDX11SwapChain();
		if(g_swapchain){
			DWORD_PTR* pSwapChainVtable = (DWORD_PTR*)g_swapchain;
			pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];
			g_oldPresent = (D3D11PresentHook)hookVMT((BYTE*)pSwapChainVtable[8], (BYTE*)hookedPresent);
		}
	}
	if(g_swapchain){
		WriteLog("Found SwapChain");	
		g_swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_d3dDevice);
		if(g_d3dDevice){
			WriteLog("Found DX11 device");
			g_d3dDevice->GetImmediateContext(&g_context);
			if(g_context){
				WriteLog("Found Context");
			}
		}
	}
	if(!g_d3dDevice || !g_swapchain || !g_context)
		WriteLog("Failed to find dx11 resources");
	if(g_oldPresent){
		WriteLog("Present Hook successfull");
		g_swapchain->Present(1, 0);
	}else{
		WriteLog("Unable to hook Present");
	}
	
}

void DeltaBestPlugin::ExitRealtime()
{
	g_realtime = false;
	//g_messageDisplayed = false;
#ifdef ENABLE_LOG
	WriteLog("---EXITREALTIME---");
#endif 
}

bool DeltaBestPlugin::WantsToDisplayMessage( MessageInfoV01 &msgInfo )
{
	if(g_realtime && !g_messageDisplayed){
		
		if(g_d3dDevice && g_swapchain && g_oldPresent){
			sprintf(msgInfo.mText, "Semaphore DX11 OK");
		}else
			sprintf(msgInfo.mText, "Semaphore DX11 FAILURE");

		g_messageDisplayed = true;
		return true;
	}
	return false;
}


 
const unsigned int DisasmLengthCheck(const SIZE_T address, const unsigned int jumplength){
		DISASM disasm;
		memset(&disasm, 0, sizeof(DISASM));
 
		disasm.EIP = (UIntPtr)address;
		disasm.Archi = 0x40;
 
		unsigned int processed = 0;
		while (processed < jumplength)
		{
			const int len = Disasm(&disasm);
			if (len == UNKNOWN_OPCODE)
			{
				++disasm.EIP;
			}
			else
			{
				processed += len;
				disasm.EIP += len;
			}
		}
 
		return processed;
}

//based on: https://www.unknowncheats.me/forum/d3d-tutorials-and-source/88369-universal-d3d11-hook.html by evolution536
void* DeltaBestPlugin::hookVMT(BYTE* src, BYTE* dest){
#ifdef _AMD64_
#define _PTR_MAX_VALUE 0x7FFFFFFEFFFF
MEMORY_BASIC_INFORMATION64 mbi = { 0 };
#else
#define _PTR_MAX_VALUE 0xFFE00000
MEMORY_BASIC_INFORMATION32 mbi = { 0 };
#endif
	//for (SIZE_T addr = (SIZE_T)src; addr > (SIZE_T)src - 0x80000000; addr = (SIZE_T)mbi.BaseAddress - 1){
	for (DWORD* memptr = (DWORD*)0x10000; memptr < (DWORD*)_PTR_MAX_VALUE; memptr = (DWORD*)(mbi.BaseAddress + mbi.RegionSize)){	
		/*if (!VirtualQuery((LPCVOID)memptr, &mbi, sizeof(mbi))){
			    WriteLog("VirtualQuery Failed");
				break;
		}*/
		if(!VirtualQuery(reinterpret_cast<LPCVOID>(memptr),reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&mbi),sizeof(MEMORY_BASIC_INFORMATION))) //Iterate memory by using VirtualQuery
			continue;
 		if(mbi.State == MEM_FREE){
			if (presenthook64 = (HookContext*)VirtualAlloc((LPVOID)mbi.BaseAddress, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)){
					break;
			}
		}
	}
 
	// If allocating a memory page failed, the function failed.
	if (!presenthook64)
		return NULL;

	// Save original code and apply detour. The detour code is:
	// push rax
	// movabs rax, 0xCCCCCCCCCCCCCCCC
	// xchg rax, [rsp]
	// ret
	BYTE detour[] = { 0x50, 0x48, 0xB8, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x48, 0x87, 0x04, 0x24, 0xC3 };
	const unsigned int length = DisasmLengthCheck((SIZE_T)src, PRESENT_JUMP_LENGTH);
	memcpy(presenthook64->original_code, src, length);
	memcpy(&presenthook64->original_code[length], detour, sizeof(detour));
	*(SIZE_T*)&presenthook64->original_code[length + 3] = (SIZE_T)src + length;
 
	// Build a far jump to the destination function.
	*(WORD*)&presenthook64->far_jmp = 0x25FF;
	*(DWORD*)(presenthook64->far_jmp + 2) = (DWORD)((SIZE_T)presenthook64 - (SIZE_T)src + FIELD_OFFSET(HookContext, dst_ptr) - 6);
	presenthook64->dst_ptr = (SIZE_T)dest;
 	
	// Write the hook to the original function.
	DWORD flOld = 0;
	VirtualProtect(src, 6, PAGE_EXECUTE_READWRITE, &flOld);
	memcpy(src, presenthook64->far_jmp, sizeof(presenthook64->far_jmp));
	VirtualProtect(src, 6, flOld, &flOld);
 
	// Return a pointer to the original code.
	return presenthook64->original_code;
}


//based on https://www.unknowncheats.me/forum/d3d-tutorials-source/121840-hook-directx-11-dynamically.html by smallC
void* DeltaBestPlugin::findInstance(void* pvReplica, DWORD dwVTable){
#ifdef _AMD64_
#define _PTR_MAX_VALUE 0x7FFFFFFEFFFF
MEMORY_BASIC_INFORMATION64 mbi = { 0 };
#else
#define _PTR_MAX_VALUE 0xFFE00000
MEMORY_BASIC_INFORMATION32 mbi = { 0 };
#endif
	for (DWORD* memptr = (DWORD*)0x10000; memptr < (DWORD*)_PTR_MAX_VALUE; memptr = (DWORD*)(mbi.BaseAddress + mbi.RegionSize)) //For x64 -> 0x10000 ->  0x7FFFFFFEFFFF
	{
		if (!VirtualQuery(reinterpret_cast<LPCVOID>(memptr),reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&mbi),sizeof(MEMORY_BASIC_INFORMATION))) //Iterate memory by using VirtualQuery
			continue;
 
		if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS || mbi.Protect & PAGE_GUARD) //Filter Regions
			continue;
 
		DWORD* len = (DWORD*)(mbi.BaseAddress + mbi.RegionSize);	 //Do once
		DWORD dwVTableAddress;
		for (DWORD* current = (DWORD*)mbi.BaseAddress; current < len; ++current){								
			__try
 				{
 				    dwVTableAddress  = *(DWORD*)current;
 				}
 				__except (1)
 				{
  				continue;
				}
			
					
			if (dwVTableAddress == dwVTable)
			{
				if (current == (DWORD*)pvReplica){ WriteLog("Found fake"); continue; }	
				return ((void*)current);	
			}
		}
	}
	return NULL;

}

void DeltaBestPlugin::CreateInvisibleWindow(HWND* hwnd){
	WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"FY";
    RegisterClassExW(&wc);
    *hwnd = CreateWindowExW(0,
                             L"FY",
                             L"null",
                             WS_OVERLAPPEDWINDOW,
                             300, 300,
                             640, 480,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr);
}

void DeltaBestPlugin::CreateSearchDevice(ID3D11Device** pDevice, ID3D11DeviceContext** pContext){
	HRESULT hr;
	D3D_FEATURE_LEVEL FeatureLevels[] ={
                    D3D_FEATURE_LEVEL_11_0,
	};
    UINT NumFeatureLevels = ARRAYSIZE( FeatureLevels );

    ID3D11DeviceContext* pd3dDeviceContext = NULL;
	hr = D3D11CreateDevice(NULL,                               
		    				D3D_DRIVER_TYPE_HARDWARE,
							( HMODULE )0,
							0,
							FeatureLevels,
							NumFeatureLevels,
							D3D11_SDK_VERSION,
							pDevice,
							&FeatureLevels[0],
							pContext);

	if(*pDevice)
		WriteLog("Created device OK");
	else{
		WriteLog("Failed to create device");
	}
	
}

void DeltaBestPlugin::CreateSearchSwapChain(ID3D11Device* device, IDXGISwapChain** tempSwapChain, HWND hwnd){
	HRESULT hr;

	IDXGIFactory1 * pIDXGIFactory1 = NULL;
	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pIDXGIFactory1);
	if(pIDXGIFactory1)
		WriteLog("Create Factory OK");
	else{
		WriteLog("Failed to create factory1");
		return;
	}



	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE; //(GetWindowLong(hWnd, GWL_STYLE) & WS_POPUP) != 0 ? FALSE : TRUE;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	
	hr = pIDXGIFactory1->CreateSwapChain(device, &swapChainDesc, tempSwapChain);
		

	if(*tempSwapChain){
		WriteLog("Created SwapChain OK");
	}else{
		WriteLog("Failed to create SwapChain");
		fprintf(out_file, "HR: %d\n", hr);
		fflush(out_file);
	}
	

	SAFE_RELEASE(pIDXGIFactory1)
}
 
IDXGISwapChain* DeltaBestPlugin::getDX11SwapChain(){
	IDXGISwapChain* pSearchSwapChain = NULL;	
	ID3D11Device* pDevice = NULL;
	ID3D11DeviceContext* pContext = NULL;
	
	WriteLog("Creating D3D11 Device");
	CreateSearchDevice(&pDevice, &pContext);
	if(!pDevice)
		return NULL;

	WriteLog("Getting window handle");
	HWND hWnd;
#ifdef DXVK
	CreateInvisibleWindow(&hWnd);
#else
	hWnd = GetForegroundWindow();
#endif // DXVK
	if (hWnd == NULL){
		WriteLog("Failed to get HWND");
		return NULL;
	}

	WriteLog("Creating fake swapChain");
	CreateSearchSwapChain(pDevice, &pSearchSwapChain, hWnd);
	if(pSearchSwapChain)
		WriteLog("Successfully created swap chain");
	else{
		WriteLog("FAILED TO CREATE SWAP CHAIN");
		return NULL;
	}
	IDXGISwapChain* pSwapChain = NULL;
	DWORD pVtable = *(DWORD*)pSearchSwapChain;
	pSwapChain = (IDXGISwapChain*) findInstance( pSearchSwapChain, pVtable );
	if(pSwapChain)
		WriteLog("Found Swapchain");
	else
		WriteLog("Unable to find Swapchain");

	SAFE_RELEASE(pSearchSwapChain)	
	SAFE_RELEASE(pContext)
	SAFE_RELEASE(pDevice)

	return pSwapChain;
 }