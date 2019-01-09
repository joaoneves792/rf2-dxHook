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

void DeltaBestPlugin::EnterRealtime()
{
	g_realtime = true;
#ifdef ENABLE_LOG
	WriteLog("---ENTERREALTIME---");
#endif 
	if(!g_swapchain){
		g_swapchain = getDX11SwapChain();
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
		
		if(g_d3dDevice && g_swapchain){
			sprintf(msgInfo.mText, "Found Swap Chain");
		}else
			sprintf(msgInfo.mText, "NOT FOUND");

		g_messageDisplayed = true;
		return true;
	}
	return false;
}



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