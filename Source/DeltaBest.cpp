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
#pragma comment(lib, "d3dx11.lib")


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


#ifdef ENABLE_LOG
FILE* out_file = NULL;
#endif

bool g_realtime = false;
bool g_messageDisplayed = false;

HWND g_HWND = NULL;

ID3D11Device* g_tempd3dDevice = NULL;
ID3D11Device* g_d3dDevice = NULL;
IDXGISwapChain1* g_swapchain = NULL;
ID3D11DeviceContext* g_context = NULL;

/*
// DirectX 9 objects, to render some text on screen
LPD3DXFONT g_Font = NULL;
D3DXFONT_DESC FontDesc = {
	DEFAULT_FONT_SIZE, 0, 400, 0, false, DEFAULT_CHARSET,
	OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_PITCH, DEFAULT_FONT_NAME
};
RECT FontPosition, ShadowPosition;
LPD3DXSPRITE bar = NULL;
LPDIRECT3DTEXTURE9 texture = NULL;
*/
//
// DeltaBestPlugin class
//

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

void DeltaBestPlugin::Startup(long version)
{
#ifdef ENABLE_LOG
	WriteLog("--STARTUP--");
#endif 
}

void DeltaBestPlugin::StartSession()
{
#ifdef ENABLE_LOG
	WriteLog("--STARTSESSION--");
#endif 
}

void DeltaBestPlugin::EndSession()
{
#ifdef ENABLE_LOG
	WriteLog("--ENDSESSION--");
	if (out_file) {
		fclose(out_file);
		out_file = NULL;
	}

#endif 
}

void DeltaBestPlugin::Load()
{
#ifdef ENABLE_LOG
	WriteLog("--LOAD--");
#endif 

}

void DeltaBestPlugin::Unload()
{
#ifdef ENABLE_LOG
	WriteLog("--UNLOAD--");

#endif 
}

void DeltaBestPlugin::EnterRealtime()
{
	g_realtime = true;
#ifdef ENABLE_LOG
	WriteLog("---ENTERREALTIME---");
#endif 
	g_swapchain = getDX11SwapChain();
	if(g_d3dDevice)
		WriteLog("Found SwapChain");	
	g_swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_d3dDevice);
	if(g_d3dDevice)
		WriteLog("Found DX11 device");
	g_d3dDevice->GetImmediateContext(&g_context);
	if(g_context)
		WriteLog("Found Context");

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


void DeltaBestPlugin::UpdateTelemetry(const TelemInfoV01 &info){
	return;
}

void DeltaBestPlugin::UpdateScoring(const ScoringInfoV01 &info){
	return;
}

void DeltaBestPlugin::UpdateGraphics(const GraphicsInfoV02 &info){
	if(!g_HWND)
		g_HWND = info.mHWND;
}

IDXGISwapChain1* DeltaBestPlugin::findChain(void* pvReplica, DWORD dwVTable){
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
				if (current == (DWORD*)pvReplica){ WriteLog("Found fake"); continue; } //we don't want to hook the tempSwapChain	
				return ((IDXGISwapChain1*)current);	//If found we hook Present in swapChain		
			}
		}
	}
	return NULL;

}

void DeltaBestPlugin::CreateSearchSwapChain(IDXGISwapChain1** tempSwapChain){
	HRESULT hr;
	D3D_FEATURE_LEVEL FeatureLevels[] ={
					D3D_FEATURE_LEVEL_11_1,
                    D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_10_1,
                    D3D_FEATURE_LEVEL_10_0,
                    D3D_FEATURE_LEVEL_9_3,
                    D3D_FEATURE_LEVEL_9_2,
                    D3D_FEATURE_LEVEL_9_1
   };
    UINT NumFeatureLevels = ARRAYSIZE( FeatureLevels );

    // Call D3D11CreateDevice to ensure that this is a D3D11 device.
    ID3D11DeviceContext* pd3dDeviceContext = NULL;
	hr = D3D11CreateDevice(NULL,                               
		    				D3D_DRIVER_TYPE_HARDWARE,
							( HMODULE )0,
							0,
							FeatureLevels,
							NumFeatureLevels,
							D3D11_SDK_VERSION,
							&g_tempd3dDevice,
							&FeatureLevels[0],
							&pd3dDeviceContext );
	SAFE_RELEASE(pd3dDeviceContext)

	if(g_tempd3dDevice)
		WriteLog("Created device OK");
	else
		WriteLog("Failed to create device");
	IDXGIFactory1 * pIDXGIFactory1 = NULL;
	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pIDXGIFactory1);
	if(pIDXGIFactory1)
		WriteLog("Create Factory OK");
	else
		WriteLog("Failed to create factory1");

	UINT i = 0; 

	IDXGIAdapter1 * pAdapter1; 
	while(pIDXGIFactory1->EnumAdapters1(i, &pAdapter1) != DXGI_ERROR_NOT_FOUND){ 

		IDXGIDevice * pDXGIDevice = NULL;
		if(FAILED(g_tempd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice))){
			WriteLog("Failed to get IDXGIDevice");
			i++;
			continue;
		}else{
			WriteLog("Get IDXGIDevice OK");
		}
		IDXGIAdapter* pAdapter = NULL;
		if(FAILED(pDXGIDevice->GetAdapter(&pAdapter))){
			WriteLog("Failed to get IDXGIAdapter");
			i++;
			continue;
		}else{
			WriteLog("Get IDXGIAdapter OK");
		}

		IDXGIFactory2 * pIDXGIFactory2 = NULL;
		if(FAILED(pAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&pIDXGIFactory2))){
			WriteLog("Failed to get IDXGIFactory2");
			i++;
			continue;
		
		}else{
			WriteLog("Get IDXGIFactory2 OK");
		}
		
		if (!g_HWND){
			WriteLog("Failed to get HWND");
			return;
		}		
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen;
		ZeroMemory(&fullscreen, sizeof(fullscreen));
		fullscreen.RefreshRate.Numerator = 1;
		fullscreen.RefreshRate.Denominator = 60;
		fullscreen.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fullscreen.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		fullscreen.Windowed = (GetWindowLong(g_HWND, GWL_STYLE) & WS_POPUP) != 0 ? FALSE : TRUE;
		

		pIDXGIFactory2->CreateSwapChainForHwnd(g_tempd3dDevice, g_HWND, &swapChainDesc, NULL, NULL, tempSwapChain);

		SAFE_RELEASE(pDXGIDevice)
		SAFE_RELEASE(pIDXGIFactory1)
		SAFE_RELEASE(pIDXGIFactory2)
		SAFE_RELEASE(pAdapter)
		if(*tempSwapChain){
			WriteLog("Created SwapChain OK");
			break;
		}else{
			WriteLog("Failed to create SwapChain");
			i++;
		}
	}
	SAFE_RELEASE(pAdapter1)
}
 
IDXGISwapChain1* DeltaBestPlugin::getDX11SwapChain(){
	IDXGISwapChain1* pSearch = NULL;
 
	WriteLog("Creating fake device");
	CreateSearchSwapChain(&pSearch);
	if(pSearch)
		WriteLog("Successfully created swap chain");
	else
		WriteLog("FAILED TO CREATE SWAP CHAIN");

	IDXGISwapChain1* pSwapChain = NULL;
 
	DWORD pVtable = *(DWORD*)pSearch;
	
	pSwapChain = (IDXGISwapChain1*) findChain( pSearch, pVtable );
	
	SAFE_RELEASE(pSearch)
	SAFE_RELEASE(g_tempd3dDevice)
	return pSwapChain;
 }